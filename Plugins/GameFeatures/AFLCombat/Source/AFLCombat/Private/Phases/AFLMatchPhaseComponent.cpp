// Copyright C12 AI Gaming. All Rights Reserved.

#include "Phases/AFLMatchPhaseComponent.h"

#include "AFLCombat.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "Online/AFLGameLiftHostSubsystem.h"   // S12-E: seal the roster (DENY_ALL) once the match is live
#include "Teams/AFLMatchmakerDataProvider.h"    // the arrival gate's roster: CountRosterMembers + RosterAbsentees
#include "AFLMatchCancelPolicy.h"               // the abandonment seam -- one watch, both modes
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraExperienceDefinition.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameModes/LyraGameState.h"
#include "HAL/IConsoleManager.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchPhaseComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Warmup_Driver, "AFL.GamePhase.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Playing_Driver, "AFL.GamePhase.Playing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_PostGame_Driver, "AFL.GamePhase.PostGame");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_ExtractionWindow_Driver, "AFL.GamePhase.Playing.ExtractionWindow");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowOpen_Driver, "Event.Extraction.WindowOpen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowClosed_Driver, "Event.Extraction.WindowClosed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Match_Ended_Driver, "Event.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Warmup_Driver, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Ended_Driver, "State.Match.Ended");
// Clean-health MODE gate (IRONICS_GAME_MODES_SSOT sec.4). Applied to every combatant ASC at Warmup in
// Pro Mod / Melee (experiences that drop the AFLDismember game feature); read off the TARGET by
// UAFLDamageExecCalc, which then forces bIsZoneRouted=false -> conventional single body-health.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Mode_NoDismember_Driver, "State.Mode.NoDismember");

// Match spine durations (S9) + the window cadence/duration (extraction cycle 1). Cvars so the
// harness compresses without a rebuild (afl.Match.Test.Run uses Warmup 3 / Active 10 / window 4/3).
static TAutoConsoleVariable<float> CVarAFLMatchWarmupDuration(
	TEXT("afl.Match.WarmupDuration"),
	30.0f,
	TEXT("Seconds of pre-match Warmup (fire/movement frozen) before chaining to Playing."));

static TAutoConsoleVariable<float> CVarAFLMatchActiveDuration(
	TEXT("afl.Match.ActiveDuration"),
	480.0f,
	TEXT("Seconds the match stays in Playing before PostGame (terminal)."));

static TAutoConsoleVariable<float> CVarAFLExtractWindowPeriod(
	TEXT("afl.Extract.WindowPeriod"),
	150.0f,
	TEXT("Seconds between extraction-window OPENINGS (read per schedule -- a change takes at the next boundary)."));

static TAutoConsoleVariable<float> CVarAFLExtractWindowDuration(
	TEXT("afl.Extract.WindowDuration"),
	60.0f,
	TEXT("Seconds an extraction window stays OPEN (read by the driver when it opens a window)."));

namespace
{
	// The BP phase shells (children of ULyraGamePhaseAbility -- the C++ subclass boundary). Authored
	// in Stage 2; resolved by soft path so a missing asset degrades to a logged no-op, not a crash.
	const TCHAR* WarmupPhasePath  = TEXT("/Game/BagMan/Phases/BP_AFL_Phase_Warmup.BP_AFL_Phase_Warmup_C");
	const TCHAR* PlayingPhasePath = TEXT("/Game/BagMan/Phases/BP_AFL_Phase_Playing.BP_AFL_Phase_Playing_C");
	const TCHAR* WindowPhasePath  = TEXT("/Game/BagMan/Phases/BP_AFL_Phase_ExtractionWindow.BP_AFL_Phase_ExtractionWindow_C");
	const TCHAR* PostGamePhasePath= TEXT("/Game/BagMan/Phases/BP_AFL_Phase_PostGame.BP_AFL_Phase_PostGame_C");

	// THE LYRA PHASE WALL: every C++ entry point to drive a phase from outside LyraGame is blocked --
	// StartPhase/WhenPhase* are public but NOT LYRAGAME_API-exported (link error), and the K2_* UFUNCTION
	// equivalents are exported but PROTECTED (access error). The ONLY reachable surface is IsPhaseActive
	// (public + exported). So we invoke the protected K2_ UFUNCTIONs REFLECTIVELY via ProcessEvent --
	// the same mechanism a Blueprint node compiles to; it ignores both C++ access control and the export
	// boundary. The param structs below mirror each UFUNCTION's parameter layout exactly.
	struct FK2StartPhaseParams
	{
		TSubclassOf<ULyraGamePhaseAbility> Phase;
		FLyraGamePhaseDynamicDelegate PhaseEnded;
	};
	struct FK2WhenPhaseParams
	{
		FGameplayTag PhaseTag;
		EPhaseTagMatchType MatchType = EPhaseTagMatchType::ExactMatch;
		FLyraGamePhaseTagDynamicDelegate WhenPhase;
	};

	void ReflectStartPhase(ULyraGamePhaseSubsystem* Sub, TSubclassOf<ULyraGamePhaseAbility> Phase)
	{
		if (!Sub || !Phase) { return; }
		if (UFunction* Fn = Sub->FindFunction(TEXT("K2_StartPhase")))
		{
			FK2StartPhaseParams Params;
			Params.Phase = Phase;
			Sub->ProcessEvent(Fn, &Params);
		}
	}

	// Even IsPhaseActive (public + BlueprintCallable) does not LINK from outside LyraGame -- the
	// subsystem class carries no LYRAGAME_API, so NONE of its member symbols cross the DLL boundary,
	// UFUNCTION or not. So it too goes through reflection. Param struct = (FGameplayTag, bool ReturnValue).
	// (The shared implementation lives in the UAFLMatchPhaseComponent::IsPhaseActiveReflected static so
	// the harness TU can call it too.)
	struct FK2IsPhaseActiveParams
	{
		FGameplayTag PhaseTag;
		bool ReturnValue = false;
	};
}

