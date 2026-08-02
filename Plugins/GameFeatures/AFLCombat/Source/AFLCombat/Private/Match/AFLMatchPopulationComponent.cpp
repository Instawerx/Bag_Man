// Copyright C12 AI Gaming. All Rights Reserved.

#include "Match/AFLMatchPopulationComponent.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchPopulationComponent)

UAFLMatchPopulationComponent::UAFLMatchPopulationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAFLMatchPopulationComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server-only, matching both subclasses' own gate. Clients consume replicated state.
	AGameStateBase* GS = GetGameStateChecked<AGameStateBase>();
	if (!GS->HasAuthority())
	{
		return;
	}

	// Bind here, not on the experience-loaded delegate: this runs during game-feature activation and is
	// provably ahead of the first join (WARMUP logged at :153, first join at :171). Registering earlier
	// than we need costs nothing; registering later would reopen the gap this class exists to close.
	if (ALyraGameMode* GM = Cast<ALyraGameMode>(GS->AuthorityGameMode))
	{
		BoundGameMode = GM;
		PlayerInitializedHandle = GM->OnGameModePlayerInitialized.AddUObject(
			this, &UAFLMatchPopulationComponent::HandleGameModePlayerInitialized);
	}
	else
	{
		// Not fatal for the sweeps, but every late joiner is uncovered -- say so loudly rather than
		// leaving another silent population gap.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_JOIN: %s could not bind OnGameModePlayerInitialized (AuthorityGameMode is not an ALyraGameMode) -- LATE JOINERS WILL BE UNCOVERED."),
			*GetClass()->GetName());
	}

	// Catch anyone already initialised before we subscribed -- on a listen server, the HOST.
	//
	// DEFERRED BY ONE TICK, AND THAT IS LOAD-BEARING. This BeginPlay runs from the TOP of each subclass's
	// BeginPlay (they call Super:: first), so reconciling inline reads state the subclass has not
	// established yet. Measured failure: the host was reconciled with Warmup=0 at :064, one millisecond
	// BEFORE StartSpineFromWarmup() began warmup at :065; the reconcile settled on attempt 1 and never
	// ran again, so the host could fire for the entire 30s warmup while every bot was correctly blocked.
	// Next tick is strictly after every subclass BeginPlay has completed.
	// (Holds because both subclasses set their phase/round state synchronously within BeginPlay. If one
	// ever defers that to a timer or a latent load, this needs revisiting -- same caveat as the phase
	// transition ordering invariant.)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this] { ReconcileExistingPopulation(); }));
	}
}

void UAFLMatchPopulationComponent::ReconcileExistingPopulation()
{
	static constexpr int32 MaxAttempts = 40;      // x 0.25s = 10s, comfortably inside a 30s warmup
	static constexpr float RetryPeriod = 0.25f;

	++ReconcileAttempts;

	AGameStateBase* GS = GetGameState<AGameStateBase>();
	int32 Covered = 0;
	int32 Pending = 0;

	if (GS)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (!PS)
			{
				continue;
			}
			AController* C = PS->GetOwningController();
			UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
			if (!C || !ASC)
			{
				++Pending;   // PlayerState exists but the controller/ASC has not caught up -- retry.
				continue;
			}

			// Identical work to the join path, and idempotent, so running it on someone the delegate
			// already covered is a no-op rather than a double-apply.
			ApplyJoinStateToPlayer(C, ASC);
			if (!BoundControllers.Contains(C))
			{
				C->OnPossessedPawnChanged.AddDynamic(this, &UAFLMatchPopulationComponent::HandlePossessedPawnChanged);
				BoundControllers.Add(C);
			}
			if (APawn* ExistingPawn = C->GetPawn())
			{
				ApplyJoinStateToPawn(C, ExistingPawn);
			}
			++Covered;
		}
	}

	// Keep polling while anything is still unresolved, or while nobody has shown up at all -- on the very
	// first tick of a listen server PlayerArray is often still empty.
	const bool bSettled = (GS != nullptr) && (Pending == 0) && (Covered > 0);
	if (!bSettled && ReconcileAttempts < MaxAttempts)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(ReconcileRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { ReconcileExistingPopulation(); }),
				RetryPeriod, /*loop=*/false);
		}
		return;
	}

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_JOIN: %s reconciled pre-existing population -- %d covered, %d pending, %d attempt(s)."),
		*GetClass()->GetName(), Covered, Pending, ReconcileAttempts);
}

void UAFLMatchPopulationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReconcileRetryTimer);
	}
	if (ALyraGameMode* GM = BoundGameMode.Get())
	{
		GM->OnGameModePlayerInitialized.Remove(PlayerInitializedHandle);
	}
	PlayerInitializedHandle.Reset();
	BoundGameMode.Reset();

	// Controllers outlive a GameFeature deactivation, so a stale AddDynamic would fire into a dead
	// component. Mirrors UAFLRoundManagerComponent's BoundHealthComps unbind discipline.
	for (TWeakObjectPtr<AController>& Weak : BoundControllers)
	{
		if (AController* C = Weak.Get())
		{
			C->OnPossessedPawnChanged.RemoveDynamic(this, &UAFLMatchPopulationComponent::HandlePossessedPawnChanged);
		}
	}
	BoundControllers.Reset();

	Super::EndPlay(EndPlayReason);
}

void UAFLMatchPopulationComponent::HandleGameModePlayerInitialized(AGameModeBase* /*GameMode*/, AController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	// -- STAGE 1: the PlayerState ASC (Lyra keeps the ASC there, so this survives respawn). --
	if (APlayerState* PS = NewPlayer->PlayerState)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
		{
			ApplyJoinStateToPlayer(NewPlayer, ASC);
		}
	}

	// -- STAGE 2 subscription: every future possession, including every respawn. --
	if (!BoundControllers.Contains(NewPlayer))
	{
		NewPlayer->OnPossessedPawnChanged.AddDynamic(this, &UAFLMatchPopulationComponent::HandlePossessedPawnChanged);
		BoundControllers.Add(NewPlayer);
	}

	// The pawn may ALREADY be possessed by the time this hook runs (join order is not guaranteed, and
	// OnPossessedPawnChanged only covers transitions from here on). Same both-ends pattern as
	// UAFLCarriedValueWidget.cpp:25. ApplyJoinStateToPawn must therefore tolerate being called twice.
	if (APawn* ExistingPawn = NewPlayer->GetPawn())
	{
		ApplyJoinStateToPawn(NewPlayer, ExistingPawn);
	}
}

void UAFLMatchPopulationComponent::HandlePossessedPawnChanged(APawn* /*OldPawn*/, APawn* NewPawn)
{
	// NewPawn is null on unpossess (death teardown) -- nothing to cover.
	if (!NewPawn)
	{
		return;
	}
	if (AController* C = NewPawn->GetController())
	{
		ApplyJoinStateToPawn(C, NewPawn);
	}
}
