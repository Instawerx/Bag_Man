// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interaction/AFLGrabbableComponent.h"   // FAFLGrabPolicy

#include "AFLInteractionComponent.generated.h"

class UAbilitySystemComponent;
class UControlRig;

/**
 * UAFLInteractionComponent  (Object-Interaction substrate, hero side)
 *
 * The carry-state OWNER. Mirrors the proven dash/climb component shape: a standalone UActorComponent added
 * to B_Hero_BagMan via the experience's GameFeatureAction_AddComponents (NOT plugin-side -- the
 * BM-PLUGIN-GRANT-LIFECYCLE workaround that dash/climb use). The grab ABILITY drives intent; THIS component
 * owns the physical state (which actor is held, at which socket, under which release policy) and performs the
 * Hybrid-physics attach/detach (Decision G): AttachToActor + physics-off for the hold, DetachFromActor +
 * physics-on + a modest "slight ragdoll" impulse on release.
 *
 * Carry/climb interaction (Decision H): listens for State.Movement.Climbing; if a climb starts while carrying,
 * it force-releases the held actor (drop on climb-start). The climb ability separately blocks activation while
 * State.Carrying is held (CanActivateAbility) -- the two together make carry+climb a clean either/or.
 *
 * ASC bind mirrors UAFLDashMovementComponent / UAFLClimbMovementComponent (direct
 * GetAbilitySystemComponentFromActor first, ULyraPawnExtensionComponent fallback for the possessed player).
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLInteractionComponent();

	/** Attach + hold the target world actor per its policy. Grants nothing itself (the ability owns the tag);
	 *  returns true on a successful attach. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Interaction")
	bool GrabActor(AActor* Target, const FAFLGrabPolicy& Policy);

	/** Detach the held actor, re-enable physics (per policy), apply the slight-ragdoll release impulse. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Interaction")
	void ReleaseActor();

	UFUNCTION(BlueprintPure, Category = "AFL|Interaction")
	bool IsCarrying() const { return CarriedActor.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "AFL|Interaction")
	AActor* GetCarriedActor() const { return CarriedActor.Get(); }

	// --- Hand-IK driver (Cycle 4c, Control Rig path) -------------------------------------------------
	// Runtime target for the right-hand Basic IK in the hero's Control Rig (CR_AFL_IRONICS, a fork of
	// CR_Mannequin_FootPlant with an additive right-hand Basic IK on Sequence.E). THIS COMPONENT is the
	// single owner of the IK state AND its delivery to the rig: every frame, while bHandIKEnabled, TickComponent
	// resolves the running rig once and pushes HandIKTarget (Position control) + HandIKAlpha (Float control)
	// into it. The grab ability (and the dev console cheat afl.SetHandIKTarget) only SET these fields -- the
	// component does the rig push, so the IK works whether or not a grab is active (true isolation test). The IK
	// runs AFTER foot-plant in the rig, so foot-plant is untouched.

	/** World-space location the right hand IK reaches when enabled (e.g. a grabbable's GrabPoint socket). */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Interaction|HandIK")
	FVector HandIKTarget = FVector::ZeroVector;

	/** Master enable for the right-hand IK. When false the hand follows the clip/slot pose. */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Interaction|HandIK")
	bool bHandIKEnabled = false;

	/** 0..1 blend weight for the IK node's Alpha (smooth fade in/out; the rig's HandIKAlpha control reads this). */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Interaction|HandIK")
	float HandIKAlpha = 0.0f;

	/** Position-control name on CR_AFL_IRONICS the world-space hand target drives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Interaction|HandIK")
	FName HandIKTargetControl = FName(TEXT("HandIKTarget"));

	/** Float-control name on CR_AFL_IRONICS the IK weight drives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Interaction|HandIK")
	FName HandIKAlphaControl = FName(TEXT("HandIKAlpha"));

	/** Thread-safe read of the hand-IK state (kept for any AnimGraph Property Access binding / BP consumer). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AFL|Interaction|HandIK", meta = (BlueprintThreadSafe))
	void GetHandIKState(FVector& OutTarget, bool& bOutEnabled, float& OutAlpha) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void OnAbilitySystemReady();
	void BindToAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindFromAbilitySystem();

	/** State.Movement.Climbing changed -> if a climb just started while carrying, force-release (drop). */
	void HandleClimbTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Holster the equipped weapon during carry (mirror climb's holster) so the rifle's upper-body anim layer
	 *  doesn't fight the grab reach + hold. Hide on grab, restore on release. */
	void HolsterEquippedWeapon();
	void RestoreEquippedWeapon();

	/** Find the running CR_AFL_IRONICS UControlRig on the owner pawn's mesh anim instance (engine
	 *  anim-node-property iteration). Caches it in CachedControlRig; null if not found. */
	UControlRig* ResolveOwnerControlRig();

	/** Push HandIKTarget/HandIKAlpha into the cached rig's controls. Called every frame from TickComponent
	 *  while bHandIKEnabled; pushes alpha=0 on the disabling frame so the hand releases cleanly. */
	void PushHandIKToControlRig();

	/** The held actor + the policy captured at grab time (so release never reaches back into the grabbable). */
	UPROPERTY()
	TWeakObjectPtr<AActor> CarriedActor;

	FAFLGrabPolicy ActivePolicy;

	/** Weapon actors hidden on grab (the holster); restored 1:1 on release. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> HolsteredWeaponActors;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle ClimbTagChangedHandle;

	/** The hero's running hand-IK Control Rig (CR_AFL_IRONICS), resolved lazily on the first IK-enabled tick.
	 *  Reset whenever the IK goes idle so a re-possess / mesh swap re-resolves a fresh rig. */
	TWeakObjectPtr<UControlRig> CachedControlRig;

	/** True once HandIKAlpha=0 has been pushed for a disable, so we stop pushing every idle frame. */
	bool bHandIKReleasedToRig = true;
};