bool UAFLMatchPhaseComponent::IsPhaseActiveReflected(const UWorld* World, const FGameplayTag& PhaseTag)
{
	const ULyraGamePhaseSubsystem* Sub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(World);
	if (!Sub) { return false; }
	if (UFunction* Fn = Sub->FindFunction(TEXT("IsPhaseActive")))
	{
		FK2IsPhaseActiveParams Params;
		Params.PhaseTag = PhaseTag;
		const_cast<ULyraGamePhaseSubsystem*>(Sub)->ProcessEvent(Fn, &Params);
		return Params.ReturnValue;
	}
	return false;
}


UAFLMatchPhaseComponent::UAFLMatchPhaseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	// Soft-resolve the BP shells (StaticLoadClass at CDO time is fine for a server component). A BP
	// child of this component may override these in the details panel.
	WarmupPhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, WarmupPhasePath);
	PlayingPhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, PlayingPhasePath);
	WindowPhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, WindowPhasePath);
	PostGamePhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, PostGamePhasePath);
}

void UAFLMatchPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	AGameStateBase* GS = GetGameStateChecked<AGameStateBase>();
	if (!GS->HasAuthority())
	{
		return; // pure server driver (the experience row is server-flagged too).
	}

	// Clean-health MODE gate (IRONICS_GAME_MODES_SSOT sec.4): defer the mode read to the experience-loaded
	// delegate. This component's BeginPlay runs DURING AFLCombat game-feature activation -- before the
	// experience manager reaches LoadState::Loaded -- so reading the experience here asserts. The delegate
	// fires once loaded (immediately if already), by which point PlayerState/dummy ASCs exist.
	if (ULyraExperienceManagerComponent* ExpMgr = GS->FindComponentByClass<ULyraExperienceManagerComponent>())
	{
		ExpMgr->CallOrRegister_OnExperienceLoaded(
			FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &UAFLMatchPhaseComponent::OnExperienceLoaded_ApplyModeTags));
	}

	ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld());
	if (!PhaseSub || !WarmupPhaseClass || !PlayingPhaseClass)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_PHASE: driver could not start -- subsystem %s / Warmup %s / Playing %s (BP shells missing?)."),
			PhaseSub ? TEXT("ok") : TEXT("MISSING"), WarmupPhaseClass ? TEXT("ok") : TEXT("MISSING"), PlayingPhaseClass ? TEXT("ok") : TEXT("MISSING"));
		return;
	}

	StartSpineFromWarmup();
}

void UAFLMatchPhaseComponent::StartSpineFromWarmup()
{
	// THE SPINE STARTS AT WARMUP. Start the phase, block fire/movement abilities on all pawns, arm the
	// warmup->playing chain timer (reads the cvar FRESH so RestartMatch can compress it).
	bMatchEnded = false;

	// ORDERING INVARIANT: START THE PHASE BEFORE SWEEPING. The join handler answers "is warmup live?"
	// with IsPhaseActiveReflected, so the phase must already be true when the sweep (and anyone joining
	// after it) reads it. Correct order here; EnterPlaying had it backwards -- see the note there.
	// IF ANY PHASE TRANSITION EVER BECOMES LATENT OR TIMER-DRIVEN, REDO THIS ANALYSIS: it holds only
	// because StartPhaseByClass is synchronous (ProcessEvent, not a latent node), so no join can
	// interleave between the state change and the sweep.
	StartPhaseByClass(WarmupPhaseClass, TAG_AFL_GamePhase_Warmup_Driver);
	const int32 Covered = SetMatchTagOnAllPawns(TAG_State_Match_Warmup_Driver, /*bPresent=*/true);

	// THE HUMANLESS WATCH RUNS FOR THE WHOLE SPINE, not just the playing phase -- it gates itself on
	// IsMatchLiveForAbandonment rather than on a phase, so it is correct to arm it once here and leave it.
	// A restart re-enters through this function and SetTimer replaces rather than stacks.
	HumanlessSeconds = 0.f;
	GetWorld()->GetTimerManager().SetTimer(AbandonmentTimer, this,
		&UAFLMatchPhaseComponent::TickAbandonmentWatch, AbandonmentPollSeconds, /*loop=*/true);

	const float Warmup = FMath::Max(0.1f, CVarAFLMatchWarmupDuration.GetValueOnGameThread());
	GetWorld()->GetTimerManager().SetTimer(WarmupTimer, this, &UAFLMatchPhaseComponent::EnterPlaying, Warmup, /*loop=*/false);
	// The COUNT is the proof. This sweep runs during game-feature activation, before any player has
	// joined -- so on a 6-player match it legitimately reads 1 (the level-placed target dummy), and the
	// remaining five are covered by ApplyJoinStateToPlayer as they arrive. A count of 1 with NO join
	// coverage was the original bug, invisible because the log said only "frozen".
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_PHASE: WARMUP started (%.0fs; fire + movement ABILITIES blocked, base locomotion unaffected) -- %d ASC(s) swept; joiners covered on arrival."),
		Warmup, Covered);
}

float UAFLMatchPhaseComponent::GetWarmupSecondsRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World || !WarmupTimer.IsValid())
	{
		return -1.f;
	}
	const float Remaining = World->GetTimerManager().GetTimerRemaining(WarmupTimer);
	return (Remaining > 0.f) ? Remaining : -1.f;   // -1 == not running, so callers never show a stale 0
}

