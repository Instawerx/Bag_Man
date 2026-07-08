// Copyright C12 AI Gaming. All Rights Reserved.

#include "Round/AFLRoundManagerComponent.h"

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
	OnRep_Score();   // listen-host local HUD
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: match START (teams %d v %d; first to %d; half-swap after round %d)."),
		ParticipatingTeams[0], ParticipatingTeams[1], RoundsToWin, HalfTimeAfterRound);

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
	if (!HasAuth())
	{
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

	LastWinningTeam = WinningTeamId;
	LastWinReason = Reason;
	OnRep_Score();            // listen-host local HUD
	OnRep_RoundResolved();    // listen-host toast + server-side OnRoundResolved binds
	EmitRoundTelemetry(WinningTeamId, Reason);
	SetPhaseAuthoritative(EAFLRoundPhase::RoundEnd);

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: round %d RESOLVED -- winner team %d, reason %s. Score %d-%d."),
		CurrentRound, WinningTeamId, *UEnum::GetValueAsString(Reason), Team0Score, Team1Score);

	if (Team0Score >= RoundsToWin) { Server_EndMatch(ParticipatingTeams[0]); return; }
	if (Team1Score >= RoundsToWin) { Server_EndMatch(ParticipatingTeams[1]); return; }

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ResetTimerHandle, this, &UAFLRoundManagerComponent::Server_BetweenRounds,
			FMath::Max(0.5f, RoundResetCountdown), /*loop=*/false);
	}
}

void UAFLRoundManagerComponent::Server_BetweenRounds()
{
	if (!HasAuth())
	{
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
}

void UAFLRoundManagerComponent::SetRoundRespawnSuppressed(bool bSuppressed)
{
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

void UAFLRoundManagerComponent::BindDeathDelegates()
{
	UnbindDeathDelegates();
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return;
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		if (const APawn* P = PS->GetPawn())
		{
			if (ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(P))
			{
				HC->OnDeathStarted.AddDynamic(this, &UAFLRoundManagerComponent::HandlePlayerDeath);
				BoundHealthComps.Add(HC);
			}
		}
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
