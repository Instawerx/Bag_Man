// Copyright C12 AI Gaming. All Rights Reserved.

#include "BattleRoyale/AFLBattleRoyaleComponent.h"

#include "AFLCombat.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"   // observe AFL.GamePhase.Playing -> ServerStartMatch (the proven phase-observer path)
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Phases/AFLMatchPhaseComponent.h"
#include "Telemetry/AFLCombatTelemetry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBattleRoyaleComponent)

// Same no-respawn tag the round manager uses: the cloned GA_AFL_AutoRespawn skips its respawn node while
// this is on the owning (PlayerState) ASC. UE dedups native+ini; AFLCombatTags.ini is the spec source.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Round_NoRespawn_BR, "State.Round.NoRespawn");

// The match-phase "Playing" tag (UAFLMatchPhaseComponent starts it at the Warmup->Playing edge). Its driver
// tag is file-local to that .cpp, so define our own static for the same string (UE dedups) and observe it
// WITHOUT linking the driver's symbol -- exactly how AAFLExtractionZone / the round manager do it.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Playing_BR, "AFL.GamePhase.Playing");

UAFLBattleRoyaleComponent::UAFLBattleRoyaleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Event-driven (deaths + phase start); no per-frame work in the spike -> never tick. The zone system
	// (S2) will own match time, not this component.
	PrimaryComponentTick.bCanEverTick = false;
	// Replicates its state to drive the HUD (sibling round manager does the same; the match-phase driver is server-only).
	SetIsReplicatedByDefault(true);
}

void UAFLBattleRoyaleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, MatchId);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, Phase);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, AlivePlayers);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, TotalParticipants);
	DOREPLIFETIME(UAFLBattleRoyaleComponent, WinnerPlayerId);
}

bool UAFLBattleRoyaleComponent::HasAuth() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UAFLBattleRoyaleComponent::BeginPlay()
{
	Super::BeginPlay();

	// Clients only consume the replicated state via OnRep -- mirror the sibling authority gate.
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		return;
	}

	SetPhaseAuthoritative(EAFLBRPhase::WarmUp);

	// Auto-start on the natural Warmup->Playing transition (no cheat needed). There is no delegate on
	// UAFLMatchPhaseComponent to bind (EnterPlaying is private + broadcasts nothing); the transition is
	// observable ONLY as the AFL.GamePhase.Playing phase-start on the Lyra GamePhaseSubsystem. Register the
	// SAME reflective observer AAFLExtractionZone/round manager use (THE LYRA PHASE WALL: the C++ WhenPhase*
	// overloads aren't LYRAGAME_API + the K2_ UFUNCTIONs are protected -> bind via ProcessEvent). ExactMatch
	// -> fires once on the Playing entry (not the .ExtractionWindow child). Already past the authority gate.
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
			Params.PhaseTag = TAG_AFL_GamePhase_Playing_BR;
			Params.MatchType = EPhaseTagMatchType::ExactMatch;
			Params.WhenPhase.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UAFLBattleRoyaleComponent, HandlePlayingPhaseActive));
			PhaseSub->ProcessEvent(Fn, &Params);
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR bind ok -- observing AFL.GamePhase.Playing -> ServerStartMatch."));
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BR bind FAILED -- K2_WhenPhaseStartsOrIsActive not found (fallback: afl.BR.Start)."));
		}
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BR bind SKIP -- no ULyraGamePhaseSubsystem in this world (fallback: afl.BR.Start)."));
	}
}

void UAFLBattleRoyaleComponent::HandlePlayingPhaseActive(const FGameplayTag& PhaseTag)
{
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR Playing phase active (tag=%s) -> ServerStartMatch."), *PhaseTag.ToString());
	ServerStartMatch();
}

void UAFLBattleRoyaleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindDeathDelegates();
	Super::EndPlay(EndPlayReason);
}

void UAFLBattleRoyaleComponent::ServerStartMatch()
{
	if (!HasAuth() || bMatchStarted)
	{
		return;
	}

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_BR: cannot START -- no GameState. Aborting (retry once it exists)."));
		return;   // abort WITHOUT marking started -- a later call retries
	}

	// SOLO participants = every PlayerState present at Playing entry (human + bots; pawns are spawned by now).
	TotalParticipants = GS->PlayerArray.Num();
	if (TotalParticipants < 2)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BR: starting with %d participant(s) -- last-standing is degenerate below 2."), TotalParticipants);
	}
	NextPlacement = TotalParticipants;

	bMatchStarted = true;
	MatchId = FGuid::NewGuid();   // authored ONCE, past the guard -> stable staking/earn contract id
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR_MATCHID assigned %s"), *GetMatchId());

	Placements.Reset();
	SetPhaseAuthoritative(EAFLBRPhase::Playing);
	AlivePlayers = AliveParticipants();
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: match START (participants=%d, alive=%d, survivorsToWin=%d, id=%s)."),
		TotalParticipants, AlivePlayers, SurvivorsToWin, *GetMatchId());

	// No-respawn for the whole match: dead is out. State.Round.NoRespawn on every PlayerState ASC (persists
	// across pawns) + ShouldBlockRestart() -> AAFLGameMode::ControllerCanRestart. Both, belt-and-braces.
	SetRespawnBlocked(true);

	// BR ends on last-standing, NOT the match-phase 480s clock -- take external match-end authority on the
	// resident match-phase component (present for the warmup->playing spine), leaving its cadence untouched.
	if (UAFLMatchPhaseComponent* MatchPhase = GS->FindComponentByClass<UAFLMatchPhaseComponent>())
	{
		MatchPhase->SetExternalMatchEndAuthority(true);
	}

	// Bind death on the now-live pawns (the per-possession join hook covers anyone arriving later).
	BindDeathDelegates();
}

