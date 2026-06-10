// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "Interaction/AFLGrabbableComponent.h"   // FAFLGrabPolicy (by-value member)

#include "AFLGameplayAbility_Grab.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UGameplayEffect;
class UAFLInteractionComponent;

/**
 * UAFLGameplayAbility_Grab  (Object-Interaction proof ability)
 *
 * The grab/hold/let-go ability. On activate it resolves+validates the target, pre-resolves the per-class
 * anim set (UAFLObjectClassAnimSet via the interaction component), then runs REACH-THEN-ATTACH (4f): the
 * reach montage plays on the UpperBody slot while the hand-IK fades toward the object at rest; the attach
 * (UAFLInteractionComponent::GrabActor) happens AT the montage's hand-contact notify
 * (Event.Interaction.GrabAttach), with montage-end as a logged fallback and pre-contact interrupts
 * aborting cleanly (no attach, no state). After the attach: State.Carrying GE + the per-class CarryPose
 * hold (montage authored bEnableAutoBlendOut=false, held until stopped). A null/empty anim set falls back
 * to the proven 4d instant-attach + frozen-reach-pose path.
 *
 * TOGGLE lifecycle (4d): OnInputTriggered -- first press grabs+holds (stays active, input released),
 * second press routes to InputPressed -> EndAbility (the drop / mid-reach abort). EndAbility is the one
 * cleanup funnel: IK off, hold montage stopped, ReleaseActor, State.Carrying removed.
 * Abstract -- the GA_AFL_Grab BP child sets CarryingEffectClass (+ legacy GrabMontage).
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

	/** (Cycle 4d grab-hold fix) TOGGLE drop: GAS routes a re-press of the input on an ALREADY-ACTIVE ability
	 *  here (not a re-activate), so the second press ends the carry. */
	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

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

	/**
	 * Cycle 4d -- SUSTAINED carry pose. When true, the reach clip is FROZEN at its reach-peak frame for the
	 * whole carry (played at rate 0 starting at GrabHoldTime), so the upper body holds a carry stance instead
	 * of the 4c transient reach-and-return gesture that relaxed after ~2s. The legs keep locomotion (UpperBody
	 * slot composition, proven in the grab-composition lane). EndAbility calls StopSlotAnimation to release it.
	 * Set false to restore the 4c transient one-shot reach (rate 1.0, plays through once).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	bool bGrabHoldPose = true;

	/**
	 * The time (s) into GrabReachAnim to FREEZE on for the held carry pose (the reach-peak frame). The
	 * retargeted A_ItemPickup clip is 2.0s / 60f and reaches its peak mid-clip; 0.9s (~frame 27) lands near
	 * the top of the reach. Tunable here without a rebuild if the visual peak is elsewhere. Only used when
	 * bGrabHoldPose is true.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab", meta = (ClampMin = "0.0", EditCondition = "bGrabHoldPose"))
	float GrabHoldTime = 0.9f;

	/** Fallback montage path (legacy). Only used if GrabReachAnim is unset. Plays via PlayMontageAndWait. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	TObjectPtr<UAnimMontage> GrabMontage;

	/** 4f reach-then-attach: the event the reach montage's contact notify fires (UAFLAnimNotify_GameplayEvent);
	 *  the attach happens when it arrives. Resolved lazily from "Event.Interaction.GrabAttach" if unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab")
	FGameplayTag GrabAttachEventTag;

	/** Time (s) into the per-class CarryPose montage where the settled hold begins. The montage plays from
	 *  here once and HOLDS its final pose (CarryPose montages author bEnableAutoBlendOut=false). The 0cm-lift
	 *  clip settles into the carry at ~2.0s. Only used on the per-class path. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Grab", meta = (ClampMin = "0.0"))
	float CarryPoseStartTime = 2.0f;

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
	/** Resolve the grab target from the interaction event data (Lyra passes it as Target/OptionalObject). */
	AActor* ResolveTargetActor(const FGameplayEventData* TriggerEventData) const;

	/** The avatar pawn-mesh's AnimInstance (where the upper-body reach slot plays), or null. */
	UAnimInstance* GetGrabAnimInstance(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** End helper that logs the reason once and ends the ability. */
	void ExitGrab(const TCHAR* Reason, bool bCancelled);

	// --- 4f reach-then-attach callbacks + the shared attach sequence ---------------------------------

	/** The contact notify's event arrived: attach now (the reach found the object). */
	UFUNCTION()
	void OnGrabAttachEvent(FGameplayEventData Payload);

	/** Reach montage finished without the contact event: logged fallback attach at montage end. */
	UFUNCTION()
	void OnReachMontageCompleted();

	/** Reach montage interrupted/cancelled BEFORE contact: abort the grab (no attach, no state). The
	 *  carry-pose montage interrupting an already-attached reach is expected and no-ops here. */
	UFUNCTION()
	void OnReachMontageInterrupted();

	/** Attach + State.Carrying + carry pose -- shared by the notify path, the montage-end fallback, and
	 *  the no-montage legacy instant path. Guarded so only the first arrival runs it. */
	void DoAttachAndCarry();

	/** Start the hold stance: per-class CarryPose montage when the anim set has one (settle-then-hold,
	 *  autoBlendOut=false), else the proven 4d frozen-reach fallback on the UpperBody slot. */
	void StartCarryPose();

	/** The hero's interaction component (resolved on activate; performs the attach/detach). */
	UPROPERTY()
	TWeakObjectPtr<UAFLInteractionComponent> InteractionComponent;

	/** Validated target + its policy, carried through the reach window to the deferred attach. */
	UPROPERTY()
	TWeakObjectPtr<AActor> PendingGrabTarget;
	FAFLGrabPolicy PendingGrabPolicy;

	/** The per-class hold montage we started (stopped in EndAbility); null on the legacy slot path. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveCarryPoseMontage;

	/** True once the attach ran (notify or fallback); later arrivals no-op. */
	bool bAttachDone = false;

	bool bExiting = false;
};