void UAFLMatchPhaseComponent::RestartMatch()
{
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		return;
	}
	// Tear down: clear all timers, remove both match tags from everyone, end any live phase by
	// starting Warmup fresh (which cancels non-ancestor siblings -- but PostGame is a sibling of
	// Warmup too, so starting Warmup cancels PostGame/Playing/.ExtractionWindow cleanly).
	if (UWorld* World = GetWorld())
	{
		// The gate timer is cleared with the rest, and bAwaitingMatchStart with it. A restart issued WHILE the
		// gate is holding would otherwise leave a live gate timer alongside the fresh WarmupTimer -- and if the
		// gate released in between, EnterPlaying would run twice (two ActiveTimers, two phase starts).
		World->GetTimerManager().ClearTimer(MatchStartGateTimer);
		World->GetTimerManager().ClearTimer(WarmupTimer);
		World->GetTimerManager().ClearTimer(ActiveTimer);
		World->GetTimerManager().ClearTimer(WindowOpenTimer);
		World->GetTimerManager().ClearTimer(WindowDurationTimer);
	}
	bAwaitingMatchStart = false;
	FirstArrivalHeldSeconds = -1.f;   // a restart re-opens the arrival window; a stale stamp would expire it instantly
	SetMatchTagOnAllPawns(TAG_State_Match_Warmup_Driver, /*bPresent=*/false);
	SetMatchTagOnAllPawns(TAG_State_Match_Ended_Driver,  /*bPresent=*/false);
	bWindowOpen = false;
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: RESTART -- spine reset, re-entering Warmup with fresh cvars."));
	StartSpineFromWarmup();
}

bool UAFLMatchPhaseComponent::IsUnderGameLift() const
{
	// IsSdkReady() is the ONLY predicate separating a GameLift-hosted server from PIE or a -Server launch
	// without -GameLift. It must stay in front of every payload/presence test.
	const UAFLGameLiftHostSubsystem* GameLift = UAFLGameLiftHostSubsystem::Get(this);
	return GameLift && GameLift->IsSdkReady();
}

bool UAFLMatchPhaseComponent::HasPayload() const
{
	const UAFLGameLiftHostSubsystem* GameLift = UAFLGameLiftHostSubsystem::Get(this);
	return GameLift && GameLift->HasGameSessionData();
}

int32 UAFLMatchPhaseComponent::CountHumanParticipants() const
{
	// Mirrors UAFLRoundManagerComponent::CountHumanParticipants. A disconnected player's PlayerState leaves
	// PlayerArray (AGameModeBase::Logout), so this needs no separate connection bookkeeping.
	const AGameStateBase* GS = GetGameStateChecked<AGameStateBase>();
	int32 Count = 0;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		if (PS && !PS->IsABot() && !PS->IsOnlyASpectator())
		{
			++Count;
		}
	}
	return Count;
}

IAFLMatchCancelPolicy* UAFLMatchPhaseComponent::FindCancelPolicy() const
{
	// Same lookup AAFLGameMode uses for IAFLRoundRestartPolicy: iterate the GameState's components and take the
	// first implementer. Null is ordinary, not an error -- the mode GameFeature may not have loaded yet.
	if (const AGameStateBase* GS = GetGameStateChecked<AGameStateBase>())
	{
		TArray<UActorComponent*> Comps;
		GS->GetComponents(Comps);
		for (UActorComponent* Comp : Comps)
		{
			if (IAFLMatchCancelPolicy* Policy = Cast<IAFLMatchCancelPolicy>(Comp))
			{
				return Policy;
			}
		}
	}
	return nullptr;
}

void UAFLMatchPhaseComponent::TickAbandonmentWatch()
{
	IAFLMatchCancelPolicy* Policy = FindCancelPolicy();

	// Before a match starts nothing is staked and no session is held on anyone's behalf; after it concludes
	// there is nothing left to cancel. Both RESET rather than merely skip, so a stale accumulator cannot
	// survive into the next match on a reused component.
	if (!Policy || !Policy->IsMatchLiveForAbandonment() || AbandonmentGraceSeconds <= 0.f)
	{
		HumanlessSeconds = 0.f;
		return;
	}

	if (CountHumanParticipants() > 0)
	{
		// ARMS HERE, ONCE, AND NEVER DISARMS. From this moment the players WERE here, so a later emptiness
		// genuinely means they left.
		if (!bAnyHumanEverJoined)
		{
			bAnyHumanEverJoined = true;
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: first human participant present -- abandonment watch ARMED."));
		}
		if (HumanlessSeconds > 0.f)
		{
			UE_LOG(LogAFLCombat, Log,
				TEXT("AFL_PHASE: a human participant is present again after %.0fs -- abandonment watch reset."), HumanlessSeconds);
			HumanlessSeconds = 0.f;
		}
		return;
	}

	// ── NOBODY HAS EVER ARRIVED: NOT AN ABANDONMENT, ON ANY PATH ────────────────────────────────────────
	// You cannot leave a match you never joined. Under GameLift the arrival gate makes this unreachable -- a
	// match cannot start without a human. It guards the paths with no such gate: PIE, listen server, and the
	// launch-line dedicated runs used to test without a backend, where a humanless match once cancelled itself
	// at the grace and the operator then joined a match that had been dead for 77 seconds.
	if (!bAnyHumanEverJoined)
	{
		return;
	}

	const bool bFirstEmptyTick = (HumanlessSeconds <= 0.f);
	HumanlessSeconds += AbandonmentPollSeconds;
	if (bFirstEmptyTick)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_PHASE: NO HUMAN PARTICIPANTS REMAIN -- cancelling in %.0fs unless one returns. (A SINGLE "
			     "leaver forfeits and does not reach here; this is the empty-field case.)"),
			AbandonmentGraceSeconds);
	}

	if (HumanlessSeconds >= AbandonmentGraceSeconds)
	{
		HumanlessSeconds = 0.f;
		Policy->ServerCancelAbandoned();
	}
}

