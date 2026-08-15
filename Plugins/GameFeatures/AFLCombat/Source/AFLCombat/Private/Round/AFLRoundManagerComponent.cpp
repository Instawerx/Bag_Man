// Copyright C12 AI Gaming. All Rights Reserved.

#include "Round/AFLRoundManagerComponent.h"

#include "Match/AFLMatchReporter.h"   // report the result to settlement + rating at match end
#include "Online/AFLGameLiftHostSubsystem.h"   // S12: has the async roster actually arrived yet?

#include "AFLCombat.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"   // Task 2: observe AFL.GamePhase.Playing -> ServerStartMatch (the proven-sibling phase-observer path)
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraGameMode.h"
#include "HAL/IConsoleManager.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Phases/AFLMatchPhaseComponent.h"
#include "Teams/LyraTeamSubsystem.h"
#include "Telemetry/AFLCombatTelemetry.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLRoundManagerComponent)

#if !UE_BUILD_SHIPPING
// DEV-ONLY series shortener. A full first-to-7 series is a long manual sit, and verifying the economy
// round-trip (escrow -> settle -> rating) only needs a series to REACH a result -- the number of rounds it
// took is irrelevant to that. 0 = leave the configured value alone.
//
// Applied to the live component at ServerStartMatch rather than via the CDO on purpose: this component is
// added by the LAS_AFL_ExtractionMatch action set, and an edited CDO was observed NOT to reach the spawned
// instance (a match still ran "first to 7" with the CDO reading 1). Overriding the instance is the only
// form that reliably takes.
static TAutoConsoleVariable<int32> CVarAFLDevRoundsToWin(
	TEXT("afl.Round.DevRoundsToWin"),
	0,
	TEXT("DEV-ONLY (non-shipping): override RoundsToWin for the next match START. 0 = use the configured value. "
	     "Set to 1 so a single round win resolves the series -- the fast path for an economy round-trip test."),
	ECVF_Default);
#endif

// The EXISTING extraction-complete message (UAFLAG_Extract broadcasts it server-side) + the channel
// state tag (the extract ability's ActivationOwnedTag). We only READ these -- no carry/extract edits.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Complete_Round, "Event.Extraction.Complete");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Extracting_Round, "State.Extracting");

// S-ROUND-RESPAWN: applied to every player's PlayerState ASC for the match (ServerStartMatch ->
// Server_EndMatch). The cloned GA_AFL_AutoRespawn's branch reads this off the owning ASC and SKIPS its
// RequestPlayerRestartNextFrame node while present -- so a mid-round death stays dead (ragdoll + death-cam =
// tactical spectate) and the round FSM's round-start force-respawn is the lone respawn authority (no BP-latent
// death-respawn competing -> no double, no orphan). Native-static for CDO-safe use; AFLCombatTags.ini is the
// spec source-of-truth (UE dedups native+ini).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Round_NoRespawn, "State.Round.NoRespawn");

// TASK 2: the match-phase "Playing" tag (UAFLMatchPhaseComponent starts it at the Warmup->Playing edge). Its
// driver tag is file-local to AFLMatchPhaseComponent.cpp, so we define our own static for the same string (UE
// dedups) and observe it WITHOUT linking the driver's symbol -- exactly how AAFLExtractionZone defines its own.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Playing_Round, "AFL.GamePhase.Playing");

namespace
{
	// NO team-id magic numbers -- the two participating ids are resolved from ULyraTeamSubsystem at
	// ServerStartMatch into the replicated ParticipatingTeams[2] (the ShooterCore two-team stack uses 1/2, not 0/1).
	// BetweenRounds requests respawns (gate open: Phase=RoundEnd/HalfTime); we delay BeginRound past the
	// next-frame restart so Phase=RoundActive does not re-lock the gate before the fresh pawns land.
	constexpr float AFLRoundPostResetBeginDelay = 1.0f;
	constexpr float AFLRoundContestRadius = 1500.0f;   // an enemy within this of the bank point = contested (telemetry)
}

UAFLRoundManagerComponent::UAFLRoundManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// DIVERGENCE from UAFLMatchPhaseComponent (which never ticks): a throttled server tick publishes the
	// replicated RoundTimeRemaining for the HUD countdown.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.25f;   // 4 Hz, not per-frame
	// DIVERGENCE: the sibling is server-only; this component replicates its state to drive the HUD.
	SetIsReplicatedByDefault(true);
	ParticipatingTeams[0] = INDEX_NONE;
	ParticipatingTeams[1] = INDEX_NONE;
}

void UAFLRoundManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLRoundManagerComponent, MatchId);   // A1.3b: per-match id (Arena series)
	DOREPLIFETIME(UAFLRoundManagerComponent, Phase);
	DOREPLIFETIME(UAFLRoundManagerComponent, CurrentRound);
	DOREPLIFETIME(UAFLRoundManagerComponent, Team0Score);
	DOREPLIFETIME(UAFLRoundManagerComponent, Team1Score);
	DOREPLIFETIME(UAFLRoundManagerComponent, RoundTimeRemaining);
	DOREPLIFETIME(UAFLRoundManagerComponent, WarmupTimeRemaining);
	DOREPLIFETIME(UAFLRoundManagerComponent, bSidesSwapped);
	DOREPLIFETIME(UAFLRoundManagerComponent, ParticipatingTeams);
	DOREPLIFETIME(UAFLRoundManagerComponent, LastWinningTeam);
	DOREPLIFETIME(UAFLRoundManagerComponent, LastWinReason);
}