void UAFLBattleRoyaleComponent::HandlePlayerDeath(AActor* OwningActor)
{
	if (!HasAuth() || Phase != EAFLBRPhase::Playing)
	{
		return;
	}

	// Resolve the victim's PlayerState (the stable participant identity across its dead pawn).
	APlayerState* VictimPS = nullptr;
	if (const APawn* VictimPawn = Cast<APawn>(OwningActor))
	{
		VictimPS = VictimPawn->GetPlayerState();
	}

	// Book the finishing place (guarded: OnDeathStarted must count each participant once).
	if (VictimPS && !Placements.Contains(VictimPS))
	{
		Placements.Add(VictimPS, NextPlacement);
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: %s eliminated -> placement %d."), *GetNameSafe(VictimPS), NextPlacement);
		NextPlacement = FMath::Max(1, NextPlacement - 1);
	}

	// Per-kill spatial telemetry (solo -> team INDEX_NONE), reusing the proven combat telemetry sink.
	const FVector Loc = OwningActor ? OwningActor->GetActorLocation() : FVector::ZeroVector;
	FAFLCombatTelemetry::EmitElimination(OwningActor, /*Killer=*/nullptr, /*VictimTeam=*/INDEX_NONE, Loc);

	// Recompute alive authoritatively (the just-dead pawn already reads IsDeadOrDying).
	APlayerState* LastAlive = nullptr;
	AlivePlayers = AliveParticipants(&LastAlive);

	if (AlivePlayers <= SurvivorsToWin)
	{
		// Last-standing (1 survivor) -> that PlayerState wins (placement 1); 0 survivors -> draw (null winner).
		Server_EndMatch(AlivePlayers == 1 ? LastAlive : nullptr);
	}
}

void UAFLBattleRoyaleComponent::Server_EndMatch(APlayerState* Winner)
{
	UnbindDeathDelegates();

	if (Winner && !Placements.Contains(Winner))
	{
		Placements.Add(Winner, 1);   // sole survivor takes first place
	}
	WinnerPlayerId = Winner ? Winner->GetPlayerId() : INDEX_NONE;

	SetPhaseAuthoritative(EAFLBRPhase::MatchEnd);
	OnRep_Resolved();   // listen-host local broadcast (OnRep does not fire for the authority's own change)

	FAFLCombatTelemetry::EmitRoundResolved(/*Round=*/0, WinnerPlayerId, FName(TEXT("last_standing")));
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: MATCH END -- winner %s (playerId=%d), participants=%d -> concluding."),
		*GetNameSafe(Winner), WinnerPlayerId, TotalParticipants);

	// Conclude via the PROVEN PostGame machinery on the resident match-phase component (freeze via
	// State.Match.Ended + PostGame + per-player Watts banner). Idempotent (bMatchEnded). Null-guarded.
	if (const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr)
	{
		if (UAFLMatchPhaseComponent* MatchPhase = GS->FindComponentByClass<UAFLMatchPhaseComponent>())
		{
			MatchPhase->ConcludeMatch();
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BR: MATCH END but no UAFLMatchPhaseComponent resident -- conclusion (freeze/PostGame/Watts) SKIPPED."));
		}
	}
	// NOTE: respawn stays blocked (BR = permadeath); PostGame freeze holds until the match tears down.
}

int32 UAFLBattleRoyaleComponent::AliveParticipants(APlayerState** OutLastAlive) const
{
	if (OutLastAlive) { *OutLastAlive = nullptr; }
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return 0;
	}
	int32 Count = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		const APawn* P = PS->GetPawn();
		if (!P) { continue; }
		const ULyraHealthComponent* HC = ULyraHealthComponent::FindHealthComponent(P);
		if (HC && !HC->IsDeadOrDying())
		{
			++Count;
			if (OutLastAlive) { *OutLastAlive = PS; }
		}
	}
	return Count;
}

int32 UAFLBattleRoyaleComponent::GetPlacementForPlayer(const APlayerState* PS) const
{
	if (!PS) { return 0; }
	const int32* Found = Placements.Find(PS);
	return Found ? *Found : 0;
}