EAFLStartGateDecision UAFLMatchPhaseComponent::EvaluateStartGate(
	bool  bHasPayload,
	int32 RosterHumanCount,
	int32 PresentHumanCount,
	int32 AbsentRosteredCount,
	float HeldSeconds,
	float SecondsSinceFirstArrival,
	float ArrivalGraceSeconds,
	float NoShowDeadlineSeconds)
{
	const bool bAtNoShowBound = HeldSeconds >= NoShowDeadlineSeconds;

	// ── NOTHING CAN START WITHOUT A PAYLOAD ──────────────────────────────────────────────────────────────
	// Unchanged, and it does not release at the payload deadline: under GameLift a server with no game session
	// refuses every connection at PreLogin, so nobody can ever join it and releasing would start an empty match.
	if (!bHasPayload)
	{
		return bAtNoShowBound ? EAFLStartGateDecision::CancelNoShow : EAFLStartGateDecision::Hold;
	}

	// ── NOBODY IS HERE ───────────────────────────────────────────────────────────────────────────────────
	// The no-show bound's ORIGINAL ending, and the only one it used to have.
	if (PresentHumanCount <= 0)
	{
		return bAtNoShowBound ? EAFLStartGateDecision::CancelNoShow : EAFLStartGateDecision::Hold;
	}

	// ── NO ROSTER TO CHECK AGAINST ───────────────────────────────────────────────────────────────────────
	// INDEX_NONE, never 0: CountRosterMembers refuses to collapse "no roster" into "a roster of nobody". PIE
	// and offline live here, and so does a payload that will not parse. This is the pre-2026-08-14 rule
	// verbatim -- payload present and at least one human -- and it is REQUIRED, not a courtesy: a match with no
	// roster has no arrival to wait for, and holding would hang it until the no-show bound killed it.
	if (RosterHumanCount < 0)
	{
		return EAFLStartGateDecision::OpenNoRoster;
	}

	// ── EVERYONE THE MATCHMAKER PROMISED IS STANDING HERE ────────────────────────────────────────────────
	if (AbsentRosteredCount <= 0)
	{
		return EAFLStartGateDecision::OpenAllPresent;
	}

	// ── SOMEONE IS STILL MISSING ─────────────────────────────────────────────────────────────────────────
	// The grace runs from the FIRST arrival, so it cannot expire on an empty field -- negative means nobody
	// has arrived and the PresentHumanCount check above has already handled that.
	if (SecondsSinceFirstArrival >= 0.f && SecondsSinceFirstArrival >= ArrivalGraceSeconds)
	{
		return EAFLStartGateDecision::OpenGraceExpired;
	}

	// THE BOUND'S SECOND ENDING. Reachable only when somebody arrived so late that the no-show bound beats
	// their own grace; with a 30s grace and a 600s bound that means an arrival past 570s. One human present is
	// a match worth starting, so it starts -- the alternative is cancelling a match somebody is sitting in.
	if (bAtNoShowBound)
	{
		return EAFLStartGateDecision::OpenGraceExpired;
	}

	return EAFLStartGateDecision::Hold;
}

EAFLStartGateDecision UAFLMatchPhaseComponent::DecideStartGate(TArray<FString>* OutAbsentees) const
{
	// THE ROSTER IS READ THROUGH THE PROVIDER, NEVER PARSED HERE. Four readers already share those statics
	// (bot fill's count, the pre-seat's by-team tally, the field size, and now this); a fifth ad-hoc
	// FJsonSerializer call in a GameFeature module is exactly how the five would drift apart.
	const FString Payload = UAFLMatchmakerDataProvider::ResolveAuthoritativeMatchmakerData(this);
	const int32 RosterHumans = UAFLMatchmakerDataProvider::CountRosterMembers(Payload);

	TArray<FString> Absent;
	if (RosterHumans > 0)
	{
		Absent = UAFLMatchmakerDataProvider::RosterAbsentees(this, Payload);
	}
	if (OutAbsentees)
	{
		*OutAbsentees = Absent;
	}

	const float SinceFirstArrival = (FirstArrivalHeldSeconds >= 0.f)
		? (MatchStartHeldSeconds - FirstArrivalHeldSeconds)
		: -1.f;

	return EvaluateStartGate(HasPayload(), RosterHumans, CountHumanParticipants(), Absent.Num(),
		MatchStartHeldSeconds, SinceFirstArrival, ArrivalGraceSeconds, NoShowDeadlineSeconds);
}

bool UAFLMatchPhaseComponent::IsReadyToStartMatch() const
{
	// Kept as a bool so EnterPlaying's `IsUnderGameLift() && !IsReadyToStartMatch()` reads unchanged. The
	// distinction between the three ways of opening only matters to the gate tick, which logs them apart.
	const EAFLStartGateDecision Decision = DecideStartGate(nullptr);
	return Decision == EAFLStartGateDecision::OpenAllPresent
		|| Decision == EAFLStartGateDecision::OpenGraceExpired
		|| Decision == EAFLStartGateDecision::OpenNoRoster;
}