bool UAFLRoundManagerComponent::HasAuth() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UAFLRoundManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Mirror the sibling's authority gate. Clients only consume the replicated state via OnRep.
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		return;
	}

	// Server-side listen for the EXISTING extraction-complete message (broadcast on THIS server world by
	// UAFLAG_Extract after EarnWattsAuthority). Consuming a server-world message ON THE SERVER is correct:
	// we resolve authoritatively and REPLICATE the score. (The "messages never reach clients" rule only
	// bars inferring state TO clients via messages -- which we do not do.) ZERO carry/extract edits.
	if (UWorld* World = GetWorld())
	{
		ExtractListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
			TAG_Event_Extraction_Complete_Round,
			[this](FGameplayTag Channel, const FLyraVerbMessage& Msg) { HandleExtractionBanked(Channel, Msg); });
	}

	SetPhaseAuthoritative(EAFLRoundPhase::WarmUp);

	// TASK 2 (was the unwired trigger): auto-start the match FSM on the natural Warmup->Playing transition so
	// MatchId assigns WITHOUT the afl.Round.Start cheat. There is NO delegate on UAFLMatchPhaseComponent to bind
	// (EnterPlaying is private + broadcasts nothing) -- the transition is observable ONLY as the
	// AFL.GamePhase.Playing phase-start on the Lyra GamePhaseSubsystem. So register the SAME reflective observer
	// AAFLExtractionZone uses (THE LYRA PHASE WALL: the C++ WhenPhase* overloads aren't LYRAGAME_API + the K2_
	// UFUNCTIONs are protected -> bind via ProcessEvent). ExactMatch -> fires once on the Playing entry (not the
	// .ExtractionWindow child). We are already past the authority gate (~:89), so this is server-only.
	if (ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld()))
	{
		struct FK2WhenPhaseParams
		{
			FGameplayTag PhaseTag;
			EPhaseTagMatchType MatchType = EPhaseTagMatchType::ExactMatch;
			FLyraGamePhaseTagDynamicDelegate WhenPhase;
		};
		if (UFunction* Fn = PhaseSub->FindFunction(TEXT("K2_WhenPhaseStartsOrIsActive")))
		{
			FK2WhenPhaseParams Params;
			Params.PhaseTag = TAG_AFL_GamePhase_Playing_Round;
			Params.MatchType = EPhaseTagMatchType::ExactMatch;
			Params.WhenPhase.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAFLRoundManagerComponent, HandlePlayingPhaseActive));
			PhaseSub->ProcessEvent(Fn, &Params);
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_TASK2 bind ok -- observing AFL.GamePhase.Playing -> ServerStartMatch (match auto-start; afl.Round.Start no longer required)."));
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TASK2 bind FAILED -- K2_WhenPhaseStartsOrIsActive not found on ULyraGamePhaseSubsystem (fallback: afl.Round.Start)."));
		}
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_TASK2 bind SKIP -- no ULyraGamePhaseSubsystem in this world (fallback: afl.Round.Start)."));
	}
}

void UAFLRoundManagerComponent::HandlePlayingPhaseActive(const FGameplayTag& PhaseTag)
{
	// TASK 2 callback: AFL.GamePhase.Playing started. ExactMatch means this fires ONLY for that tag, so no
	// in-handler phase filter is needed (mirrors AAFLExtractionZone, which relies on ExactMatch, not a tag
	// check). Route straight into the EXISTING ServerStartMatch (unchanged): its bMatchStarted guard + the
	// <2-teams abort-without-marking retry make a re-fire a no-op -> exactly one AFL_A13B_MATCHID assign.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_TASK2 Playing phase active (tag=%s) -> ServerStartMatch."), *PhaseTag.ToString());

	// NO GATE HERE ANY MORE. It lived here, then in ServerStartMatch, and both were the wrong altitude --
	// EnterPlaying() has nine downstream consumers and gating one left eight ungated (rounds, then escrow,
	// then the roster seal, each found by a separate dead run). The hold now sits at the top of
	// UAFLMatchPhaseComponent::EnterPlaying, so this phase CANNOT fire before the payload has arrived and a
	// placed player is present. By the time this observer runs, both are already true.
	ServerStartMatch();
}

void UAFLRoundManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
		World->GetTimerManager().ClearTimer(ResetTimerHandle);
	}
	if (ExtractListenerHandle.IsValid())
	{
		ExtractListenerHandle.Unregister();
	}
	UnbindDeathDelegates();
	Super::EndPlay(EndPlayReason);
}

void UAFLRoundManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// WARMUP COUNTDOWN -- published BEFORE the RoundActive gate below, because warmup is by definition not
	// RoundActive. UAFLMatchPhaseComponent owns the clock (it holds WarmupTimer) but is a server-only
	// driver that replicates nothing; this component already ticks and already replicates, so it mirrors
	// the value out. Reading the phase component's live timer means there is no second countdown to drift.
	if (HasAuth())
	{
		float NewWarmup = 0.f;
		if (Phase == EAFLRoundPhase::WarmUp)
		{
			if (!PhaseComp.IsValid())
			{
				const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
				PhaseComp = GS ? GS->FindComponentByClass<UAFLMatchPhaseComponent>() : nullptr;
			}
			if (const UAFLMatchPhaseComponent* MP = PhaseComp.Get())
			{
				NewWarmup = FMath::Max(0.f, MP->GetWarmupSecondsRemaining());
			}
		}
		// Whole-second throttle: the HUD re-texts on second boundaries only, so replicating every frame
		// would be bandwidth for no visible change.
		if (FMath::CeilToInt(NewWarmup) != FMath::CeilToInt(WarmupTimeRemaining))
		{
			WarmupTimeRemaining = NewWarmup;
			// ~30 lines per warmup, one per second. Diagnostic for the countdown watch: if these tick
			// down but the HUD does not, the fault is the render; if they never appear, it is the
			// publish. Drop to Verbose once the countdown is signed off.
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: warmup countdown -> %.0fs"), NewWarmup);
		}

		// THE ABANDONMENT WATCHDOG MOVED, 2026-08-15. It now lives on UAFLMatchPhaseComponent and reaches this
		// component through IAFLMatchCancelPolicy. The reason is not tidiness: this component is in the MATCH
		// PLAY experiences and in NEITHER battle royale one, so the watch it owned could never see an abandoned
		// BR -- a staked BR held its pot until the process tore down. The phase component is resident in both.
		//
		// Its properties are unchanged and are now that component's: still 60s, still ABOVE any round gate (a
		// lobby can empty during warmup, between rounds or at half time), still reset by any human returning.
	}

	if (!HasAuth() || Phase != EAFLRoundPhase::RoundActive)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (World)
	{
		RoundTimeRemaining = World->GetTimerManager().GetTimerRemaining(RoundTimerHandle);
	}

	// s6 TRAVERSAL SAMPLER (additive): throttled per-LIVING-pawn position emit -- the traversal-density heatmap
	// source. Server-side only (the HasAuth + RoundActive gate above). Mirrors the living-pawn iteration in
	// HandleExtractionBanked. The accumulator makes this tick-rate-agnostic (fires every TraverseSampleInterval s).
	TraverseSampleAccum += DeltaTime;
	if (World && TraverseSampleAccum >= TraverseSampleInterval)
	{
		TraverseSampleAccum = 0.f;
		const AGameStateBase* GS = World->GetGameState<AGameStateBase>();
		const ULyraTeamSubsystem* Teams = World->GetSubsystem<ULyraTeamSubsystem>();
		if (GS && Teams)
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				if (!PS) { continue; }
				const APawn* P = PS->GetPawn();
				if (!P) { continue; }
				const ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(P);
				if (HC && HC->IsDeadOrDying()) { continue; }   // living pawns only
				FAFLCombatTelemetry::EmitTraverse(P, Teams->FindTeamFromObject(PS), P->GetActorLocation());
			}
		}
	}
}

