// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"

#include "AFLMatchPopulationComponent.generated.h"

class AController;
class AGameModeBase;
class ALyraGameMode;
class APawn;
class UAbilitySystemComponent;

/**
 * UAFLMatchPopulationComponent -- JOIN COVERAGE for match state written to a POPULATION.
 *
 * THE BUG CLASS THIS EXISTS TO CLOSE. A GameState component that writes state across "everyone who
 * exists" (a loose tag on every ASC, a delegate bound on every pawn) does that write ONCE, at a phase
 * edge. Anyone arriving afterwards is silently uncovered. That is not one bug -- it was FIVE sites
 * across UAFLMatchPhaseComponent + UAFLRoundManagerComponent, all keyed off present membership, none
 * with a join hook:
 *   #1 State.Mode.NoDismember  -- a late joiner never got ProMod's clean-health gate, so
 *                                 UAFLDamageExecCalc kept zone-routing them (the 4b89557b gate)
 *   #2 State.Match.Warmup      -- the reported symptom: WARMUP logged at :153, first join at :171,
 *                                 so the sweep covered ONLY the level-placed target dummy
 *   #3 OnDeathStarted bind     -- a pawn spawning after round start never counted toward AliveCount
 *   #4 State.Round.NoRespawn   -- late joiner keeps auto-respawning mid-round
 *   #5 State.Match.Ended       -- same shape as #2, at the far end of the spine
 *
 * TWO STAGES, because those five split 4/1 on what they need:
 *   STAGE 1  ALyraGameMode::OnGameModePlayerInitialized -> AController.
 *            The PlayerState (and its ASC) is live. THE PAWN IS NOT GUARANTEED: Server_ResetRoundActors
 *            defers through RequestPlayerRestartNextFrame, so controllers exist without pawns for a
 *            real frame. Everything ASC-shaped happens here, on the PLAYERSTATE ASC -- which is where
 *            Lyra keeps it, so the write survives respawn for free.
 *   STAGE 2  AController::OnPossessedPawnChanged, subscribed per-controller during stage 1.
 *            Anything needing the pawn itself. Fires on EVERY possession, so respawn is covered by the
 *            same mechanism rather than a second one.
 *
 * WHY THESE TWO DELEGATES (the ASC-spawn-race lesson: a hook with a working call site in THIS repo
 * beats a framework-correct one with none):
 *   OnGameModePlayerInitialized -- UAFLTeamCreationComponent.cpp:73, UAFLBotFillComponent.cpp:113.
 *                                  Bots and humans both travel it; both logged in the diagnosing session.
 *   OnPossessedPawnChanged      -- UAFLCharacterPartSelectorComponent.cpp:28,
 *                                  UAFLSkinColorControllerComponent.cpp:66, UAFLCarriedValueWidget.cpp:29.
 *
 * THE ONE-SHOT SWEEPS STAY. They are not redundant with this: B_AFL_TargetDummy_C_0 is level-placed,
 * carries its own ASC, and NEVER passes through OnGameModePlayerInitialized. It is what the clean-health
 * gate was proven against. Sweep = everyone present (incl. non-player pawns); this = everyone arriving.
 * Both write with SetLooseGameplayTagCount so overlapping on a real player is a no-op, not a refcount
 * of 2 that survives one removal.
 *
 * ORDERING INVARIANT -- see the comment at each transition site. EVERY TRANSITION UPDATES ITS STATE
 * BEFORE IT SWEEPS, and every join write derives from a live query or an already-updated cache.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLMatchPopulationComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UAFLMatchPopulationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** STAGE 1. A controller finished initialising; its PlayerState ASC is live, its pawn may not be.
	 *  Apply every ASC-shaped write here, deriving each from state that is TRUE NOW -- a live query
	 *  (IsPhaseActiveReflected) where one exists, an already-updated cache otherwise. Never replay a
	 *  history of transitions. Use SetLooseGameplayTagCount, never AddLooseGameplayTag: this can run
	 *  after a sweep already covered the same ASC, and a refcount of 2 outlives one removal. */
	virtual void ApplyJoinStateToPlayer(AController* NewPlayer, UAbilitySystemComponent* PlayerStateASC) {}

	/** STAGE 2. A pawn was possessed -- on join, and again on every respawn. Apply anything that needs
	 *  the pawn (components live on it, not the PlayerState). GUARD YOUR BINDS: AddDynamic is NOT
	 *  idempotent, and this legitimately runs more than once per controller. */
	virtual void ApplyJoinStateToPawn(AController* Controller, APawn* NewPawn) {}

private:
	void HandleGameModePlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer);

	/** THE OTHER HALF OF STAGE 1 -- everyone who was ALREADY THERE when we bound.
	 *  OnGameModePlayerInitialized only covers arrivals AFTER the subscription. On a listen server the
	 *  HOST's controller is initialised during map load, so its broadcast has already happened by the
	 *  time this component's BeginPlay runs -- log-proven: five AFL_JOIN lines for the bots, none for
	 *  LyraPlayerController_0, and the host fired freely 5s into a 30s warmup. The warmup sweep cannot
	 *  rescue it either; that ran 54ms before the host was even assigned a team.
	 *  Idempotent (Set-count + guarded binds), so it composes safely with the sweeps and the join hook.
	 *  Bounded retry because the host's PlayerState can exist before its ASC resolves -- the same
	 *  "not ready yet" poll UAFLW_RoundHeader::TryArm uses. */
	void ReconcileExistingPopulation();

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** Controllers we have subscribed to, so a re-init cannot double-subscribe and EndPlay can unwind
	 *  cleanly -- GameFeature deactivation tears this component down while controllers outlive it. */
	TArray<TWeakObjectPtr<AController>> BoundControllers;

	TWeakObjectPtr<ALyraGameMode> BoundGameMode;
	FDelegateHandle PlayerInitializedHandle;

	FTimerHandle ReconcileRetryTimer;
	int32 ReconcileAttempts = 0;
};