void UAFLMatchPhaseComponent::TickMatchStartGate()
{
	if (!bAwaitingMatchStart || bMatchEnded)
	{
		GetWorld()->GetTimerManager().ClearTimer(MatchStartGateTimer);
		return;
	}

	// STAMP THE FIRST ARRIVAL BEFORE DECIDING. The grace measures from the first human, so the frame they
	// appear must already know it -- otherwise the first poll after their arrival reads "nobody has come yet"
	// and the grace starts one tick late, every time.
	if (FirstArrivalHeldSeconds < 0.f && CountHumanParticipants() > 0)
	{
		FirstArrivalHeldSeconds = MatchStartHeldSeconds;
	}

	TArray<FString> Absentees;
	const EAFLStartGateDecision Decision = DecideStartGate(&Absentees);

	switch (Decision)
	{
	case EAFLStartGateDecision::OpenAllPresent:
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_PHASE: Warmup->Playing RELEASED after %.0fs -- all %d rostered player(s) present."),
			MatchStartHeldSeconds, CountHumanParticipants());
		EnterPlaying();   // clears bAwaitingMatchStart itself, and now passes the gate
		return;

	case EAFLStartGateDecision::OpenNoRoster:
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_PHASE: Warmup->Playing RELEASED after %.0fs -- payload present, %d player(s) on the field, "
			     "NO USABLE ROSTER to check against (PIE/offline/unparseable payload)."),
			MatchStartHeldSeconds, CountHumanParticipants());
		EnterPlaying();
		return;

	case EAFLStartGateDecision::OpenGraceExpired:
		// ⚠ THE LINE THAT SEPARATES A BAD CONNECTION FROM A GATE REGRESSION, so it names names. If this fires
		// with a full lobby's worth of absentees, the gate did not regress -- the roster did. If it fires
		// every match with exactly one straggler, the grace is too short. Neither reading is available from
		// "started short-handed".
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_PHASE: STARTING SHORT-HANDED after %.0fs -- the %.0fs arrival grace expired with %d "
			     "rostered player(s) MISSING: [%s]. %d present. They were promised by the matchmaker and never "
			     "arrived; they are not participants in this match."),
			MatchStartHeldSeconds, ArrivalGraceSeconds, Absentees.Num(), *FString::Join(Absentees, TEXT(", ")),
			CountHumanParticipants());
		EnterPlaying();
		return;

	case EAFLStartGateDecision::CancelNoShow:
		// Falls through to the no-show block below, which owns the terminal log and EnterPostGame.
		break;

	case EAFLStartGateDecision::Hold:
	default:
		break;
	}

	MatchStartHeldSeconds += MatchStartGatePollSeconds;

	// The payload deadline is the GameLift queue timeout: past it the placement was cancelled at source, so no
	// payload is coming. Logged ONCE, and it does NOT release the gate -- under GameLift a server with no
	// payload has no game session, so /claim-session can mint nothing and AFLGameMode refuses every connection
	// at PreLogin. Nobody can ever join it, and releasing would only start an empty match.
	if (!bPayloadTimeoutLogged && MatchStartHeldSeconds >= PayloadWaitSeconds)
	{
		bPayloadTimeoutLogged = true;
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_PHASE: no payload after %.0fs (the GameLift queue timeout) -- the placement was cancelled at "
			     "source. No client can connect without it; holding until the NO SHOW bound."),
			PayloadWaitSeconds);
	}

	// THE TERMINATION GUARANTEE. Nobody arrived; conclude so the process stops holding a session and the
	// compute can be recycled. The match never started, so nothing was staked and there is nothing to refund.
	if (MatchStartHeldSeconds >= NoShowDeadlineSeconds)
	{
		GetWorld()->GetTimerManager().ClearTimer(MatchStartGateTimer);
		bAwaitingMatchStart = false;
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_PHASE: NO SHOW after %.0fs -- payload=%s, no placed player ever arrived. Concluding to "
			     "PostGame; the match never started."),
			MatchStartHeldSeconds, HasPayload() ? TEXT("yes") : TEXT("NO"));
		EnterPostGame();
	}
}