void UAFLRoundManagerComponent::ServerStartMatch()
{
	if (!HasAuth() || bMatchStarted)
	{
		return;
	}
	// Resolve the two participating team ids DYNAMICALLY (no magic numbers). ULyraTeamCreationComponent
	// creates the teams at experience load (its GameState-component BeginPlay); ServerStartMatch fires
	// post-load (the afl.Round.Start cheat / the match-phase trigger), so the teams exist by now.
	const ULyraTeamSubsystem* Teams = GetWorld() ? GetWorld()->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	TArray<int32> Ids = Teams ? Teams->GetTeamIDs() : TArray<int32>();
	Ids.Sort();   // ascending -- slot 0 = lowest id, slot 1 = next
	if (Ids.Num() < 2)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_ROUND: cannot START -- need 2 teams, ULyraTeamSubsystem::GetTeamIDs found %d. Aborting (retry once teams exist)."), Ids.Num());
		return;   // abort WITHOUT marking started -- a later call retries once teams exist
	}
	ParticipatingTeams[0] = Ids[0];
	ParticipatingTeams[1] = Ids[1];

	bMatchStarted = true;
	// A1.3b: author the per-MATCH id ONCE here -- past the bMatchStarted guard (~:164) + the <2-teams abort
	// (which returns WITHOUT marking started), so it is set exactly once per match and cannot re-roll. Stable
	// for the whole Arena series; the earn push (later cycle) sends it as the contract's matchId.
	MatchId = FGuid::NewGuid();
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_A13B_MATCHID assigned %s"), *GetMatchId());
	CurrentRound = 0;
	Team0Score = 0;
	Team1Score = 0;
	bSidesSwapped = false;
	bMatchConcluded = false;
	ConsecutiveReplays = 0;
	HumanlessSeconds = 0.f;
	// SEEDED HERE, LATCHED LATER. Derived rather than written from outside: under GameLift the phase gate
	// guarantees a human before Warmup->Playing can fire, so this reads true and cannot disagree with the
	// condition that released the gate -- and a derived value cannot drift the way a member set by another
	// component could. The phase component never reaches across into this one.
	//
	// OFF GameLift there is no gate and this reads FALSE, which is correct and is the point: the match has
	// started with nobody here yet. Server_TickAbandonmentWatch latches it the moment the first human appears
	// and refuses to run the abandonment clock until then. Seeding it without that latch is what left it a
	// dead value -- written every match, read nowhere.
	bAnyHumanEverJoined = CountHumanParticipants() > 0;
	OnRep_Score();   // listen-host local HUD
#if !UE_BUILD_SHIPPING
	// Read once per match, here, so the log line below always reports the value the match will ACTUALLY use.
	if (const int32 DevRoundsToWin = CVarAFLDevRoundsToWin.GetValueOnGameThread(); DevRoundsToWin > 0)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_ROUND: afl.Round.DevRoundsToWin=%d -- overriding RoundsToWin (was %d). DEV ONLY."),
			DevRoundsToWin, RoundsToWin);
		RoundsToWin = DevRoundsToWin;
		// Keep half-time out of range of a shortened series; a swap mid-way through a 1-round match is noise.
		HalfTimeAfterRound = FMath::Max(HalfTimeAfterRound, RoundsToWin * 2);
	}
#endif

#if !UE_BUILD_SHIPPING
	// S12 GATE. Under GameLift the roster arrives asynchronously, so a match that starts without it will run
	// on whatever the team provider guessed -- observed once as both players on one team and unstaked, while
	// the log cheerfully showed the payload arriving. Not a hard block: refusing to start with no retry path
	// would hang the match, and escrow already refuses an unverifiable roster, so currency is safe regardless.
	// What was missing was a way to SEE the condition without team-assignment archaeology.
	if (const UAFLGameLiftHostSubsystem* GameLift = UAFLGameLiftHostSubsystem::Get(this))
	{
		if (GameLift->IsSdkReady() && !GameLift->HasGameSessionData())
		{
			UE_LOG(LogAFLCombat, Error,
				TEXT("AFL_ROUND: STARTING UNDER GAMELIFT WITH NO PAYLOAD. onStartGameSession has not been "
				     "received, so teams and economics come from a guess. Expect LocalFill and LEAGUE PLAY."));
		}
	}
