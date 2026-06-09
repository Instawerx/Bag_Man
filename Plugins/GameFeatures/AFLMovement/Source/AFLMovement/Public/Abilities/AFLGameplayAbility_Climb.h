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

	/** Root-motion climb montage. BP child sets it. Two sections expected: the ascent LOOP (LoopSectionName)
	 *  and a one-shot mantle CAP (MantleSectionName). The loop sustains while held; the ability jumps to the
	 *  mantle on input-release-near-top (see OnInputReleased). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Climb")
	TObjectPtr<UAnimMontage> ClimbMontage;

	/** Montage section that loops the wall-ascent (anim_Climb_Up). Sustains until the ability ends or hands off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Climb")
	FName LoopSectionName = TEXT("Loop");

	/** Montage section that plays the one-shot mantle-onto-ledge cap (anim_Mantle_2M_R). Fired on release-near-top. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Climb")
	FName MantleSectionName = TEXT("Mantle");

	/** Forward trace length (cm) for the climbable-surface validation on activate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Climb")
	float SurfaceTraceDistance = 80.0f;

	/** Upward trace length (cm) on input-release to detect a ledge top: clear above -> mantle, blocked -> drop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Climb")
	float LedgeTopTraceDistance = 180.0f;

private:
	// Montage task exit callbacks.
	UFUNCTION()
	void OnMontageCompleted();
	UFUNCTION()
	void OnMontageInterruptedOrCancelled();

	/** Input-release callback (WaitInputRelease). Branches: ledge-top clear -> mantle cap; blocked -> drop. */
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	/** Component surface-loss delegate handler -> cancel. */
	void OnSurfaceLost();

	/** End helper that logs the reason once and ends the ability. */
	void ExitClimb(const TCHAR* Reason, bool bCancelled);

	/** Fire the mantle cap if the montage has one; otherwise ExitClimb(FallbackReason). Funnels BOTH top
	 *  signals (input-release-near-top AND forward-wall-lost) so whichever arrives first caps the climb. */
	void TryMantleOrExit(const TCHAR* FallbackReason);

	/** Guaranteed mantle exit: if the mantle section's OnCompleted is swallowed (jump issued mid-loop-blend),
	 *  this timer force-completes the ability so the character can never be left floating in MOVE_Flying. */
	void OnMantleTimeout();

	/** Upward trace from the avatar on input-release: true = ledge top clear above (mantle), false = wall continues. */
	bool IsLedgeTopClear() const;

	/** The climb component on the avatar (resolved on activate; the surface-loss delegate binds to it). */
	UPROPERTY()
	TWeakObjectPtr<UAFLClimbMovementComponent> ClimbComponent;

	/** The montage task (kept so the input-release branch can JumpToSection(Mantle) to hand off ascent->cap). */
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FDelegateHandle SurfaceLostHandle;

	/** Fallback timer guaranteeing the ability exits after the mantle, even if OnMontageCompleted is missed. */
	FTimerHandle MantleTimeoutHandle;

	/** True once the mantle cap has been triggered: the loop's OnCompleted is now the mantle's completion ->
	 *  that exit reports reason=complete (the success cap), not the unreachable loop completion. */
	bool bMantling = false;

	bool bExiting = false;
};
