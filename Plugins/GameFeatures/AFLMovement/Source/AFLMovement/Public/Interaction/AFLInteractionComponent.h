// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"   // FGameplayMessageListenerHandle (drop-on-damage listen)
#include "GameplayTagContainer.h"
#include "Interaction/AFLGrabbableComponent.h"   // FAFLGrabPolicy

#include "AFLInteractionComponent.generated.h"

class UAbilitySystemComponent;
class UAFLObjectClassAnimSet;
class UControlRig;
struct FAFLHitConfirmMessage;

/** How a carried actor leaves the hand. Drop = the modest policy-shaped toss (toggle drop, climb forced
 *  drop, teardown). Throw = an aimed launch along a caller-supplied FULL-3D direction (throw cycle). */
UENUM(BlueprintType)
enum class EAFLReleaseMode : uint8
{
	Drop,
	Throw,
};

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

	/** Detach the held actor, re-enable physics (per policy), seed it with the carrier's velocity (skill
	 *  momentum hand-off), then apply the release impulse. Drop = the policy's actor-forward+up shape;
	 *  Throw = the caller's FULL-3D aim direction at the policy's ThrowImpulseMagnitude. Defaulted params
	 *  keep every legacy caller (toggle drop, climb forced drop, teardown) compiling as Drop. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Interaction")
	void ReleaseActor(EAFLReleaseMode Mode = EAFLReleaseMode::Drop, const FVector& ThrowDirection = FVector::ZeroVector);

	UFUNCTION(BlueprintPure, Category = "AFL|Interaction")
	bool IsCarrying() const { return CarriedActor.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "AFL|Interaction")
	AActor* GetCarriedActor() const { return CarriedActor.Get(); }

	/** 4f: resolve the per-class anim set for a grab about to begin (policy -> DefaultAnimSet fallback),
	 *  cache it as the active set, and return it. The grab ability calls this BEFORE the reach plays so the
	 *  reach/carry montages are known pre-attach; GrabActor() no longer resolves -- it rides this cache.
	 *  Cleared in ReleaseActor. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Interaction")
	UAFLObjectClassAnimSet* ResolveAndCacheAnimSet(const FAFLGrabPolicy& Policy);

	/** The set cached by ResolveAndCacheAnimSet for the current grab/carry; null when idle. */
	UFUNCTION(BlueprintPure, Category = "AFL|Interaction")
	UAFLObjectClassAnimSet* GetActiveAnimSet() const { return ActiveAnimSet; }

	/** 4e: designer fallback when a grabbable's policy has no ObjectAnimSet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Interaction")
	TSoftObjectPtr<UAFLObjectClassAnimSet> DefaultAnimSet;

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

	/** Alpha the IK FADES toward while enabled (4f: TickComponent FInterpTo's HandIKAlpha to this -- never a
	 *  hard 0/1 switch). The console cheat writes HandIKAlpha=1 directly, which is already at this goal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Interaction|HandIK")
	float HandIKAlphaGoal = 1.0f;

	/** FInterpTo speed for the IK alpha fade (DeltaSeconds-driven, frame-rate independent; ~15 = snappy). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Interaction|HandIK")
	float HandIKInterpSpeed = 15.0f;

	/** 4f ability-facing IK control: world target the right hand reaches for (e.g. the grabbable at rest). */
	UFUNCTION(BlueprintCallable, Category = "AFL|Interaction|HandIK")
	void SetHandIKTarget(const FVector& InTarget) { HandIKTarget = InTarget; }

	/** 4f ability-facing IK control: enable starts the alpha fade-in; disable fades out, then releases the rig.
	 *  The console afl.HandIK.* path writes the same fields directly and is unchanged. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Interaction|HandIK")
	void SetHandIKEnabled(bool bEnabled) { bHandIKEnabled = bEnabled; }

	/** Position-control name on CR_AFL_IRONICS the world-space hand target drives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Interaction|HandIK")
	FName HandIKTargetControl = FName(TEXT("HandIKTarget"));

	/** Float-control name on CR_AFL_IRONICS the IK weight drives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Interaction|HandIK")
	FName HandIKAlphaControl = FName(TEXT("HandIKAlpha"));

	// --- LEFT-hand weapon-foregrip IK channel (Layer 1, weapon-hold) ---------------------------------
	// SEPARATE from the right-hand grab channel above. While a weapon is equipped (and NOT grabbing/carrying),
	// each tick the component queries the weapon's GripPoint_L world transform and pushes it into CR_AFL_IRONICS'
	// LeftHandIKTarget/LeftHandIKAlpha controls (the cold-verified left-arm Basic IK on upperarm_l/lowerarm_l/
	// hand_l), so hand_l plants on the foregrip. Cosmetic -> runs on EVERY client (owning + simulated proxies),
	// driven by the REPLICATED equipment-equipped state; the IK target itself is queried locally, never replicated.

	/** MASTER ENABLE for the left-hand weapon-foregrip IK channel (Layer 1). DEFAULT FALSE: the channel is
	 *  DORMANT -- never arms, and LeftHandIKAlpha is held at 0, so the rig's LeftHandIK_TwoBone contributes
	 *  nothing and hand_l stays in the baked two-handed-hold clip pose (strictly better than the broken IK).
	 *  The CR node, controls, and the whole channel stay intact -- just neutralized. Flip true (EditAnywhere,
	 *  no rebuild) to re-engage once the weapon-hold is rebuilt the canonical way. Right-hand grab IK, foot IK,
	 *  and the CR locomotion rig are separate channels and are UNAFFECTED by this flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Interaction|HandIK")
	bool bEnableLeftHandWeaponIK = false;

	/** 0..1 blend weight for the left-hand weapon-IK (eases on when armed, off when not -- never a hard switch). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Interaction|HandIK")
	float LeftHandIKAlpha = 0.0f;

	/** Per-frame scratch: the foregrip world location resolved in TickComponent's gate, reused by
	 *  UpdateLeftHandWeaponIK so the equipped-weapon path is walked once per tick (not twice). */
	FVector LeftHandIKTargetScratch = FVector::ZeroVector;

	/** Per-frame: "a weapon is equipped AND not carrying" (resolved once in TickComponent, consumed by
	 *  UpdateLeftHandWeaponIK to pick the alpha goal). Eliminates the double equipment-path walk. */
	bool bLeftWeaponArmed = false;

	/** Position-control name on CR_AFL_IRONICS the world-space LEFT-hand (foregrip) target drives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Interaction|HandIK")
	FName LeftHandIKTargetControl = FName(TEXT("LeftHandIKTarget"));

	/** Float-control name on CR_AFL_IRONICS the LEFT-hand IK weight drives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Interaction|HandIK")
	FName LeftHandIKAlphaControl = FName(TEXT("LeftHandIKAlpha"));

	/** REACHABILITY GUARD (Layer 1): one-shot flag so the AFL_LEFTIK_REACH verdict logs exactly once per equip
	 *  (target-to-shoulder distance vs arm length), NOT every tick. Reset when no weapon is armed so a re-equip
	 *  re-logs. Measures with the IK gated OFF (alpha 0) -- the guard that proves a weapon's GripPoint_L is within
	 *  arm's reach BEFORE the dynamic IK channel is ever engaged (an unreachable target = a fully-extended limb). */
	bool bLeftReachLogged = false;

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

	/** Event.Damage.Confirmed arrived -> if it hit OUR pawn while carrying (and the policy says so),
	 *  force-release (drop). The drop-on-damage half of the carry pressure loop (climb's sibling). */
	void HandleDamageConfirmed(FGameplayTag Channel, const FAFLHitConfirmMessage& Msg);

	/** Cancel the owning ASC's active grab ability (the throw's cancel-by-class seam, reused). The forced
	 *  release paths call this AFTER ReleaseActor so EVERY exit funnels through the grab's EndAbility --
	 *  the single State.Carrying (+ per-object carrier GE) removal site. EndAbility's own ReleaseActor()
	 *  then no-ops on the !Target guard (the proven double-release protection). Without this, a forced
	 *  drop left the ability active and the carry tag applied until the next G press (the leak). */
	void CancelOwningGrabAbility();

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

	/** LEFT-hand weapon-foregrip channel (Layer 1). Resolves "armed AND not carrying", takes the foregrip world
	 *  target (the weapon's GripPoint_L socket), FInterpTo's LeftHandIKAlpha toward 1 (armed) / 0 (not), and pushes
	 *  LeftHandIKTarget + LeftHandIKAlpha into Rig. Shares the SAME resolved rig as the right-hand push (no second
	 *  resolve). Returns true if the left channel is still active (alpha > ~0) so the tick keeps the rig cached. */
	bool UpdateLeftHandWeaponIK(float DeltaTime, class UControlRig* Rig);

	/** Resolve the LEFT-grip world target (Layer 1, canonical weapon-hold). ARMED gate: a weapon is equipped
	 *  (Pawn->EquipMgr->GetSpawnedActors -> the weapon actor). TARGET = that weapon's GripPoint_L USceneComponent
	 *  world location (the weapon authors its own foregrip point; matched by name). Returns false when unarmed OR
	 *  when the weapon ships no GripPoint_L (-> the left channel fades off; never IK to a fallback). */
	bool ResolveEquippedWeaponForegripWorld(FVector& OutWorldLocation) const;

	/** The held actor + the policy captured at grab time (so release never reaches back into the grabbable). */
	UPROPERTY()
	TWeakObjectPtr<AActor> CarriedActor;

	FAFLGrabPolicy ActivePolicy;

	/** 4e: resolved set for the carried actor; cached at grab-begin, cleared on release. Consumed in 4f. */
	UPROPERTY(Transient)
	TObjectPtr<UAFLObjectClassAnimSet> ActiveAnimSet = nullptr;

	/** Weapon actors hidden on grab (the holster); restored 1:1 on release. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> HolsteredWeaponActors;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle ClimbTagChangedHandle;

	/** Drop-on-damage listen on Event.Damage.Confirmed (registered/unregistered beside the climb bind). */
	FGameplayMessageListenerHandle DamageMessageHandle;

	/** The hero's running hand-IK Control Rig (CR_AFL_IRONICS), resolved lazily on the first IK-enabled tick.
	 *  Reset whenever the IK goes idle so a re-possess / mesh swap re-resolves a fresh rig. */
	TWeakObjectPtr<UControlRig> CachedControlRig;

	/** True once HandIKAlpha=0 has been pushed for a disable, so we stop pushing every idle frame. */
	bool bHandIKReleasedToRig = true;

	/** 4f carry diagnostic: accumulator for the 1 Hz attached-state log while carrying. */
	float CarryDiagAccum = 0.0f;
};