#endif

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: match START (teams %d v %d; first to %d; half-swap after round %d)."),
		ParticipatingTeams[0], ParticipatingTeams[1], RoundsToWin, HalfTimeAfterRound);

	// TAKE THE STAKE. This is the earliest point where MatchId exists and the roster is settled, and it must
	// happen BEFORE the match is played: /settle-match verifies its claimed entries against the escrow rows
	// and refuses a pot that was never funded, so a match that skips this cannot pay out at all.
	//
	// A no-op in LEAGUE PLAY (no buy-in) and in any session without the economy env vars -- so an ordinary
	// PIE run is unaffected. Escrow is all-or-nothing: it validates every team before debiting anyone, so a
	// misconfigured staked match charges nobody rather than charging half the lobby.
	//
	// THE RETURNED LEDGER IS HELD FOR THE WHOLE MATCH, and it is the only reason an abandoned match can be
	// refunded: by the time abandonment is detected the players are gone from PlayerArray, and this snapshot is
	// all that still knows who was charged what. Null when nothing was taken.
	EscrowLedger = FAFLMatchReporter::EscrowTeamSeries(this, MatchId, FAFLMatchReporter::ReadEconomics(this));

	// Suppress the ShooterCore auto-respawn for the whole match: the cloned GA_AFL_AutoRespawn skips its
	// respawn node while State.Round.NoRespawn is on the owning ASC, so the round FSM is the LONE respawn
	// authority (the round-start force-respawn -- no BP-latent death-respawn competing). Human + bot.
	SetRoundRespawnSuppressed(true);

	// Round-based mode: the round FSM is the SOLE match-end authority -- tell the resident match-phase
	// component (present for the extraction-window cadence) NOT to time-conclude at its 480s ActiveDuration
	// (a clock ending a best-of mid-series is illogical). The window cadence stays; only its match-END no-ops.
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr)
	{
		if (UAFLMatchPhaseComponent* MatchPhase = GS->FindComponentByClass<UAFLMatchPhaseComponent>())
		{
			MatchPhase->SetExternalMatchEndAuthority(true);
		}
	}

	Server_BeginRound();
}

void UAFLRoundManagerComponent::Server_BeginRound()
{
	if (!HasAuth() || bMatchConcluded)
	{
		// The latch, not just the cleared timers. Both terminal paths clear ResetTimerHandle, so this is
		// belt-and-braces -- but a round starting after the outcome has been reported would run a match whose
		// result is already settled, and that is worth three lines to make structurally impossible.
		return;
	}
	++CurrentRound;
	Team0Banked = 0;
	Team1Banked = 0;
	RoundTimeRemaining = RoundTimeLimit;
	SetPhaseAuthoritative(EAFLRoundPhase::RoundActive);

	BindDeathDelegates();   // bind OnDeathStarted on the now-live pawns (round 1: initial spawns; later: post-reset)

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RoundTimerHandle, this, &UAFLRoundManagerComponent::Server_OnRoundTimeout,
			FMath::Max(1.0f, RoundTimeLimit), /*loop=*/false);
	}
	FAFLCombatTelemetry::EmitRoundStart(CurrentRound);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: round %d START (%.0fs; sides %s)."),
		CurrentRound, RoundTimeLimit, bSidesSwapped ? TEXT("SWAPPED") : TEXT("normal"));
}

void UAFLRoundManagerComponent::HandlePlayerDeath(AActor* OwningActor)
{
	if (!HasAuth() || Phase != EAFLRoundPhase::RoundActive)
	{
		return;
	}

	const ULyraTeamSubsystem* Teams = GetWorld() ? GetWorld()->GetSubsystem<ULyraTeamSubsystem>() : nullptr;

	// Per-kill spatial telemetry (world-Z) for Task-2 per-level heatmaps -- emitted from MY listener, so
	// the proven combat/damage code stays untouched. (There were no kill/death emits to add Z to.)
	const int32 VictimTeam = Teams ? Teams->FindTeamFromObject(OwningActor) : INDEX_NONE;
	const FVector Loc = OwningActor ? OwningActor->GetActorLocation() : FVector::ZeroVector;
	FAFLCombatTelemetry::EmitElimination(OwningActor, /*Killer=*/nullptr, VictimTeam, Loc);

	// Team-wipe check -- recompute alive counts authoritatively (the just-dead pawn already reads IsDeadOrDying).
	const int32 Alive0 = AliveCount(ParticipatingTeams[0]);   // slot 0
	const int32 Alive1 = AliveCount(ParticipatingTeams[1]);   // slot 1
	if (Alive0 == 0 && Alive1 == 0)
	{
		Server_ResolveRound(INDEX_NONE, EAFLRoundWinReason::Replay);                  // simultaneous double-wipe -> no-score
	}
	else if (Alive0 == 0)
	{
		Server_ResolveRound(ParticipatingTeams[1], EAFLRoundWinReason::Elimination);  // slot-0 team wiped -> slot-1 wins
	}
	else if (Alive1 == 0)
	{
		Server_ResolveRound(ParticipatingTeams[0], EAFLRoundWinReason::Elimination);  // slot-1 team wiped -> slot-0 wins
	}
}

void UAFLRoundManagerComponent::HandleExtractionBanked(FGameplayTag /*Channel*/, const FLyraVerbMessage& Message)
{
	if (!HasAuth() || Phase != EAFLRoundPhase::RoundActive)
	{
		return;
	}
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	const ULyraTeamSubsystem* Teams = GetWorld() ? GetWorld()->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	if (!GS || !Teams)
	{
		return;
	}

	UObject* InstigatorObj = Message.Instigator;                  // the channeling pawn (set by UAFLAG_Extract)
	const int32 TeamId = Teams->FindTeamFromObject(InstigatorObj);
	const AActor* Channeler = Cast<AActor>(InstigatorObj);
	const FVector Loc = Channeler ? Channeler->GetActorLocation() : FVector::ZeroVector;
	const int32 BankValue = FMath::Max(0, static_cast<int32>(Message.Magnitude));

	// Accumulate per-team banked (the timeout tiebreak source). In practice the first complete ends the round.
	const int32 BankSlot = SlotForTeam(TeamId);
	if (BankSlot == 0) { Team0Banked += BankValue; }
	else if (BankSlot == 1) { Team1Banked += BankValue; }

	// Telemetry: a contest read (any LIVE enemy near the bank point) + the outcome -- both with world-Z.
	bool bContested = false;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		const int32 PTeam = Teams->FindTeamFromObject(PS);
		if (PTeam == INDEX_NONE || PTeam == TeamId) { continue; }
		const APawn* P = PS->GetPawn();
		if (!P || FVector::Dist(P->GetActorLocation(), Loc) > AFLRoundContestRadius) { continue; }
		const ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(P);
		if (HC && !HC->IsDeadOrDying()) { bContested = true; break; }
	}
	FAFLCombatTelemetry::EmitExtractContest(Channeler, bContested, Loc);
	FAFLCombatTelemetry::EmitExtractOutcome(Channeler, TeamId, /*bSuccess=*/true, Loc);

	if (BankSlot != INDEX_NONE)
	{
		Server_ResolveRound(TeamId, EAFLRoundWinReason::Extraction);   // completing a central bank wins the round
	}
}