void UAFLMatchPhaseComponent::EnterPlaying()
{
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority() || bMatchEnded)
	{
		return;
	}

	// ══ THE MATCH-START GATE ═════════════════════════════════════════════════════════════════════════════
	//
	// Under GameLift, hold this transition until the placement payload has arrived AND at least one placed
	// player is actually on the field. Everything below inherits the hold.
	//
	// ⚠ IT IS GATED HERE, AND ONLY HERE, BECAUSE THIS ONE FUNCTION HAS NINE DOWNSTREAM CONSUMERS. Gating any
	// single consumer leaves the other eight ungated -- which is not a hypothetical: it was tried three times
	// on 2026-08-13 and produced three separate dead-match defects, each found by a different live run.
	//
	//   DIRECT, in the body below:
	//     1. StartPhaseByClass(Playing)      fires every phase observer (see 7-9)
	//     2. SetMatchTagOnAllPawns(Warmup,0) lifts the fire/movement block
	//     3. SnapshotMatchStartWatts()
	//     4. ScheduleNextWindow()            arms the extraction-window cadence
	//     5. ActiveTimer -> EnterPostGame    the 480s match-duration clock
	//     6. SetAcceptingPlayers(false)      SEALS THE ROSTER -- and this one bit the gate that lived in
	//                                        ServerStartMatch: the match waited for players while this
	//                                        locked them out, so /claim-session refused every arrival with
	//                                        InvalidGameSessionStatusException. A deadlock only the no-show
	//                                        bound could end.
	//   OBSERVERS of AFL.GamePhase.Playing, fired by (1):
	//     7. UAFLRoundManagerComponent   ServerStartMatch -> rounds tick, ESCROW is taken (once, here)
	//     8. UAFLBattleRoyaleComponent   its OWN ServerStartMatch, same reflective binding
	//     9. AAFLExtractionZone          observes the CHILD tag Playing.ExtractionWindow, not this one
	//
	// ADDING A TENTH? It inherits this gate for free, and that is the point of the altitude. Do not add a
	// second gate on the same condition further down -- two gates at two altitudes is exactly how (6) was
	// missed.
	//
	// GAMELIFT ONLY. PIE has no expected roster and its listen host is the only human; gating there would
	// hang every local session on its first match.
	if (IsUnderGameLift() && !IsReadyToStartMatch())
	{
		if (!bAwaitingMatchStart)
		{
			bAwaitingMatchStart = true;
			MatchStartHeldSeconds = 0.f;
			bPayloadTimeoutLogged = false;
			// The grace has not started: it runs from the first ARRIVAL, not from the start of the hold.
			FirstArrivalHeldSeconds = -1.f;
			UE_LOG(LogAFLCombat, Warning,
				TEXT("AFL_PHASE: HOLDING Warmup->Playing -- payload=%s players=%d. Nothing downstream runs: no "
				     "rounds, no escrow, no roster seal. Concluding as NO SHOW in %.0fs."),
				HasPayload() ? TEXT("yes") : TEXT("NO"), CountHumanParticipants(), NoShowDeadlineSeconds);
		}
		// Re-check on a cheap repeating timer rather than ticking: this component does not tick, and the
		// transition is already timer-driven, so a timer keeps the mechanism uniform.
		GetWorld()->GetTimerManager().SetTimer(MatchStartGateTimer, this,
			&UAFLMatchPhaseComponent::TickMatchStartGate, MatchStartGatePollSeconds, /*loop=*/true);
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(MatchStartGateTimer);
	bAwaitingMatchStart = false;
	// Lift the warmup block, go active (auto-cancels Warmup), snapshot Watts, arm the cadence + the
	// match-duration timer.
	//
	// ORDERING INVARIANT -- THIS ORDER IS LOAD-BEARING AND WAS PREVIOUSLY REVERSED. Starting Playing is
	// what cancels the Warmup phase, so it must happen BEFORE the sweep: otherwise there is a window in
	// which the sweep has already cleared the tag while IsPhaseActiveReflected(Warmup) still answers
	// true, and a joiner reading that would be tagged into a phase that just ended -- with nothing left
	// to untag it. PERMANENTLY FROZEN. Idempotent writes do not protect against this; ordering does.
	// IF ANY PHASE TRANSITION EVER BECOMES LATENT OR TIMER-DRIVEN, REDO THIS ANALYSIS.
	StartPhaseByClass(PlayingPhaseClass, TAG_AFL_GamePhase_Playing_Driver);
	const int32 Lifted = SetMatchTagOnAllPawns(TAG_State_Match_Warmup_Driver, /*bPresent=*/false);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: warmup block lifted -- %d ASC(s) swept."), Lifted);
	SnapshotMatchStartWatts();
	ScheduleNextWindow();
	const float Active = FMath::Max(0.1f, CVarAFLMatchActiveDuration.GetValueOnGameThread());
	GetWorld()->GetTimerManager().SetTimer(ActiveTimer, this, &UAFLMatchPhaseComponent::EnterPostGame, Active, /*loop=*/false);
	// S12-E: SEAL THE ROSTER. From here the match is live and, in a staked tier, stakes are escrowed against
	// exactly the players present. A late placement would put someone into a match they never paid into and
	// that settlement will not pay out to. The PreLogin identity gate already refuses anyone whose session is
	// not on this roster; this stops GameLift minting a session for this match at all.
	//
	// This does NOT close the door on a dropped player returning: the policy blocks CREATION of new player
	// sessions, while a reconnect re-presents the ACTIVE session the player already holds (verified against
	// live GameLift). So it can stay closed for the whole match. Safe no-op off GameLift.
	if (const UAFLGameLiftHostSubsystem* GameLift = UAFLGameLiftHostSubsystem::Get(this))
	{
		GameLift->SetAcceptingPlayers(false);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: PLAYING started (%.0fs; window cadence armed, Watts snapshotted)."), Active);
}

void UAFLMatchPhaseComponent::EnterPostGame()
{
	// The 480s match-duration ActiveTimer fired (the TIME-based match-end). In round-based mode the round
	// FSM is the SOLE match-end authority (set via SetExternalMatchEndAuthority at match-start) -- a clock
	// ending a best-of mid-series is illogical -- so this time-conclude no-ops. The extraction-WINDOW
	// cadence (separate WindowOpen/WindowDuration timers) is untouched; ONLY the match-END is suppressed.
	if (bExternalMatchEndAuthority)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: 480s time-conclude SUPPRESSED -- round FSM is the match-end authority (windows continue)."));
		return;
	}
	ConcludeMatch();
}

void UAFLMatchPhaseComponent::ConcludeMatch()
{
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority() || bMatchEnded)
	{
		return;   // idempotent -- whichever authority fires first concludes; the other no-ops
	}
	bMatchEnded = true; // the cadence (ScheduleNextWindow/OpenWindow) no-ops from here -- terminal.

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindowOpenTimer);   // no window reopens under PostGame
		World->GetTimerManager().ClearTimer(WindowDurationTimer);
	}

	// Starting PostGame auto-cancels Playing AND .ExtractionWindow (non-ancestor siblings) -> the
	// zone's WhenPhaseEnds observer fires -> SetZoneActive(false) -> handle sweep -> any channeler's
	// GA self-cancels. NO explicit window force-close needed (the cancel chain, proven in cycle 1).
	// ORDERING INVARIANT: phase started BEFORE the sweep (already correct here) so a joiner's
	// IsPhaseActiveReflected(PostGame) query agrees with what the sweep just wrote.
	StartPhaseByClass(PostGamePhaseClass, TAG_AFL_GamePhase_PostGame_Driver);
	bWindowOpen = false;
	const int32 Covered = SetMatchTagOnAllPawns(TAG_State_Match_Ended_Driver, /*bPresent=*/true);   // abilities blocked, terminal
	BroadcastMatchEnded();                                     // per-player dual-broadcast w/ Watts
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_PHASE: POSTGAME (match concluded -- Playing+Window cancelled, fire + movement ABILITIES blocked on %d ASC(s), ended-broadcast sent)."),
		Covered);
}

void UAFLMatchPhaseComponent::StartPhaseByClass(TSubclassOf<ULyraGamePhaseAbility> PhaseClass, const FGameplayTag& PhaseTag)
{
	ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld());
	if (PhaseSub && PhaseClass && !IsPhaseActiveReflected(GetWorld(), PhaseTag)) // re-entrancy guard
	{
		ReflectStartPhase(PhaseSub, PhaseClass);
	}
}

