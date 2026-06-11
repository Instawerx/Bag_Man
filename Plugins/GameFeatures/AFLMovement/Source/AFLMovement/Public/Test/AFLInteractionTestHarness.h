// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLInteractionTestHarness.generated.h"

class AActor;
class APawn;
class APlayerController;
class UAbilitySystemComponent;
class ULyraAbilitySystemComponent;
class UAFLGrabbableComponent;
class UAFLInteractionComponent;

/**
 * UAFLInteractionTestHarness  (stress-object cycle -- automated PIE interaction test)
 *
 * Cheat-driven FSM behind `afl.Interaction.Test.Run` (dotted console command, the afl.HandIK.*
 * registration pattern). One run scripts the P0-P4 must-passes of the verb set + carrier
 * vulnerability at ~1.5s step pacing so the operator can WATCH each verb while the log accrues
 * machine-checkable verdicts:
 *
 *   AFL_TEST[Pn] PASS|FAIL -- <check>     per assertion
 *   AFL_TEST SUMMARY ...                  per-phase rollup
 *   AFL_TEST COMPLETE                     end marker (operator stops PIE after this)
 *
 * Honesty contract (the false-green lessons): every check is asserted off live GAS/world state,
 * never off "the command was issued". Phases the harness cannot script are emitted as SKIP with
 * the manual instruction (climb-forced drop needs a wall; P5 sphere look is operator-visual).
 *
 * REAL-PIPELINE rules (mirrors the lint rails):
 *  - Grab/drop/throw are driven through ULyraAbilitySystemComponent::AbilityInputTagPressed/
 *    Released (the literal input seam ProcessAbilityInput consumes) -- never TryActivate shortcuts,
 *    never direct GrabActor/ReleaseActor.
 *  - Damage goes through the established AFL.Combat.Damage console cheat (GE_AFL_Damage_Pulse ->
 *    the real UAFLDamageExecCalc), issued via PlayerController::ConsoleCommand. Health resets use
 *    the established SetCombatAttribute cheat. The harness applies NO effects itself.
 *  - Attribute READS are stringly (walk the spawned attribute sets for "Health") -- test-scoped,
 *    read-only, keeps AFLMovement free of an AFLCombat link dep. Writes stay in the cheats.
 */
UCLASS()
class AFLMOVEMENT_API UAFLInteractionTestHarness : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Entry point for the console command. Refuses to start while a prior run is live. */
	static void RunInWorld(UWorld* World);

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return bRunning; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return WorldPtr.Get(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLInteractionTestHarness, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	struct FStep
	{
		FString Label;
		TFunction<void()> Action;
	};

	/** Resolve world refs + classify grabbables; build the step list; root self. */
	bool StartRun(UWorld* World);
	void FinishRun();

	// -- step-list construction (one builder per phase keeps the script readable) --
	void BuildSteps();

	// -- primitives --
	void PressTag(const FName TagName);                  // press+release same frame (the real IA trigger shape)
	void Console(const FString& Cmd);                    // PC->ConsoleCommand (cheat reuse: damage / attr reset)
	void PlaceInFront(AActor* Target);                   // teleport target ~150cm ahead of the pawn (zero velocity)
	void Park(AActor* Target);                           // teleport target far behind the pawn (out of grab reach)
	void ParkAll();
	bool IsCarryingNow() const;                          // interaction comp + State.Carrying tag agreement
	bool HasTag(const FName TagName) const;
	float ReadHealth(bool& bOutFound) const;             // stringly read, see class comment
	void Check(const TCHAR* Phase, bool bPass, const FString& What);
	void Skip(const TCHAR* Phase, const FString& Why);
	void Banner(const FString& Msg) const;               // log + on-screen step narration

	// -- run state --
	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<ULyraAbilitySystemComponent> LyraASC;
	TWeakObjectPtr<UAFLInteractionComponent> Interaction;

	// classified world objects (VULN = CarrierEffectClass set; NODROP = !bDropOnDamage; PLAIN = rest)
	TWeakObjectPtr<AActor> VulnActor;
	TWeakObjectPtr<AActor> NoDropActor;
	TWeakObjectPtr<AActor> PlainActor;

	TArray<FStep> Steps;
	int32 StepIndex = 0;
	float Accum = 0.0f;
	bool bRunning = false;

	// scratch between steps
	float HealthBefore = 0.0f;
	bool bRecoveryTagObserved = false;                   // snapshotted 0.2s after the P4 throw (window is 0.4s)

	// per-phase rollup for SUMMARY
	TMap<FString, int32> PhaseTotals;
	TMap<FString, int32> PhaseFails;
	TArray<FString> PhaseOrder;

	static constexpr float StepInterval = 1.5f;

	/** The single live run (one harness at a time; cleared at FinishRun). */
	static TWeakObjectPtr<UAFLInteractionTestHarness> ActiveRun;
};