void UAFLRoundManagerComponent::Server_OnRoundTimeout()
{
	if (!HasAuth() || Phase != EAFLRoundPhase::RoundActive)
	{
		return;
	}
	const int32 Winner = ComputeTimeoutWinner();
	Server_ResolveRound(Winner, (Winner == INDEX_NONE) ? EAFLRoundWinReason::Replay : EAFLRoundWinReason::Timeout);
}

int32 UAFLRoundManagerComponent::ComputeTimeoutWinner() const
{
	if (Team0Banked > Team1Banked) { return ParticipatingTeams[0]; }
	if (Team1Banked > Team0Banked) { return ParticipatingTeams[1]; }
	return TeamHoldingCore();   // banked tie -> core holder; INDEX_NONE on double-tie -> Replay (no-score)
}

void UAFLRoundManagerComponent::Server_ResolveRound(int32 WinningTeamId, EAFLRoundWinReason Reason)
{
	if (!HasAuth() || Phase != EAFLRoundPhase::RoundActive)
	{
		return;   // single-resolve guard (a wipe + timeout in the same frame both call here)
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
	}
	UnbindDeathDelegates();

	const int32 WinSlot = SlotForTeam(WinningTeamId);
	if (WinSlot == 0) { ++Team0Score; }
	else if (WinSlot == 1) { ++Team1Score; }
	// INDEX_NONE (Replay / non-participating) -> no score change

	// DEFECT 1: BOUND THE REPLAYS. Keyed off whether the round SCORED, not off the reason -- the score is what
	// first-to-RoundsToWin actually consumes, so it is the honest test for "did this round move the series".
	// Reset on any scoring round, because the stall this guards against is a RUN of them (see the header).
	if (WinSlot == INDEX_NONE) { ++ConsecutiveReplays; }
	else                       { ConsecutiveReplays = 0; }

	LastWinningTeam = WinningTeamId;
	LastWinReason = Reason;
	OnRep_Score();            // listen-host local HUD
	OnRep_RoundResolved();    // listen-host toast + server-side OnRoundResolved binds
	EmitRoundTelemetry(WinningTeamId, Reason);
	SetPhaseAuthoritative(EAFLRoundPhase::RoundEnd);

	// The run is reported ON the line that would otherwise repeat identically forever -- the S12 log had 219
	// of those and nothing in any of them said the series was going nowhere.
	const FString RunSuffix = (ConsecutiveReplays > 0)
		? FString::Printf(TEXT(" (no-score run: %d/%d)"), ConsecutiveReplays, MaxConsecutiveReplays)
		: FString();
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: round %d RESOLVED -- winner team %d, reason %s. Score %d-%d.%s"),
		CurrentRound, WinningTeamId, *UEnum::GetValueAsString(Reason), Team0Score, Team1Score, *RunSuffix);

	if (Team0Score >= RoundsToWin) { Server_EndMatch(ParticipatingTeams[0]); return; }
	if (Team1Score >= RoundsToWin) { Server_EndMatch(ParticipatingTeams[1]); return; }

	// Checked AFTER the win conditions: a round that scores cannot be a replay, so this can never pre-empt a
	// legitimately won series. It only fires where the series cannot progress at all.
	if (MaxConsecutiveReplays > 0 && ConsecutiveReplays >= MaxConsecutiveReplays)
	{
		Server_CancelMatch(EAFLMatchCancelReason::ReplayCap);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ResetTimerHandle, this, &UAFLRoundManagerComponent::Server_BetweenRounds,
			FMath::Max(0.5f, RoundResetCountdown), /*loop=*/false);
	}
}

void UAFLRoundManagerComponent::Server_BetweenRounds()
{
	if (!HasAuth() || bMatchConcluded)
	{
		// Same latch as Server_BeginRound, and for the same reason: this respawns the whole lobby, which is a
		// conspicuous thing to do in a match whose outcome has already been reported. The terminal paths clear
		// ResetTimerHandle so it should be unreachable -- "should be" is what the guard is for.
		return;
	}
	// Side swap BEFORE the reset, so the respawn selects the swapped side (the game mode reads bSidesSwapped).
	if (CurrentRound == HalfTimeAfterRound)
	{
		Server_EnterHalfTime();
	}
	Server_ResetRoundActors();   // request fresh pawns -- gate is OPEN here (Phase is RoundEnd/HalfTime)

	// Begin the next round AFTER the next-frame restarts land (so RoundActive does not deny them).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ResetTimerHandle, this, &UAFLRoundManagerComponent::Server_BeginRound,
			AFLRoundPostResetBeginDelay, /*loop=*/false);
	}
}

void UAFLRoundManagerComponent::Server_EnterHalfTime()
{
	bSidesSwapped = !bSidesSwapped;
	SetPhaseAuthoritative(EAFLRoundPhase::HalfTime);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: HALFTIME after round %d -- sides now %s."),
		CurrentRound, bSidesSwapped ? TEXT("SWAPPED") : TEXT("normal"));
}

