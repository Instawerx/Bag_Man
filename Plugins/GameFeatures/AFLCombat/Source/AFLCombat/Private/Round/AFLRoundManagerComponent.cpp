// Copyright C12 AI Gaming. All Rights Reserved.

#include "Round/AFLRoundManagerComponent.h"

#include "AFLCombat.h"
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
#include "Teams/LyraTeamSubsystem.h"
#include "Telemetry/AFLCombatTelemetry.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLRoundManagerComponent)

// The EXISTING extraction-complete message (UAFLAG_Extract broadcasts it server-side) + the channel
// state tag (the extract ability's ActivationOwnedTag). We only READ these -- no carry/extract edits.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Complete_Round, "Event.Extraction.Complete");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Extracting_Round, "State.Extracting");

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
	// The FSM waits for ServerStartMatch() -- called once pawns are live (the match-phase Playing entry /
	// the afl.Round.Start dev cheat). BeginPlay is too early to bind per-pawn death. Trigger = Task 2.
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
	CurrentRound = 0;
	Team0Score = 0;
	Team1Score = 0;
	bSidesSwapped = false;
	OnRep_Score();   // listen-host local HUD
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: match START (teams %d v %d; first to %d; half-swap after round %d)."),
		ParticipatingTeams[0], ParticipatingTeams[1], RoundsToWin, HalfTimeAfterRound);
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
	SetPhaseAuthoritative(EAFLRoundPhase::MatchEnd);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUND: MATCH END -- team %d wins %d-%d (terminal this build)."),
		WinningTeamId, Team0Score, Team1Score);
	// Terminal -- a rematch wrapper is future work, mirroring the match-phase PostGame HOLD.
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
