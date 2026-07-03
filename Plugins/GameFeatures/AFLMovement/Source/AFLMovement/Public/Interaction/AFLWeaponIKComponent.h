// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "AFLWeaponIKComponent.generated.h"

class USkeletalMeshComponent;
class ULyraEquipmentManagerComponent;
class UControlRig;

/**
 * Weapon-handling class -- selects the support(left)-hand IK branch. The C++-reachable classifier is the
 * foregrip-socket physical contract (IRONICS_WEAPONS_SSOT §7: GripPoint_L is authored on 2H meshes ONLY --
 * Rifle/Shotgun -- never on a 1H Pistol), which is the public equivalent of the anim-layer class (§1.3:
 * {Rifle,Shotgun}=2H, {Pistol}=1H). We do NOT invent a parallel classifier: PickBestAnimLayer + EquippedAnimSet
 * are protected on ULyraWeaponInstance, so we read the equivalent §7 socket -- the very point the IK solves to.
 * Thrown is an ability-state tag on the ASC (a grenade throw active), not a weapon field.
 */
UENUM(BlueprintType)
enum class EAFLWeaponHandling : uint8
{
	None       UMETA(DisplayName = "None (unarmed)"),          // -> IK off
	TwoHanded  UMETA(DisplayName = "Two-Handed"),              // {Rifle,Shotgun} -> solve hand_l to GripPoint_L foregrip
	OneHanded  UMETA(DisplayName = "One-Handed"),              // {Pistol} -> CUP stance (target = hand_r + offset), or free
	Thrown     UMETA(DisplayName = "Thrown (grenade active)")  // -> IK off
};

/**
 * Spec Part 1 output struct. COMPONENT-space (FIX 2) + FQuat rotation (FIX 3). GetCurrentIKOutputs() also stays
 * BlueprintThreadSafe so an AnimBP CAN read it; but the ACTUAL delivery is C++ SetControlValue on the CR controls
 * the native CR_AFL_CoreIK solve reads (PushToControlRig) -- the spec's AnimBP-pin feed is not viable here because
 * the anim graph lives in the SHARED base ABP_Mannequin_Base (editing it = a frozen shared asset). SetControlValue
 * is the PROVEN mechanism (the right-hand grab IK), converted to the LEFT chain + component-space.
 */
USTRUCT(BlueprintType)
struct FAFLIKDataOutputs
{
	GENERATED_BODY()