void UAFLRoundManagerComponent::Server_EndMatch(int32 WinningTeamId)
{
	// ONE TERMINAL REPORT PER MATCH. Settlement claims the matchId with a conditional write, so a second report
	// does not correct the first -- it races it. See bMatchConcluded in the header.
	if (bMatchConcluded)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_ROUND: Server_EndMatch(team %d) ignored -- match %s has already concluded."),
			WinningTeamId, *GetMatchId());
		return;
	}
	bMatchConcluded = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
		World->GetTimerManager().ClearTimer(ResetTimerHandle);
	}
	UnbindDeathDelegates();
	SetRoundRespawnSuppressed(false);   // match over -> restore normal auto-respawn (warmup / non-round / next match)
	SetPhaseAuthoritative(EAFLRoundPhase::MatchEnd);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: MATCH END -- team %d wins %d-%d -> concluding."),
		WinningTeamId, Team0Score, Team1Score);

	// Officially conclude via the PROVEN PostGame machinery on the resident match-phase component (CALL,
	// not replicate -- residency verified from the log: AFL_ROUND + AFL_PHASE both run this experience).
	// ConcludeMatch frees fire/movement (State.Match.Ended) + starts PostGame + broadcasts per-player Watts
	// (-> the MATCH COMPLETE banner). bMatchEnded makes it idempotent. Null-guarded defensively despite the
	// confirmed residency -- a missing component logs + skips rather than crashing.
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr)
	{
		if (UAFLMatchPhaseComponent* MatchPhase = GS->FindComponentByClass<UAFLMatchPhaseComponent>())
		{
			MatchPhase->ConcludeMatch();
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_ROUND: MATCH END but no UAFLMatchPhaseComponent resident -- conclusion (freeze/PostGame/Watts) SKIPPED."));
		}
	}

	// ECONOMY + LADDER. The match is over and the outcome is final, so this is the one place the result can
	// be reported: settlement pays the §5.2 curve against the escrow rows, rating moves the OpenSkill ladder.
	//
	// AFTER ConcludeMatch on purpose. Reporting is fire-and-forget HTTP whose completion may land many frames
	// later; making the players wait on a backend round-trip to see the match end would trade a guaranteed
	// UX cost for no correctness gain -- both endpoints are idempotent, so a late or retried report is safe.
	//
	// Everything below is a no-op off the server: the HMAC key exists only on a dedicated server or in the
	// editor, so a cooked client silently reports nothing (N11 -- the client asserts no outcome).
	{
		const FAFLMatchReporter::FMatchEconomics Econ = FAFLMatchReporter::ReadEconomics(this);

		FAFLMatchResult Result;
		FString BuildError;
		// THE LEDGER IS PASSED SO LEAVERS ARE STILL PAID. Operator ruling 2026-08-15: a MATCH PLAY player who
		// disconnects forfeits but SHARES THEIR TEAM'S PAYOUT if that team wins -- their stake funded the
		// position's unit. PlayerArray cannot describe them by now; the ledger has held them since match start.
		// Null on an unstaked match, where there is no pot and nobody is owed anything to miss.
		if (FAFLMatchReporter::BuildTeamSeriesResult(this, MatchId, WinningTeamId, Econ, EscrowLedger.Get(), Result, BuildError))
		{
			FAFLMatchReporter::ReportMatchEnd(this, Result, Econ.StakePerPosition, Econ.CurrencyCode);
		}
		else
		{
			// Loud, and it does NOT block conclusion. A match that cannot be reported has still been played;
			// swallowing the reason is what would make it unrecoverable.
			UE_LOG(LogAFLCombat, Error, TEXT("AFL_ROUND: match %s NOT reported -- %s"),
				*GetMatchId(), *BuildError);
		}
	}
}

int32 UAFLRoundManagerComponent::CountHumanParticipants() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		// No game state is not "no humans". Returning 0 here would start the abandonment clock during a world
		// transition, so report the state that cannot be wrong: something is present until proven otherwise.
		return 1;
	}
	int32 Count = 0;
	for (const APlayerState* PS : GS->PlayerArray)
	{
		// A disconnected player's PlayerState leaves PlayerArray (AGameModeBase::Logout), which is exactly the
		// signal this needs -- no separate connection bookkeeping to drift out of sync with it.
		if (!PS || PS->IsABot() || PS->IsOnlyASpectator())
		{
			continue;
		}
		++Count;
	}
	return Count;
}

bool UAFLRoundManagerComponent::IsMatchLiveForAbandonment() const
{
	// The same window the watch used when it lived here: between match start and match conclusion.
	return bMatchStarted && !bMatchConcluded;
}

void UAFLRoundManagerComponent::ServerCancelAbandoned()
{
	Server_CancelMatch(EAFLMatchCancelReason::Abandoned);
}

// Server_TickAbandonmentWatch DELETED 2026-08-15 -- the humanless watch now lives on
// UAFLMatchPhaseComponent and reaches this component through IAFLMatchCancelPolicy. Removed rather than
// left dormant: two implementations of one policy is precisely the drift that made AreBotsPermitted
// necessary, and a dead copy of a money-critical clock is the worst kind to leave lying around.

void UAFLRoundManagerComponent::Server_CancelMatch(EAFLMatchCancelReason Reason)
{
	if (!HasAuth() || bMatchConcluded)
	{
		return;   // the latch shared with Server_EndMatch -- one terminal report per match, never a race
	}
	bMatchConcluded = true;

	// NO SHOW READS DIFFERENTLY FROM ABANDONED ON PURPOSE. "Nobody remained" and "nobody ever came" are
	// different events with different causes -- one is a player problem, the other a placement/travel one --
	// and a log that spells them the same way sends the next reader down the wrong path.
	const TCHAR* ReasonText;
	switch (Reason)
	{
	case EAFLMatchCancelReason::Abandoned: ReasonText = TEXT("ABANDONED -- no human participants remained"); break;
	case EAFLMatchCancelReason::NoShow:    ReasonText = TEXT("NO SHOW -- no placed player ever arrived");     break;
	default:                               ReasonText = TEXT("STALEMATE -- consecutive no-score rounds hit the replay cap"); break;
	}

	// Stop the FSM FIRST. Every handler below is gated on RoundActive, so clearing the timers and leaving
	// RoundActive is what actually stops rounds resolving -- without it a pending reset timer would begin
	// round N+1 into a match whose refund is already in flight.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
		World->GetTimerManager().ClearTimer(ResetTimerHandle);
	}
	UnbindDeathDelegates();
	SetRoundRespawnSuppressed(false);
	SetPhaseAuthoritative(EAFLRoundPhase::MatchEnd);

	UE_LOG(LogAFLCombat, Warning,
		TEXT("AFL_ROUND: MATCH CANCELLED -- %s. Match %s ended at round %d, score %d-%d, NO RESULT."),
		ReasonText, *GetMatchId(), CurrentRound, Team0Score, Team1Score);

	// REFUND FIRST. Server_EndMatch concludes before reporting so players are not held on an HTTP round-trip
	// to see the match end; here the usual case is that nobody is left to hold, and the refund is the whole
	// reason this path exists. Both are fire-and-forget, so nothing downstream depends on the order.
	//
	// NOT ReportMatchEnd: that would post terminalState 'settled' against a match with no winner AND move the
	// ladder. A cancelled match pays back exactly what it took and moves no rating.
	if (EscrowLedger.IsValid())
	{
		FAFLMatchReporter::ReportMatchCancelled(this, *EscrowLedger, ReasonText);
	}
	else
	{
		// Nothing was staked (LEAGUE PLAY, an unwired economy, or a refused escrow) -- so there is nothing to
		// give back. Said explicitly, because "no refund line in the log" and "the refund never fired" look
		// identical otherwise, and only one of them is fine.
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: match %s held no escrow -- cancellation refunds nothing."), *GetMatchId());
	}

	// Then release the match the same way a won one is released: ConcludeMatch is what frees fire/movement,
	// starts PostGame and lets the session wind down. Skipping it for a cancellation would leave a dedicated
	// server sitting in a live match phase with no match -- the exact state this fix exists to end.
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr)
	{
		if (UAFLMatchPhaseComponent* MatchPhase = GS->FindComponentByClass<UAFLMatchPhaseComponent>())
		{
			MatchPhase->ConcludeMatch();
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_ROUND: MATCH CANCELLED but no UAFLMatchPhaseComponent resident -- conclusion (freeze/PostGame) SKIPPED."));
		}
	}
}

