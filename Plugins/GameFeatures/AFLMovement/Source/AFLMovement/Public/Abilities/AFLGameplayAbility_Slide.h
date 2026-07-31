// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_Slide.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_PlayMontageAndWait;

/**
 * UAFLGameplayAbility_Slide  (Movement Overhaul — Phase 1 keystone: slide + slide-and-shoot)
 *
 * The MOTION half of slide (mirrors GA_AFL_Climb's Motion-Warp + montage pattern). THIS ability owns the
 * movement — a root-motion slide montage + a Motion Warping target (WarpToSlideStop) that skews the fixed-
 * distance raw slide to a variable, geometry-aware stop point. UAFLSlideMovementComponent owns the CMC feel
 * (low friction/braking + capsule duck) while State.Movement.Sliding is held. Parented to ULyraGameplayAbility,
 * Local-predicted, server-validated. Abstract — the GA_AFL_Slide BP child sets the GE + montage.
 *
 * SLIDE-AND-SHOOT: this ability does NOT block the fire/ADS abilities and does NOT grant a no-fire tag; the
 * slide MONTAGE is authored on a LOWER-BODY slot so the upper body keeps aiming (weapon IK + aim-offset
 * untouched). Root motion applies regardless of slot, so the character still slides while shooting.
 *
 * Lifecycle:
 *   ActivateAbility: require grounded + moving >= MinSlideSpeed; add WarpToSlideStop (forward+down trace);
 *     apply SlideActiveEffectClass (grants State.Movement.Sliding); play SlideMontage; bind exits
 *     (montage complete/blendout/interrupt + input-release early-out).
 *   EndAbility: RemoveAllWarpTargets + remove the active GE (tag clears -> component restores the CMC).
 *
 * Native defaults: InstancedPerActor, LocalPredicted, OnInputTriggered (press-to-slide).
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_Slide : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_Slide(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

	/** Duration GE applied on activation — grants State.Movement.Sliding (mirror GE_AFL_Dash_Active). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Slide")
	TSubclassOf<UGameplayEffect> SlideActiveEffectClass;

	/** Root-motion slide montage. BP child sets it. Author on a LOWER-BODY slot (slide-and-shoot) with an
	 *  AnimNotifyState_MotionWarping window targeting WarpToSlideStop (mirror AM_AFL_ClimbWarp). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Slide")
	TObjectPtr<UAnimMontage> SlideMontage;

	/** Minimum horizontal speed (cm/s) required to start a slide (prevents slide-from-standstill). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Slide")
	float MinSlideSpeed = 250.0f;

	/** Max forward distance (cm) the slide warps toward; the forward/down traces shorten it against geometry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Slide")
	float SlideStopDistance = 350.0f;

	/** Warp target name — must match the AnimNotifyState_MotionWarping window's target on SlideMontage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Slide")
	FName SlideStopWarpTargetName = TEXT("WarpToSlideStop");

private:
	UFUNCTION()
	void OnMontageCompleted();
	UFUNCTION()
	void OnMontageInterruptedOrCancelled();
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	/** Compute the geometry-aware slide stop point (forward trace shortens against walls, down trace snaps to
	 *  ground) and its facing (the slide direction). Returns false if no valid target (caller skips the warp). */
	bool ComputeSlideStopTarget(FVector& OutLocation, FRotator& OutRotation) const;

	/** Single-exit-wins end helper. */
	void ExitSlide(const TCHAR* Reason, bool bCancelled);

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	bool bWarpApplied = false;
	bool bExiting = false;
};