	/** COMPONENT-space (relative to the character mesh component). FIX 2 -- never a world-space target. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|IK") FVector LeftHandTargetLocation = FVector::ZeroVector;

	/** FQuat -- FIX 3 (no Euler; gimbal-safe past 90deg). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|IK") FQuat   LeftHandTargetRotation = FQuat::Identity;

	/** COMPONENT-space dynamic pole vector (cross-product derived) -- stabilizes the elbow bend plane. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|IK") FVector LeftElbowPoleVector    = FVector::ZeroVector;

	/** 0..1 solve weight (FInterpTo-smoothed; 0 when gated off by class or reach). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|IK") float   LeftHandIKAlpha        = 0.0f;
};

/**
 * UAFLWeaponIKComponent -- PATH B: the spec's component-space, delta-driven weapon support-hand IK.
 *
 * Rebuilds the (dormant, flung) left-hand weapon IK correctly. The old CR_AFL_IRONICS LeftHandIK_TwoBone flung
 * because it fed a WORLD-space GripPoint_L into a COMPONENT-space effector (no conversion) on an UN-MIRRORED
 * (right-arm) chain. This component fixes both at the source and the CR (CR_AFL_CoreIK) uses the correct LEFT
 * chain (upperarm_l/lowerarm_l/hand_l). The three fixes that make re-treading the Two-Bone node class safe:
 *   FIX 1 (mirrored chain): CR effector = upperarm_l/lowerarm_l/hand_l (verified in CR_AFL_CoreIK, Tier 2).
 *   FIX 2 (component space): every target here is component-space (GripLWorld.GetRelativeTransform(MeshComp)).
 *   FIX 3 (quaternions):     rotation is FQuat, no Euler past 90deg.
 * Plus the 94% reach-gate backstop (past 0.94 * arm length -> alpha 0 -> hand_l falls to the baked pose; a
 * flung arm can never ship). Class-gate is FIRST (1H-free/thrown -> off up front); the reach-gate is second.
 *
 * Lives on B_Hero_BagMan (added via the experience GameFeatureAction_AddComponents -- the dash/climb/interaction
 * pattern). TG_PostPhysics so the weapon attachment is locked before we read GripPoint_L. Cosmetic/LOCAL only --
 * the IK output is never replicated (equipped state already replicates); runs on owning + simulated proxies.
 *
 * SUPERSEDES the left-hand weapon-IK channel in UAFLInteractionComponent (flagged there; physically retired with
 * the CR_AFL_IRONICS LeftHandIK_TwoBone node in Tier 2). Reuses that channel's proven equip-resolve pattern
 * (EquipMgr-on-Pawn -> GetSpawnedActors -> GripPoint_L), converted to component space.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLWeaponIKComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLWeaponIKComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The AnimBP reads this every thread-safe update and feeds CR_AFL_CoreIK's exposed pins. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AFL|IK", meta = (BlueprintThreadSafe))
	FAFLIKDataOutputs GetCurrentIKOutputs() const { return CachedIKOutputs; }

	UFUNCTION(BlueprintPure, Category = "AFL|IK")
	EAFLWeaponHandling GetCurrentHandling() const { return CurrentHandling; }

	// --- Settings ------------------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|IK|Settings")
	float InterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|IK|Settings")
	float DynamicPoleVectorOutwardOffset = 45.0f;

	/** Reach-gate backstop threshold: past this fraction of the real arm length, alpha -> 0 (a flung/over-
	 *  stretched arm can never ship). Tuned 0.94 -> 0.97 for Arclight's rear-handguard reach (+~2cm forward).
	 *  Higher = a straighter arm at the limit (the pole matters less near-locked) -> revert toward 0.94 if any
	 *  weapon starts to fling at the reach edge. Do NOT push toward 1.0 (locked arm = the flung-arm danger zone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|IK|Settings", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float ArmLengthBufferPercent = 0.97f;

	/** Too-close guard (spec): target closer than this to the shoulder -> alpha 0 (weapon jammed to chest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|IK|Settings")
	float MinReachDistance = 12.0f;

	/** 1H CUP stance (AAA default): the support hand's offset from hand_r in component space (cup under the
	 *  firing hand). NOT a weapon socket -> a 1H weapon never points the reach at a nonexistent foregrip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|IK|Settings")
	FVector OneHandedCupOffset = FVector(-4.0f, -6.0f, -3.0f);

	/** 1H default = cup stance. false -> 1H free-hand (alpha 0, the SSOT §1.4 baseline). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|IK|Settings")
	bool bOneHandedCupStance = true;

	/** Presence of this tag on the owner's ASC = a throw is in progress -> the Thrown branch (IK off). Set to
	 *  the grenade-active state tag in the details panel (thrown is an ability, not a weapon field). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|IK|Settings")
	FGameplayTag ThrowingStateTag;

	// --- Bone / socket names (SK_Mannequin -- NOT the spec's shoulder_l/elbow_l) ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "AFL|IK|Rig") FName ShoulderBone   = FName(TEXT("upperarm_l")); // IK chain root
	UPROPERTY(EditDefaultsOnly, Category = "AFL|IK|Rig") FName ElbowBone      = FName(TEXT("lowerarm_l"));
	UPROPERTY(EditDefaultsOnly, Category = "AFL|IK|Rig") FName HandLBone      = FName(TEXT("hand_l"));     // effector
	UPROPERTY(EditDefaultsOnly, Category = "AFL|IK|Rig") FName HandRBone      = FName(TEXT("hand_r"));     // 1H cup ref
	UPROPERTY(EditDefaultsOnly, Category = "AFL|IK|Rig") FName ForegripSocket = FName(TEXT("GripPoint_L"));

	// --- CR controls the native CR_AFL_CoreIK solve reads (this component pushes them via SetControlValue). ----
	//     LeftHandIKTarget + LeftHandIKAlpha already exist on CR_AFL_IRONICS (orphaned from the removed flung
	//     solver -> reused); AFL_LeftHandPole is the new Position control carrying the dynamic bend-plane pole.
	UPROPERTY(EditDefaultsOnly, Category = "AFL|IK|Rig") FName LeftHandTargetControl = FName(TEXT("LeftHandIKTarget"));
	UPROPERTY(EditDefaultsOnly, Category = "AFL|IK|Rig") FName LeftHandPoleControl   = FName(TEXT("AFL_LeftHandPole"));
	UPROPERTY(EditDefaultsOnly, Category = "AFL|IK|Rig") FName LeftHandAlphaControl  = FName(TEXT("LeftHandIKAlpha"));
	// The socket's ROTATION orients the hand (fingers/palm wrap) -- the load-bearing IK interface's 2nd axis.
	UPROPERTY(EditDefaultsOnly, Category = "AFL|IK|Rig") FName LeftHandRotControl    = FName(TEXT("AFL_LeftHandRot"));

protected:
	virtual void BeginPlay() override;

private:
	FAFLIKDataOutputs  CachedIKOutputs;
	EAFLWeaponHandling CurrentHandling = EAFLWeaponHandling::None;
	float TargetAlpha  = 0.0f;
	float CurrentAlpha = 0.0f;

	// Runtime-computed arm lengths FROM the ref skeleton (NOT the spec's 32.5/29.0 hardcode). Lazy-cached on
	// the first tick with a valid posed mesh (bone lengths are pose-invariant, so any pose measures them).
	bool  bArmLenCached = false;
	float UpperArmLength = 0.0f;
	float LowerArmLength = 0.0f;
	float TotalArmLength = 0.0f;

	USkeletalMeshComponent* GetOwnerMesh() const;
	void EnsureArmLengths(USkeletalMeshComponent* Mesh);

	/** Classify the equipped weapon (anim-layer class; socket-presence fallback) + the thrown ability state. */
	EAFLWeaponHandling ResolveHandling(const ULyraEquipmentManagerComponent* EquipMgr, AActor*& OutWeaponActor) const;

	/** FIX 2: GripPoint_L world transform -> COMPONENT space (relative to the mesh component). */
	bool ResolveForegripComponentSpace(AActor* WeaponActor, USkeletalMeshComponent* Mesh, FVector& OutCompLoc, FQuat& OutCompRot) const;

	/** Compute the dynamic pole vector (component-space cross-products) so the elbow bends outward/down. */
	FVector ComputeElbowPoleVector(const FVector& ShoulderCompPos, const FVector& TargetCompPos) const;

	/** PATH B delivery: push CachedIKOutputs into the CR controls the native CR_AFL_CoreIK solve reads. The
	 *  PROVEN ResolveOwnerControlRig walk (mesh -> AnimInstance -> FAnimNode_ControlRig) + SetControlValue --
	 *  the right-hand grab IK's exact mechanism -- because the anim graph is in the shared base ABP. */
	UControlRig* ResolveOwnerControlRig(USkeletalMeshComponent* Mesh);
	void PushToControlRig();
	TWeakObjectPtr<UControlRig> CachedControlRig;
};