void UAFLRoundManagerComponent::SetRoundRespawnSuppressed(bool bSuppressed)
{
	// ORDERING INVARIANT: CACHE BEFORE SWEEPING. bRespawnSuppressed is the source of truth for every
	// later joiner (site #4 has no live query to ask), so it must be correct before anything -- including
	// a same-frame join -- can read it. Set it first, unconditionally, even on the early-out below.
	// IF THIS EVER BECOMES LATENT OR TIMER-DRIVEN, REDO THIS ANALYSIS.
	bRespawnSuppressed = bSuppressed;

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return;
	}
	int32 Count = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}
		// The ASC lives on the PlayerState in Lyra, so the tag persists across pawn deaths/respawns -- one
		// apply at match-start covers every round. SetLooseGameplayTagCount is idempotent. Server-side suffices:
		// the cloned GA's respawn branch runs on authority (RequestPlayerRestartNextFrame is authority-only).
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
		{
			ASC->SetLooseGameplayTagCount(TAG_State_Round_NoRespawn, bSuppressed ? 1 : 0);
			++Count;
		}
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: round-respawn suppression %s on %d player ASC(s) (State.Round.NoRespawn -> GA_AFL_AutoRespawn branch)."),
		bSuppressed ? TEXT("APPLIED") : TEXT("REMOVED"), Count);
}

void UAFLRoundManagerComponent::Server_ResetRoundActors()
{
	UWorld* World = GetWorld();
	ALyraGameMode* GM = World ? World->GetAuthGameMode<ALyraGameMode>() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState<AGameStateBase>() : nullptr;
	if (!GM || !GS)
	{
		return;
	}
	// Force-respawn everyone. The FRESH pawn brings a fresh (empty) carry component and the destroyed pawn
	// cancels any live extract channel -- so "reset central extract / clear carried parts" falls out of the
	// respawn with ZERO carry/extract edits. Side selection reads bSidesSwapped (game mode hook; Task 2 data).
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (AController* C = PS ? PS->GetOwningController() : nullptr)
		{
			// Round-end SURVIVORS (alive at resolve) still possess a live pawn. RequestPlayerRestartNextFrame
			// resets only the CONTROLLER (AController::Reset clears StartSpot -- never unpossesses or ChangeState),
			// NOT the pawn. The canonical Lyra "reset everyone" (AGameModeBase::ResetLevel) also resets the PAWN:
			// ALyraCharacter::Reset() -> UninitAndDestroy -> DetachFromControllerPendingDestroy (UnPossess +
			// ChangeState(NAME_Inactive)) + SetLifeSpan. Skipping it left a surviving BOT reaching
			// ServerRestartController NAME_Playing-with-a-pawn -> ensure((pawn==null)&&Inactive) trips (stack-walk
			// + ~5s hitch), AND the Inactive restart-guard skipped it -> the survivor never reset, keeping its
			// pawn/position into the next round (breaks the fresh-start round design). Mirror the canonical
			// teardown for EVERY survivor (bot AND human): Reset() the live pawn so the controller reaches the
			// restart pawn-null + Inactive -- the exact clean state a DEAD controller already reached via the
			// death flow's UninitAndDestroy. NO-OP for the usually-dead human (no pawn here) -> the proven
			// dead-human respawn path is untouched; this acts only on the rare ALIVE survivor (the case that
			// needs the fresh reset). Pawn->Reset() is a teardown, not a death -> fires no respawn ability; the
			// PlayerState ASC + its State.Round.NoRespawn suppression tag persist across it (one restart = one pawn).
			if (APawn* OldPawn = C->GetPawn())
			{
				OldPawn->Reset();   // ALyraCharacter::Reset -> UninitAndDestroy -> pawn-null + NAME_Inactive
			}
			GM->RequestPlayerRestartNextFrame(C, /*bForceReset=*/true);
		}
	}
}

int32 UAFLRoundManagerComponent::AliveCount(int32 TeamId) const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	const ULyraTeamSubsystem* Teams = GetWorld() ? GetWorld()->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	if (!GS || !Teams)
	{
		return 0;
	}
	int32 Count = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS || Teams->FindTeamFromObject(PS) != TeamId)
		{
			continue;
		}
		if (const APawn* P = PS->GetPawn())
		{
			if (const ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(P))
			{
				if (!HC->IsDeadOrDying())
				{
					++Count;
				}
			}
		}
	}
	return Count;
}

int32 UAFLRoundManagerComponent::TeamHoldingCore() const
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	const ULyraTeamSubsystem* Teams = GetWorld() ? GetWorld()->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	if (!GS || !Teams)
	{
		return INDEX_NONE;
	}
	const FGameplayTag Extracting = TAG_State_Extracting_Round;
	bool bSlot0 = false;
	bool bSlot1 = false;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		const APawn* P = PS->GetPawn();
		if (!P) { continue; }
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(P);
		if (!ASC || !ASC->HasMatchingGameplayTag(Extracting)) { continue; }
		const int32 Slot = SlotForTeam(Teams->FindTeamFromObject(PS));
		if (Slot == 0) { bSlot0 = true; }
		else if (Slot == 1) { bSlot1 = true; }
	}
	if (bSlot0 && !bSlot1) { return ParticipatingTeams[0]; }
	if (bSlot1 && !bSlot0) { return ParticipatingTeams[1]; }
	return INDEX_NONE;   // none or both channeling -> Replay
}

