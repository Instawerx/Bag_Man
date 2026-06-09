// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_Climb.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;
class UAFLClimbMovementComponent;

/**
 * UAFLGameplayAbility_Climb  (P-CONTROLS climb, Option-γ two-layer split)
 *
 * The MOTION half of climb. THIS ability owns the movement (a root-motion climb montage via
 * AbilityTask_PlayMontageAndWait); the UAFLClimbMovementComponent owns the CMC STATE (gravity
 * off / MOVE_Flying while State.Movement.Climbing is held). Parented to ULyraGameplayAbility
 * (same as UAFLGameplayAbility_Dash), Local-predicted, server-validated. Abstract -- the
 * GA_AFL_Climb BP child sets ClimbActiveEffectClass + ClimbMontage.
 *
 * Lifecycle (gameplay-ai.md ability+montage pattern):
 *  ActivateAbility:
 *   1. Surface validation: forward LineTrace ~SurfaceTraceDistance from the pawn origin. No hit ->
 *      CancelAbility (input pressed but no wall). Hit -> proceed.
 *   2. Apply ClimbActiveEffectClass (grants State.Movement.Climbing -> the component flips CMC state).
 *   3. PlayMontageAndWait on ClimbMontage (root motion translates the character up the wall).
 *   4. Bind exits: montage OnCompleted (success, root motion placed the char at the ledge top),
 *      OnInterrupted/OnCancelled, the input-release listener (WaitInputRelease), and the component's
 *      OnClimbSurfaceLost delegate. Any of them -> EndAbility.
 *  EndAbility removes the active GE (tag clears -> component restores CMC state).
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_Climb : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_Climb(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

	/** Duration GE applied on activation -- grants State.Movement.Climbing (mirror GE_AFL_Dash_Active). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Climb")
	TSubclassOf<UGameplayEffect> ClimbActiveEffectClass;

	/** Root-motion climb montage. BP child sets it (placeholder linear-up now; AAA climb anim swaps in later). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Climb")
	TObjectPtr<UAnimMontage> ClimbMontage;

	/** Forward trace length (cm) for the climbable-surface validation on activate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Climb")
	float SurfaceTraceDistance = 80.0f;

private:
	// Montage task exit callbacks.
	UFUNCTION()
	void OnMontageCompleted();
	UFUNCTION()
	void OnMontageInterruptedOrCancelled();

	/** Input-release callback (WaitInputRelease) -> cancel. */
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	/** Component surface-loss delegate handler -> cancel. */
	void OnSurfaceLost();

	/** End helper that logs the reason once and ends the ability. */
	void ExitClimb(const TCHAR* Reason, bool bCancelled);

	/** The climb component on the avatar (resolved on activate; the surface-loss delegate binds to it). */
	UPROPERTY()
	TWeakObjectPtr<UAFLClimbMovementComponent> ClimbComponent;

	FDelegateHandle SurfaceLostHandle;

	bool bExiting = false;
};