int32 UAFLMatchPhaseComponent::SetMatchTagOnAllPawns(const FGameplayTag& Tag, bool bPresent)
{
	// SetLooseGameplayTagCount, not Add/Remove: ApplyJoinStateToPlayer writes the SAME tag to the SAME
	// PlayerState ASC (a player's pawn resolves to it), and refcounted adds would reach 2 and survive a
	// single removal -- a permanently blocked player. Set is idempotent across both paths.
	//
	// The PAWN iteration is deliberate and must stay. B_AFL_TargetDummy_C_0 is level-placed, carries its
	// own ASC, and never passes through OnGameModePlayerInitialized -- this sweep is the only thing that
	// can ever reach it, and it is what the clean-health gate (4b89557b) was proven against. Dedup so a
	// pawn and its PlayerState sharing one ASC counts once.
	TSet<UAbilitySystemComponent*> Seen;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(*It))
		{
			if (!Seen.Contains(ASC))
			{
				ASC->SetLooseGameplayTagCount(Tag, bPresent ? 1 : 0);
				Seen.Add(ASC);
			}
		}
	}
	return Seen.Num();
}

void UAFLMatchPhaseComponent::ApplyJoinStateToPlayer(AController* NewPlayer, UAbilitySystemComponent* PlayerStateASC)
{
	if (!PlayerStateASC)
	{
		return;
	}

	// LIVE QUERIES, no cache -- IsPhaseActiveReflected is the same call StartPhaseByClass uses as its
	// re-entrancy guard, so "what is true now" is answered by the phase system itself and there is
	// nothing to keep in sync. Sites #2 and #5.
	const UWorld* World = GetWorld();
	const bool bWarmupLive = IsPhaseActiveReflected(World, TAG_AFL_GamePhase_Warmup_Driver);
	const bool bEndedLive  = IsPhaseActiveReflected(World, TAG_AFL_GamePhase_PostGame_Driver);
	PlayerStateASC->SetLooseGameplayTagCount(TAG_State_Match_Warmup_Driver, bWarmupLive ? 1 : 0);
	PlayerStateASC->SetLooseGameplayTagCount(TAG_State_Match_Ended_Driver,  bEndedLive  ? 1 : 0);

	// SITE #1 -- the CACHED decision. Do NOT re-read the experience here: that read asserts on
	// LoadState when it happens off the loaded delegate, which is the crash BeginPlay defers around.
	if (bCleanHealthMode)
	{
		PlayerStateASC->SetLooseGameplayTagCount(TAG_State_Mode_NoDismember_Driver, 1);
	}

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_JOIN: phase state applied to %s -- Warmup=%d Ended=%d NoDismember=%d."),
		*GetNameSafe(NewPlayer), bWarmupLive ? 1 : 0, bEndedLive ? 1 : 0, bCleanHealthMode ? 1 : 0);
}

void UAFLMatchPhaseComponent::OnExperienceLoaded_ApplyModeTags(const ULyraExperienceDefinition* Experience)
{
	// Mode == the active experience. Pro Mod / Melee DROP the AFLDismember game feature; Haywire keeps it.
	//
	// ORDERING INVARIANT: CACHE THE DECISION BEFORE SWEEPING. This is the one site of the five with no
	// live query to ask (there is no "is clean-health mode active" phase), so the cache IS the source of
	// truth for every later joiner -- it must be true before anything reads it.
	bCleanHealthMode = (Experience != nullptr) && !Experience->GameFeaturesToEnable.Contains(TEXT("AFLDismember"));
	if (!bCleanHealthMode)
	{
		return;   // Haywire (or unreadable) -> dismember stays ON; never alter the current gore path.
	}

	// Clean-health mode: stamp State.Mode.NoDismember on every combatant ASC so UAFLDamageExecCalc forces
	// bIsZoneRouted=false (single body-health). Cover PlayerState ASCs (persist across respawns) AND pawns
	// (the target dummy carries its own ASC). Dedup because a player's pawn and PlayerState share one ASC.
	TSet<UAbilitySystemComponent*> Applied;
	auto ApplyTo = [&Applied](AActor* Actor)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
		{
			if (!Applied.Contains(ASC))
			{
				ASC->AddLooseGameplayTag(TAG_State_Mode_NoDismember_Driver);
				Applied.Add(ASC);
			}
		}
	};

	if (const AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			ApplyTo(PS);
		}
	}
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		ApplyTo(*It);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MODE: NoDismember applied to %d combatant ASC(s) (clean single-health -- experience '%s' omits AFLDismember)."),
		Applied.Num(), *GetNameSafe(Experience));
}

void UAFLMatchPhaseComponent::SnapshotMatchStartWatts()
{
	MatchStartWatts.Reset();
	if (const AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (const UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr)
			{
				MatchStartWatts.Add(PS, Wallet->GetWatts());
			}
		}
	}
}

void UAFLMatchPhaseComponent::BroadcastMatchEnded()
{
	// One dual-broadcast PER PLAYER: Target = their PlayerState, Magnitude = this-match Watts delta
	// (GetWatts() - snapshot). Each client's announce widget filters Target == its own PlayerState.
	if (const AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			const UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
			const int32* StartWatts = PS ? MatchStartWatts.Find(PS) : nullptr;
			const int32 Earned = (Wallet && StartWatts) ? FMath::Max(0, Wallet->GetWatts() - *StartWatts) : 0;
			BroadcastAnnounce(TAG_Event_Match_Ended_Driver, PS, static_cast<double>(Earned));
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: match-end for %s -- %d Watts this match."), *GetNameSafe(PS), Earned);
		}
	}
}

void UAFLMatchPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchStartGateTimer);
		World->GetTimerManager().ClearTimer(WarmupTimer);
		World->GetTimerManager().ClearTimer(ActiveTimer);
		World->GetTimerManager().ClearTimer(WindowOpenTimer);
		World->GetTimerManager().ClearTimer(WindowDurationTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLMatchPhaseComponent::ScheduleNextWindow()
{
	if (bMatchEnded) { return; } // terminal: no windows reopen under PostGame.
	// Read the period PER SCHEDULE so a mid-match cvar change lands at the next boundary.
	const float Period = FMath::Max(0.5f, CVarAFLExtractWindowPeriod.GetValueOnGameThread());
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(WindowOpenTimer, this, &UAFLMatchPhaseComponent::OpenWindow, Period, /*loop=*/false);
	}
}

