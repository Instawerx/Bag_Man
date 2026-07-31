// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_WallRun.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;
class UAFLWallRunMovementComponent;

/**
 * UAFLGameplayAbility_WallRun  (Movement Overhaul — Phase 2: contextual horizontal wall-run)
 *
 * The MOTION-lifecycle half of wall-run, mirroring GA_AFL_Climb. CONTEXTUAL — activated by the
 * Event.Movement.WallRun.Detected gameplay event that UAFLWallRunMovementComponent fires when the airborne
 * pawn finds a side wall (no keypress; see AbilityTriggers). On activate: confirm+orient to the wall, apply
 * WallRunActiveEffectClass (grants State.Movement.WallRunning -> the component flips gravity-0/flying and
 * drives velocity along the wall tangent), play a looping wall-run montage, and exit on the component's
 * OnWallSurfaceLost, a max-duration timeout, or a wall-jump (which launches the pawn off the wall -> the
 * component loses the surface next tick -> we exit). Wall-jump itself is UAFLGameplayAbility_WallJump.
 *
 * v1 = horizontal wall-run only (vertical wall-climb is deferred; Climb already handles climb-up-to-ledge).
 * Abstract; the GA_AFL_WallRun BP child sets the GE + montage.
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_WallRun : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_WallRun(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** Duration GE applied on activation — grants State.Movement.WallRunning (mirror GE_AFL_Climb_Active). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun")
	TSubclassOf<UGameplayEffect> WallRunActiveEffectClass;

	/** Looping wall-run montage (in-place run cycle, body tilted toward the wall; the component drives velocity). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun")
	TObjectPtr<UAnimMontage> WallRunMontage;

	/** Hard cap (s) on a single wall-run so the pawn can't ride a wall forever. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun")
	float MaxWallRunDuration = 2.0f;

	/** Side reach (cm) for the confirm+orient trace on activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun")
	float DetectSideDistance = 70.0f;

private:
	UFUNCTION()
	void OnMontageCompleted();
	UFUNCTION()
	void OnMontageInterruptedOrCancelled();

	void OnSurfaceLost();
	void OnTimeout();
	void ExitWallRun(const TCHAR* Reason, bool bCancelled);

	/** Confirm a side wall is present and compute the facing to orient the pawn along it. */
	bool FindWall(FRotator& OutFacing) const;

	UPROPERTY()
	TWeakObjectPtr<UAFLWallRunMovementComponent> WallRunComponent;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FDelegateHandle SurfaceLostHandle;
	FTimerHandle TimeoutHandle;
	bool bExiting = false;
};
