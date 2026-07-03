// Copyright C12 AI Gaming. All Rights Reserved.

#include "Interaction/AFLWeaponIKComponent.h"

#include "AFLMovement.h"                               // LogAFLMovement
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "Weapons/LyraWeaponInstance.h"

#include "ControlRig.h"
#include "AnimNode_ControlRig.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLWeaponIKComponent)

UAFLWeaponIKComponent::UAFLWeaponIKComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// CRUCIAL (spec): after physics + weapon attachment are locked, so GripPoint_L is at its final world pose.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UAFLWeaponIKComponent::BeginPlay()
{
	Super::BeginPlay();
	// Arm lengths are computed lazily on the first tick with a posed mesh (bone lengths are pose-invariant).
}

USkeletalMeshComponent* UAFLWeaponIKComponent::GetOwnerMesh() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->GetMesh();
	}
	return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UAFLWeaponIKComponent::EnsureArmLengths(USkeletalMeshComponent* Mesh)
{
	if (bArmLenCached || !Mesh)
	{
		return;
	}
	// RUNTIME-COMPUTE (NOT the spec's 32.5/29.0 hardcode): adjacent-bone component-space distances = bone
	// lengths (pose-invariant), so any valid pose measures the real skeleton (SK_Mannequin).
	const FVector Shoulder = Mesh->GetBoneLocation(ShoulderBone, EBoneSpaces::ComponentSpace);
	const FVector Elbow    = Mesh->GetBoneLocation(ElbowBone,    EBoneSpaces::ComponentSpace);
	const FVector HandL    = Mesh->GetBoneLocation(HandLBone,    EBoneSpaces::ComponentSpace);
	UpperArmLength = FVector::Dist(Shoulder, Elbow);
	LowerArmLength = FVector::Dist(Elbow, HandL);
	TotalArmLength = UpperArmLength + LowerArmLength;
	if (TotalArmLength > 1.0f) // a sane posed skeleton (guards the pre-pose frame)
	{
		bArmLenCached = true;
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_WEAPONIK: arm length from ref skeleton: upper=%.1f lower=%.1f total=%.1f"),
			UpperArmLength, LowerArmLength, TotalArmLength);
	}
}

EAFLWeaponHandling UAFLWeaponIKComponent::ResolveHandling(const ULyraEquipmentManagerComponent* EquipMgr, AActor*& OutWeaponActor) const
{
	OutWeaponActor = nullptr;

	// 1) THROWN first -- a grenade throw overrides the held weapon's class (thrown is an ability, not a field).
	if (ThrowingStateTag.IsValid())
	{
		if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::Get().GetAbilitySystemComponentFromActor(GetOwner()))
		{
			if (ASC->HasMatchingGameplayTag(ThrowingStateTag))
			{
				return EAFLWeaponHandling::Thrown;
			}
		}
	}

	if (!EquipMgr)
	{
		return EAFLWeaponHandling::None;
	}

	// 2) The equipped weapon's spawned actor (proven EquipMgr-on-Pawn walk). GetEquipmentInstancesOfType already
	//    filters to weapons, so any instance returned IS a weapon -- no cast needed.
	for (ULyraEquipmentInstance* Inst : EquipMgr->GetEquipmentInstancesOfType(ULyraWeaponInstance::StaticClass()))
	{
		if (Inst && Inst->GetSpawnedActors().Num() > 0)
		{
			OutWeaponActor = Inst->GetSpawnedActors()[0];
			break;
		}
	}
	if (!OutWeaponActor)
	{
		return EAFLWeaponHandling::None;
	}

	// 3) CLASSIFIER = the SSOT §7 physical contract: a GripPoint_L foregrip socket is authored on 2H meshes ONLY
	//    (Rifle/Shotgun), never on 1H (Pistol). This is the PUBLIC, C++-reachable equivalent of the anim-layer
	//    class (§1.3) -- PickBestAnimLayer + EquippedAnimSet are protected on ULyraWeaponInstance, not callable
	//    here -- AND it is the more robust signal: it is exactly the point the IK solves to (no GripPoint_L ->
	//    nothing to two-hand -> 1H), with no fragile layer-name string match. A 2H weapon that (mis)ships no
	//    GripPoint_L reads as 1H and gets the cup stance rather than a flail -- fail-safe by construction.
	if (const USkeletalMeshComponent* WeaponMesh = OutWeaponActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (WeaponMesh->DoesSocketExist(ForegripSocket)) // a real bone-parented mesh socket, NOT a loose component
		{
			return EAFLWeaponHandling::TwoHanded;
		}
	}
	return EAFLWeaponHandling::OneHanded;
}