void UAFLMatchPhaseComponent::OpenWindow()
{
	if (bMatchEnded) { return; } // terminal guard (a timer that slipped through).
	ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld());
	if (PhaseSub && WindowPhaseClass && !IsWindowActive())
	{
		ReflectStartPhase(PhaseSub, WindowPhaseClass); // zone WhenPhaseStartsOrIsActive observers fire -> Active
		bWindowOpen = true;
		BroadcastAnnounce(TAG_Event_Extraction_WindowOpen_Driver);

		// The driver owns the DURATION now (the phase is a BP shell). Arm the close timer.
		const float Duration = FMath::Max(0.1f, CVarAFLExtractWindowDuration.GetValueOnGameThread());
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(WindowDurationTimer, this, &UAFLMatchPhaseComponent::CloseWindowNow, Duration, /*loop=*/false);
		}
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: extraction window OPEN (%.0fs)."), Duration);
	}
	ScheduleNextWindow(); // chain the next opening regardless (cadence continues).
}

void UAFLMatchPhaseComponent::CloseWindowNow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindowDurationTimer);
	}
	if (!bWindowOpen)
	{
		return;
	}

	// End the active window phase ability on the GameState ASC -> OnEndPhase fires -> zone
	// WhenPhaseEnds observers run (-> SetActive(false) -> handle sweep). Phase abilities carry no
	// AbilityTags (GamePhaseTag is a separate property), so cancel by CLASS via the exported
	// CancelAbilitiesByFunc.
	AGameStateBase* GS = GetGameStateChecked<AGameStateBase>();
	ULyraAbilitySystemComponent* GameState_ASC = GS->FindComponentByClass<ULyraAbilitySystemComponent>();
	if (GameState_ASC && WindowPhaseClass)
	{
		UClass* TargetClass = WindowPhaseClass.Get();
		GameState_ASC->CancelAbilitiesByFunc(
			[TargetClass](const ULyraGameplayAbility* Ability, FGameplayAbilitySpecHandle /*Handle*/)
			{
				return Ability && TargetClass && Ability->IsA(TargetClass);
			},
			/*bReplicateCancelAbility=*/true);
	}
	bWindowOpen = false;
	BroadcastAnnounce(TAG_Event_Extraction_WindowClosed_Driver);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: extraction window CLOSED."));
}

bool UAFLMatchPhaseComponent::IsWindowActive() const
{
	return IsPhaseActiveReflected(GetWorld(), TAG_AFL_GamePhase_ExtractionWindow_Driver);
}

void UAFLMatchPhaseComponent::ForceWindowOpen()
{
	if (GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		OpenWindow();
	}
}

void UAFLMatchPhaseComponent::RescheduleCadence()
{
	if (GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		ScheduleNextWindow(); // reads afl.Extract.WindowPeriod fresh.
	}
}

void UAFLMatchPhaseComponent::ForceWindowClose()
{
	if (GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		CloseWindowNow();
	}
}

void UAFLMatchPhaseComponent::BroadcastAnnounce(const FGameplayTag& EventTag, UObject* Target, double Magnitude) const
{
	UWorld* World = GetWorld();
	ALyraGameState* GS = World ? World->GetGameState<ALyraGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	FLyraVerbMessage Message;
	Message.Verb = EventTag;
	Message.Instigator = GS;
	Message.Target = Target;       // match-end: the player this payload is for (window announces: null)
	Message.Magnitude = Magnitude; // match-end: this-match Watts

	// Clients: the GameState multicast rebroadcasts into each CLIENT's local message subsystem
	// (LyraGameState.cpp:103-114). Its NM_Client guard skips the LISTEN-SERVER HOST, so broadcast
	// locally on the server world too (the accolade-relay dual pattern). Dedicated server: the local
	// broadcast is harmless (no HUD).
	GS->MulticastReliableMessageToClients(Message);
	UGameplayMessageSubsystem::Get(World).BroadcastMessage(Message.Verb, Message);
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	UAFLMatchPhaseComponent* FindDriver(UWorld* World)
	{
		AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->FindComponentByClass<UAFLMatchPhaseComponent>() : nullptr;
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLExtractForceWindowCmd(
		TEXT("afl.Extract.ForceWindow"),
		TEXT("Match phases cycle 1: afl.Extract.ForceWindow open|close -- drive an extraction window deterministically (HOST/authority)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				UAFLMatchPhaseComponent* Driver = FindDriver(World);
				if (!Driver) { Ar.Log(TEXT("afl.Extract.ForceWindow -- no UAFLMatchPhaseComponent on the GameState (run on the HOST inside PIE).")); return; }
				const bool bOpen = Args.Num() > 0 && Args[0].Equals(TEXT("open"), ESearchCase::IgnoreCase);
				if (bOpen) { Driver->ForceWindowOpen(); Ar.Log(TEXT("afl.Extract.ForceWindow -- opened.")); }
				else       { Driver->ForceWindowClose(); Ar.Log(TEXT("afl.Extract.ForceWindow -- closed.")); }
			}));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLMatchRestartCmd(
		TEXT("afl.Match.Restart"),
		TEXT("Match spine cycle 1: restart the spine from Warmup NOW, reading afl.Match.WarmupDuration/ActiveDuration fresh (HOST/authority). Used by afl.Match.Test.Run for a deterministic compressed run."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				UAFLMatchPhaseComponent* Driver = FindDriver(World);
				if (!Driver) { Ar.Log(TEXT("afl.Match.Restart -- no UAFLMatchPhaseComponent on the GameState (HOST inside PIE).")); return; }
				Driver->RestartMatch();
				Ar.Log(TEXT("afl.Match.Restart -- spine restarted from Warmup."));
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
