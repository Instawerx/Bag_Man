// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_Grab.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UGameplayEffect;
class UAFLInteractionComponent;

/**
 * UAFLGameplayAbility_Grab  (Object-Interaction proof ability)
 *
 * The grab/hold/let-go ability. Granted via Lyra's FInteractionOption.InteractionAbilityToGrant when the
 * player looks at a UAFLGrabbableComponent-bearing actor (the Lyra-discovery half of the Hybrid). On
 * activate it reads the target from the interaction event, validates it, asks the hero's
 * UAFLInteractionComponent to attach+hold it (the AFL carry-substrate half), grants State.Carrying, and
 * plays the grab montage (held at end-pose for the hold). WaitInputRelease -> release: the interaction
 * component detaches + re-enables physics + applies the slight-ragdoll impulse.
 *
 * Channeled lifecycle mirrors the proven sibling UAFLGameplayAbility_Climb (the Pressed-IA recipe):
 * ActivationPolicy=WhileInputActive (single sustained activation per hold) + IA on InputTriggerPressed +
 * a WaitInputRelease task (nothing else calls CancelInputActivatedAbilities -- CheatManager only).
 * Abstract -- the GA_AFL_Grab BP child sets CarryingEffectClass + GrabMontage.
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_Grab : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_Grab(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

	/** Duration GE applied on grab -- grants State.Carrying (mirror GE_AFL_Climb_Active's shape). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	TSubclassOf<UGameplayEffect> CarryingEffectClass;

	/**
	 * Grab reach animation, played on the UPPER-BODY slot as a dynamic montage so the arms reach
	 * while the legs keep following locomotion (the Lyra-canonical layered-upper-body pattern; the
	 * AnimGraph routes the named slot through a Layered Blend Per Bone). This is the PRIMARY path --
	 * it owns the slot binding in gameplay (not baked into a montage asset) and sidesteps the
	 * montage-slot-registration step. Set to the retargeted pickup clip (on Lyra's SK_Mannequin).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	TObjectPtr<UAnimSequenceBase> GrabReachAnim;

	/**
	 * The anim-graph slot the reach plays on. Default "UpperBody" -- one of the five slots Lyra's
	 * ABP_Mannequin_Base wires (FullBody / UpperBody / AdditiveHitReact / FullBodyAdditivePreAim /
	 * UpperBodyAdditive). Tunable here without a rebuild if the slot vocabulary changes. "DefaultSlot"
	 * is NOT routed in Lyra's AnimGraph -- a montage/anim on it plays into the void (the bug this fixes).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	FName GrabReachSlot = FName(TEXT("UpperBody"));

	/** Blend-in / blend-out (s) for the upper-body reach. Short blend = snappy grab. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	float GrabReachBlendTime = 0.15f;

	/** Fallback montage path (legacy). Only used if GrabReachAnim is unset. Plays via PlayMontageAndWait. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	TObjectPtr<UAnimMontage> GrabMontage;

	// Hand-IK (Cycle 4c, Control Rig path): the hero's UAFLInteractionComponent OWNS resolving CR_AFL_IRONICS
	// and pushing HandIKTarget/HandIKAlpha into it every frame (its TickComponent, Path 1). This ability holds
	// NO rig state -- it sets the component's HandIK fields when it wants the hand driven (a follow-on; not
	// wired in the isolation-proof commit). Keeps the rig logic single-owner on the component.

	/** Reach (cm) of the input-activated forward grab trace (PATH 2, when there is no interaction event). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	float GrabReachDistance = 250.0f;

	/** Sphere radius (cm) of the grab trace -- a sphere (not a line) gives forgiving acquisition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	float GrabTraceRadius = 35.0f;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	/** Resolve the grab target from the interaction event data (Lyra passes it as Target/OptionalObject). */
	AActor* ResolveTargetActor(const FGameplayEventData* TriggerEventData) const;

	/** The avatar pawn-mesh's AnimInstance (where the upper-body reach slot plays), or null. */
	UAnimInstance* GetGrabAnimInstance(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** End helper that logs the reason once and ends the ability. */
	void ExitGrab(const TCHAR* Reason, bool bCancelled);

	/** The hero's interaction component (resolved on activate; performs the attach/detach). */
	UPROPERTY()
	TWeakObjectPtr<UAFLInteractionComponent> InteractionComponent;

	/** Orientation fix: cache the hero's controller-yaw flag so EndAbility restores free aim after the grab. */
	bool bOrientationApplied = false;
	bool bCachedUseControllerYaw = true;

	bool bExiting = false;
};