bool UAFLWeaponIKComponent::ResolveForegripComponentSpace(AActor* WeaponActor, USkeletalMeshComponent* Mesh, FVector& OutCompLoc, FQuat& OutCompRot) const
{
	if (!WeaponActor || !Mesh)
	{
		return false;
	}
	// Read the GripPoint_L SOCKET on the weapon's skeletal mesh -- the canonical IK interface: a real bone-parented
	// mesh socket carrying position AND rotation, NOT a loose scene component. No GripPoint_L socket -> don't engage
	// (never IK to a fallback = the old flail).
	const USkeletalMeshComponent* WeaponMesh = WeaponActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!WeaponMesh || !WeaponMesh->DoesSocketExist(ForegripSocket))
	{
		return false;
	}
	// FIX 2: world SOCKET transform -> COMPONENT space of the character mesh. No world-space target ever leaves here.
	const FTransform GripWorld = WeaponMesh->GetSocketTransform(ForegripSocket, RTS_World);
	const FTransform GripComp = GripWorld.GetRelativeTransform(Mesh->GetComponentTransform());
	OutCompLoc = GripComp.GetLocation();
	OutCompRot = GripComp.GetRotation(); // FIX 3: FQuat -- the SOCKET's rotation orients the hand
	return true;
}

FVector UAFLWeaponIKComponent::ComputeElbowPoleVector(const FVector& ShoulderCompPos, const FVector& TargetCompPos) const
{
	// Component-space cross-products so the elbow always bends outward + down, independent of weapon rotation
	// (the fix for the "loses the bend plane -> flips" failure mode; all in component space -> no world drift).
	const FVector ArmDirection = (TargetCompPos - ShoulderCompPos).GetSafeNormal();
	const FVector ChestForward = FVector::ForwardVector; // component-space +X
	const FVector ElbowOutwardAxis  = FVector::CrossProduct(ArmDirection, ChestForward).GetSafeNormal();
	const FVector ElbowDownwardAxis = FVector::CrossProduct(ElbowOutwardAxis, ArmDirection).GetSafeNormal();
	const FVector ArmMidpoint = ShoulderCompPos + (TargetCompPos - ShoulderCompPos) * 0.5f;
	return ArmMidpoint + (ElbowOutwardAxis * DynamicPoleVectorOutwardOffset) - (ElbowDownwardAxis * (DynamicPoleVectorOutwardOffset * 0.3f));
}

void UAFLWeaponIKComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (!Mesh)
	{
		CurrentAlpha = FMath::FInterpTo(CurrentAlpha, 0.0f, DeltaTime, InterpSpeed);
		CachedIKOutputs.LeftHandIKAlpha = CurrentAlpha;
		return;
	}
	EnsureArmLengths(Mesh);

	// --- CLASS-GATE FIRST (up front, not the reach-fallback): 2H solve / 1H cup / thrown|none off --------
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const ULyraEquipmentManagerComponent* EquipMgr = Pawn ? Pawn->FindComponentByClass<ULyraEquipmentManagerComponent>() : nullptr;
	if (!EquipMgr && Pawn && Pawn->GetController())
	{
		EquipMgr = Pawn->GetController()->FindComponentByClass<ULyraEquipmentManagerComponent>();
	}

	AActor* WeaponActor = nullptr;
	CurrentHandling = ResolveHandling(EquipMgr, WeaponActor);

	FVector TargetCompLoc = FVector::ZeroVector;
	FQuat   TargetCompRot = FQuat::Identity;
	bool bHaveTarget = false;

	switch (CurrentHandling)
	{
	case EAFLWeaponHandling::TwoHanded:
		// Solve hand_l to the weapon's own GripPoint_L foregrip, in component space (FIX 2).
		bHaveTarget = ResolveForegripComponentSpace(WeaponActor, Mesh, TargetCompLoc, TargetCompRot);
		break;

	case EAFLWeaponHandling::OneHanded:
		if (bOneHandedCupStance)
		{
			// CUP (AAA default): target = hand_r (component space) + the cup offset. NOT a weapon socket -> a
			// 1H weapon never points the reach at a nonexistent foregrip (the flinging trap is impossible here).
			const FVector HandRComp = Mesh->GetBoneLocation(HandRBone, EBoneSpaces::ComponentSpace);
			TargetCompLoc = HandRComp + OneHandedCupOffset;
			TargetCompRot = FQuat::Identity;
			bHaveTarget = true;
		}
		// else: free-hand -> no target -> alpha fades to 0 (the SSOT §1.4 baseline).
		break;

	case EAFLWeaponHandling::Thrown:
	case EAFLWeaponHandling::None:
	default:
		break; // off -- the class-gate turns the solver off BEFORE any resolve/reach math
	}

	// --- REACH-GATE SECOND (the 94% backstop): a flung / over-stretched arm can never ship ---------------
	TargetAlpha = 0.0f;
	if (bHaveTarget && bArmLenCached)
	{
		const FVector ShoulderComp = Mesh->GetBoneLocation(ShoulderBone, EBoneSpaces::ComponentSpace); // upperarm_l = chain root
		const float DistToTarget = FVector::Dist(ShoulderComp, TargetCompLoc);
		const float SafeReach = TotalArmLength * ArmLengthBufferPercent;
		if (DistToTarget <= SafeReach && DistToTarget >= MinReachDistance)
		{
			TargetAlpha = 1.0f;
			CachedIKOutputs.LeftHandTargetLocation = TargetCompLoc;
			CachedIKOutputs.LeftHandTargetRotation = TargetCompRot;
			CachedIKOutputs.LeftElbowPoleVector    = ComputeElbowPoleVector(ShoulderComp, TargetCompLoc);
		}
		// Past 94% (or jammed < MinReach): TargetAlpha stays 0 -> alpha fades out -> hand_l falls to the baked pose.
	}

	// --- Smooth the alpha (FInterpTo, DeltaSeconds-driven, frame-rate independent) -----------------------
	// On fade-out we keep the last target/pole so the hand eases off, exactly like the proven right-hand channel.
	CurrentAlpha = FMath::FInterpTo(CurrentAlpha, TargetAlpha, DeltaTime, InterpSpeed);
	CachedIKOutputs.LeftHandIKAlpha = CurrentAlpha;

	// --- Deliver to the native CR_AFL_CoreIK solve (Path B). Resolve the rig once (cached), then push the 3
	//     controls it reads. If no rig yet (pre-possession), we simply don't push -- alpha stays baked-pose safe.
	if (ResolveOwnerControlRig(Mesh))
	{
		PushToControlRig();
	}

}