void UAFLBattleRoyaleComponent::SetRespawnBlocked(bool bBlocked)
{
	// ORDERING INVARIANT (from the round manager): CACHE BEFORE SWEEPING. bRespawnBlocked is the source of
	// truth for every later joiner (site #4 has no live query) AND for ShouldBlockRestart -- set it first.
	bRespawnBlocked = bBlocked;

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return;
	}
	int32 Count = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
		{
			ASC->SetLooseGameplayTagCount(TAG_State_Round_NoRespawn_BR, bBlocked ? 1 : 0);
			++Count;
		}
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: respawn %s on %d player ASC(s) (State.Round.NoRespawn)."),
		bBlocked ? TEXT("BLOCKED") : TEXT("RESTORED"), Count);
}

bool UAFLBattleRoyaleComponent::BindDeathDelegateForPawn(APawn* Pawn)
{
	ULyraHealthComponent* HC = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
	if (!HC || BoundHealthComps.Contains(HC))
	{
		return false;   // THE GUARD -- AddDynamic is not idempotent; a double bind double-counts a death.
	}
	HC->OnDeathStarted.AddDynamic(this, &UAFLBattleRoyaleComponent::HandlePlayerDeath);
	BoundHealthComps.Add(HC);
	return true;
}

void UAFLBattleRoyaleComponent::BindDeathDelegates()
{
	// Full reconcile at match start: drop stale weak entries + guarantee coverage even for a pawn possessed
	// before this component's BeginPlay. The per-possession join hook (ApplyJoinStateToPawn) covers arrivals.
	UnbindDeathDelegates();
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	if (!GS)
	{
		return;
	}
	int32 Bound = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (PS && BindDeathDelegateForPawn(PS->GetPawn()))
		{
			++Bound;
		}
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: death delegates reconciled -- %d pawn(s) bound of %d participant(s)."),
		Bound, GS->PlayerArray.Num());
}

void UAFLBattleRoyaleComponent::UnbindDeathDelegates()
{
	for (TWeakObjectPtr<ULyraHealthComponent>& Weak : BoundHealthComps)
	{
		if (ULyraHealthComponent* HC = Weak.Get())
		{
			HC->OnDeathStarted.RemoveDynamic(this, &UAFLBattleRoyaleComponent::HandlePlayerDeath);
		}
	}
	BoundHealthComps.Reset();
}

void UAFLBattleRoyaleComponent::ApplyJoinStateToPlayer(AController* NewPlayer, UAbilitySystemComponent* PlayerStateASC)
{
	if (!PlayerStateASC)
	{
		return;
	}
	// SITE #4 -- cached, not inferred. Set-count, not Add (may run after a sweep already covered this ASC).
	PlayerStateASC->SetLooseGameplayTagCount(TAG_State_Round_NoRespawn_BR, bRespawnBlocked ? 1 : 0);
}

void UAFLBattleRoyaleComponent::ApplyJoinStateToPawn(AController* Controller, APawn* NewPawn)
{
	// SITE #3 -- fires on join AND every respawn; binds a pawn created mid-match the moment it is possessed.
	if (BindDeathDelegateForPawn(NewPawn))
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR: death delegate bound on %s (controller %s)."),
			*GetNameSafe(NewPawn), *GetNameSafe(Controller));
	}
}

void UAFLBattleRoyaleComponent::SetPhaseAuthoritative(EAFLBRPhase NewPhase)
{
	Phase = NewPhase;
	OnRep_Phase();   // OnRep does not fire for the authority's own change -> drive the listen-host locally
}

void UAFLBattleRoyaleComponent::OnRep_Phase()   { /* BlueprintReadOnly -- HUD reads Phase (may bind in a BP child). */ }
void UAFLBattleRoyaleComponent::OnRep_MatchId() { UE_LOG(LogAFLCombat, Log, TEXT("AFL_BR_MATCHID replicated %s"), *GetMatchId()); }
void UAFLBattleRoyaleComponent::OnRep_Resolved(){ OnBattleRoyaleResolved.Broadcast(nullptr); /* winner identity via WinnerPlayerId; PS ptr not replicated */ }

#if !UE_BUILD_SHIPPING
// Dev trigger for the PIE watch (host-side authority world). Production trigger = the match-phase Playing entry.
static FAutoConsoleCommandWithWorld GAFLBRStartCmd(
	TEXT("afl.BR.Start"),
	TEXT("Start the Battle Royale FSM (ServerStartMatch on the authority GameState's BR component)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World) { return; }
		if (AGameStateBase* GS = World->GetGameState())
		{
			if (UAFLBattleRoyaleComponent* BR = GS->FindComponentByClass<UAFLBattleRoyaleComponent>())
			{
				BR->ServerStartMatch();
			}
		}
	}));
#endif