bool UAFLRoundManagerComponent::BindDeathDelegateForPawn(APawn* Pawn)
{
	ULyraHealthComponent* HC = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
	if (!HC || BoundHealthComps.Contains(HC))
	{
		return false;   // THE GUARD. Two paths reach here (round-start reconcile + per-possession join);
		                // AddDynamic is not idempotent, and a double bind would count each death twice.
	}
	HC->OnDeathStarted.AddDynamic(this, &UAFLRoundManagerComponent::HandlePlayerDeath);
	BoundHealthComps.Add(HC);
	return true;
}

void UAFLRoundManagerComponent::BindDeathDelegates()
{
	// KEPT, not removed, now that ApplyJoinStateToPawn binds per possession. This is a full reconcile at a
	// known-good moment: it drops stale weak entries from destroyed pawns and guarantees round-start
	// correctness even if a possession was never observed (a pawn possessed before this component's
	// BeginPlay, or a controller that never travelled OnGameModePlayerInitialized). Making round-start
	// depend solely on delegate coverage, with no backstop, is the single-mechanism fragility that
	// produced this whole bug class. It is cheap and idempotent -- keep the belt with the braces.
	UnbindDeathDelegates();
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return;
	}
	int32 Bound = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		if (BindDeathDelegateForPawn(PS->GetPawn()))
		{
			++Bound;
		}
	}
	// The COUNT is the proof, same as the tag sweeps: a bind count below the live population means a
	// pawn's death will never reach AliveCount and the round cannot resolve on elimination.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: death delegates reconciled -- %d pawn(s) bound of %d player(s)."),
		Bound, GS->PlayerArray.Num());
}

void UAFLRoundManagerComponent::ApplyJoinStateToPlayer(AController* NewPlayer, UAbilitySystemComponent* PlayerStateASC)
{
	if (!PlayerStateASC)
	{
		return;
	}
	// SITE #4. Cached, not inferred: there is no live query for "is respawn suppressed right now", and
	// deriving it from Phase would go wrong across the between-rounds window. Set-count, not Add.
	PlayerStateASC->SetLooseGameplayTagCount(TAG_State_Round_NoRespawn, bRespawnSuppressed ? 1 : 0);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_JOIN: round state applied to %s -- NoRespawn=%d (phase %d)."),
		*GetNameSafe(NewPlayer), bRespawnSuppressed ? 1 : 0, static_cast<int32>(Phase));
}

void UAFLRoundManagerComponent::ApplyJoinStateToPawn(AController* Controller, APawn* NewPawn)
{
	// SITE #3. Fires on join AND on every respawn, so a pawn created mid-round -- exactly the case the
	// round-start reconcile could not see -- is bound the moment it is possessed.
	if (BindDeathDelegateForPawn(NewPawn))
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_JOIN: death delegate bound on %s (controller %s)."),
			*GetNameSafe(NewPawn), *GetNameSafe(Controller));
	}
}

void UAFLRoundManagerComponent::UnbindDeathDelegates()
{
	for (TWeakObjectPtr<ULyraHealthComponent>& Weak : BoundHealthComps)
	{
		if (ULyraHealthComponent* HC = Weak.Get())
		{
			HC->OnDeathStarted.RemoveDynamic(this, &UAFLRoundManagerComponent::HandlePlayerDeath);
		}
	}
	BoundHealthComps.Reset();
}

void UAFLRoundManagerComponent::SetPhaseAuthoritative(EAFLRoundPhase NewPhase)
{
	Phase = NewPhase;
	OnRep_Phase();   // OnRep does not fire for the authority's own change -> drive the listen-host locally
}

void UAFLRoundManagerComponent::EmitRoundTelemetry(int32 WinningTeamId, EAFLRoundWinReason Reason) const
{
	static const TCHAR* ReasonNames[] = { TEXT("elimination"), TEXT("extraction"), TEXT("timeout"), TEXT("replay") };
	const int32 Idx = static_cast<int32>(Reason);
	const FName ReasonName((Idx >= 0 && Idx < UE_ARRAY_COUNT(ReasonNames)) ? ReasonNames[Idx] : TEXT("unknown"));
	FAFLCombatTelemetry::EmitRoundResolved(CurrentRound, WinningTeamId, ReasonName);
}

void UAFLRoundManagerComponent::OnRep_Phase()
{
	// Replicated Phase is BlueprintReadOnly -- the HUD reads it (and may bind this OnRep in a BP child).
}

void UAFLRoundManagerComponent::OnRep_Score()
{
	// Replicated Team0/1Score are BlueprintReadOnly -- the HUD reads them.
}

void UAFLRoundManagerComponent::OnRep_RoundResolved()
{
	OnRoundResolved.Broadcast(LastWinningTeam, LastWinReason);
}

void UAFLRoundManagerComponent::OnRep_MatchId()
{
	// A1.3b proof (client-side): the server-authored MatchId replicated in. OnRep fires on the change from the
	// invalid default to the authored guid, so this logs the real match id once. The operator asserts this ==
	// the server's "assigned" line. (Server-side reads use GetMatchId() directly, not this OnRep.)
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_A13B_MATCHID replicated %s"), *GetMatchId());
}

#if !UE_BUILD_SHIPPING
// Dev trigger for the PIE watch (host-side: the listen-server console runs on the authority world). The
// production trigger (the match-phase Playing entry calling ServerStartMatch) is Task 2.
static FAutoConsoleCommandWithWorld GAFLRoundStartCmd(
	TEXT("afl.Round.Start"),
	TEXT("Start the Arena round FSM (ServerStartMatch on the authority GameState's round manager)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) { return; }
		if (AGameStateBase* GS = World->GetGameState())
		{
			if (UAFLRoundManagerComponent* RM = GS->FindComponentByClass<UAFLRoundManagerComponent>())
			{
				RM->ServerStartMatch();
			}
		}
	}));
#endif