UControlRig* UAFLWeaponIKComponent::ResolveOwnerControlRig(USkeletalMeshComponent* Mesh)
{
	if (CachedControlRig.IsValid())
	{
		return CachedControlRig.Get();
	}
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return nullptr;
	}
	// Engine-canonical (the proven grab-IK resolve): walk the anim instance class's anim-node properties, find the
	// FAnimNode_ControlRig struct, read its live UControlRig. This is the SAME rig (CR_AFL_IRONICS) the feet +
	// right-hand-grab IK run on -- our native CR_AFL_CoreIK left solve lives in it and reads the controls below.
	if (IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(AnimInstance->GetClass()))
	{
		for (const FStructProperty* NodeProp : AnimClass->GetAnimNodeProperties())
		{
			if (NodeProp && NodeProp->Struct && NodeProp->Struct->IsChildOf(FAnimNode_ControlRig::StaticStruct()))
			{
				const FAnimNode_ControlRig* CRNode = NodeProp->ContainerPtrToValuePtr<FAnimNode_ControlRig>(AnimInstance);
				if (CRNode)
				{
					if (UControlRig* Rig = CRNode->GetControlRig())
					{
						CachedControlRig = Rig;
						UE_LOG(LogAFLMovement, Log, TEXT("AFL_WEAPONIK: resolved Control Rig %s on %s."),
							*GetNameSafe(Rig), *GetNameSafe(GetOwner()));
						return Rig;
					}
				}
			}
		}
	}
	return nullptr;
}

void UAFLWeaponIKComponent::PushToControlRig()
{
	UControlRig* Rig = CachedControlRig.Get();
	if (!Rig)
	{
		return;
	}
	// FIX 2: these are COMPONENT-space (the rig's global space for a mesh-bound Control Rig) -- NEVER world-space
	// (world-space was the flung channel's bug). The native CR_AFL_CoreIK Basic IK reads:
	//   LeftHandIKTarget -> Effector.Translation,  AFL_LeftHandPole -> PoleVector,  LeftHandIKAlpha -> Weight.
	Rig->SetControlValue<FVector3f>(LeftHandTargetControl, FVector3f(CachedIKOutputs.LeftHandTargetLocation), /*bNotify*/ false);
	Rig->SetControlValue<FVector3f>(LeftHandPoleControl,   FVector3f(CachedIKOutputs.LeftElbowPoleVector),    /*bNotify*/ false);
	Rig->SetControlValue<float>(LeftHandAlphaControl,      CachedIKOutputs.LeftHandIKAlpha,                    /*bNotify*/ false);
	// FIX 3 delivery: the socket's ROTATION (component-space FQuat) orients the hand at the grip. The native
	// CR_AFL_CoreIK Basic IK reads AFL_LeftHandRot into its Effector.Rotation (the load-bearing socket's 2nd axis).
	Rig->SetControlValue<FRotator>(LeftHandRotControl, CachedIKOutputs.LeftHandTargetRotation.Rotator(),      /*bNotify*/ false);
}
