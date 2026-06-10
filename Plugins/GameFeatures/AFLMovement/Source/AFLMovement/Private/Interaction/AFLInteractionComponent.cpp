// Copyright C12 AI Gaming. All Rights Reserved.

#include "Interaction/AFLInteractionComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_ControlRig.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "Engine/EngineTypes.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/AFLGrabbableComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLInteractionComponent)

// Same tag the climb GE grants. Listening here lets carry drop the held object when a climb starts.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Climbing_Interaction, "State.Movement.Climbing");

UAFLInteractionComponent::UAFLInteractionComponent()
{
	// Tick drives the hand-IK push into the Control Rig each frame while bHandIKEnabled (Cycle 4c, Path 1:
	// the component -- not the grab ability -- owns delivery to the rig, so the cheat works standalone). Tick
	// is otherwise cheap: a single bool check + early-out when the IK is idle.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAFLInteractionComponent::GetHandIKState(FVector& OutTarget, bool& bOutEnabled, float& OutAlpha) const
{
	// Read-only snapshot, kept for any AnimGraph Property Access binding / BP consumer. Marked
	// BlueprintThreadSafe so the AnimGraph evaluator can call it from the animation worker thread. NOTE: the
	// live IK is driven by TickComponent pushing straight into the Control Rig controls (Path 1); this getter
	// is no longer on the critical path but is harmless and free to keep.
	OutTarget = HandIKTarget;
	bOutEnabled = bHandIKEnabled;
	OutAlpha = HandIKAlpha;
}

void UAFLInteractionComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bHandIKEnabled)
	{
		// Active: resolve the rig (lazy, cached) and push target + alpha every frame.
		if (ResolveOwnerControlRig())
		{
			PushHandIKToControlRig();
			bHandIKReleasedToRig = false;
		}
	}
	else if (!bHandIKReleasedToRig)
	{
		// Just disabled: push one final alpha=0 so the rig releases the hand to the clip pose, then stop
		// pushing every idle frame and drop the cached rig (a re-possess/mesh swap re-resolves next enable).
		if (UControlRig* Rig = CachedControlRig.Get())
		{
			Rig->SetControlValue<float>(HandIKAlphaControl, 0.0f, /*bNotify*/ false);
		}
		bHandIKReleasedToRig = true;
		CachedControlRig.Reset();
	}
}

void UAFLInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// ASC resolve: DIRECT first, PawnExtension hook FALLBACK for the possessed player. The exact pattern
	// proven on B_Hero_BagMan by UAFLDashMovementComponent / UAFLClimbMovementComponent.
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
		else if (ULyraPawnExtensionComponent* PawnExt = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Owner))
		{
			PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
				FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemReady));
		}
	}
}

void UAFLInteractionComponent::OnAbilitySystemReady()
{
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: %s OnAbilitySystemReady -> binding climb-tag listener."),
		*GetNameSafe(GetOwner()));
	if (AActor* Owner = GetOwner())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
		{
			BindToAbilitySystem(ASC);
		}
	}
}

void UAFLInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CarriedActor.IsValid())
	{
		ReleaseActor();
	}
	UnbindFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UAFLInteractionComponent::BindToAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC)
	{
		return;
	}
	if (CachedASC.Get() == InASC && ClimbTagChangedHandle.IsValid())
	{
		return; // idempotent
	}
	if (CachedASC.IsValid() && CachedASC.Get() != InASC)
	{
		UnbindFromAbilitySystem();
	}

	CachedASC = InASC;
	ClimbTagChangedHandle = InASC->RegisterGameplayTagEvent(
			TAG_State_Movement_Climbing_Interaction, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLInteractionComponent::HandleClimbTagChanged);

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: %s bound climb-tag listener (ASC %s)."),
		*GetNameSafe(GetOwner()), *GetNameSafe(InASC));
}

void UAFLInteractionComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (ClimbTagChangedHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Movement_Climbing_Interaction, EGameplayTagEventType::NewOrRemoved)
				.Remove(ClimbTagChangedHandle);
		}
	}
	ClimbTagChangedHandle.Reset();
	CachedASC.Reset();
}

void UAFLInteractionComponent::HandleClimbTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Climb just STARTED (tag added) while we're carrying -> drop the held object (Decision H).
	if (NewCount > 0 && CarriedActor.IsValid())
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: forced-release (reason=climb-start)."));
		ReleaseActor();
	}
}

bool UAFLInteractionComponent::GrabActor(AActor* Target, const FAFLGrabPolicy& Policy)
{
	if (!Target || CarriedActor.IsValid())
	{
		return false; // nothing to grab, or already carrying (one object at a time in the proof).
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* HeroMesh = Character ? Character->GetMesh() : nullptr;
	if (!HeroMesh)
	{
		return false;
	}

	// HYBRID hold: turn the actor inert, then snap it to the hand socket so it rides rigidly (kinematic
	// parent-follow). Release re-enables physics + impulse.
	//
	// ORDER IS CRITICAL (this is what the "Invalid Simulate Options: set to simulate physics but Collision
	// Enabled is incompatible" PIE warning was telling us -- and the cause of the jitter/push-away/roll): a body
	// that is STILL SIMULATING when you set NoCollision enters a broken half-state. So per primitive, in THIS
	// order: zero velocity -> STOP SIMULATING (while collision is still valid) -> THEN drop collision. Now there
	// is never a "simulating + no-collision" frame. Done on EVERY primitive (the box's mesh is a child of a bare
	// SceneComponent root). Then attach with weld off so the socket owns the transform and the inert body follows.
	const bool bSocketExists = HeroMesh->DoesSocketExist(Policy.HoldSocketName);
	int32 PrimCount = 0;
	Target->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors*/ true,
		[&PrimCount](UPrimitiveComponent* P)
		{
			if (P->IsSimulatingPhysics())
			{
				P->SetPhysicsLinearVelocity(FVector::ZeroVector);
				P->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			}
			P->SetSimulatePhysics(false);              // sim OFF first (collision still valid -> no warning)
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision); // then drop collision on the now-inert body
			++PrimCount;
		});

	const bool bAttached = Target->AttachToComponent(
		HeroMesh,
		FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, /*bWeldSimulatedBodies*/ false),
		Policy.HoldSocketName);

	// HOLD OFFSET: SnapToTarget zeroes the relative transform, so the actor's origin lands EXACTLY on the
	// hand_r bone -- which is inside the wrist/forearm mesh, so the object is buried and invisible (the
	// dist=0.0 diagnostic proved this). Push it out along the socket's local axes so it sits in front of the
	// palm where the player can see it.
	if (USceneComponent* Root = Target->GetRootComponent())
	{
		Root->SetRelativeLocation(Policy.HoldOffset);
	}

	CarriedActor = Target;
	ActivePolicy = Policy;

	if (UAFLGrabbableComponent* Grab = Target->FindComponentByClass<UAFLGrabbableComponent>())
	{
		Grab->SetHeld(true);
	}

	// Holster the rifle for the carry (the rifle's upper-body anim layer would fight the grab reach + hold).
	HolsterEquippedWeapon();

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: attached %s at socket=%s (offset=%s, socketExists=%d attachOk=%d, physOff=%d prims)."),
		*GetNameSafe(Target), *Policy.HoldSocketName.ToString(), *Policy.HoldOffset.ToCompactString(),
		bSocketExists ? 1 : 0, bAttached ? 1 : 0, PrimCount);
	return true;
}

void UAFLInteractionComponent::ReleaseActor()
{
	AActor* Target = CarriedActor.Get();
	if (!Target)
	{
		return;
	}

	// Clear the held flag first so re-discovery can offer it again.
	if (UAFLGrabbableComponent* Grab = Target->FindComponentByClass<UAFLGrabbableComponent>())
	{
		Grab->SetHeld(false);
	}

	// Restore the holstered rifle (symmetric with HolsterEquippedWeapon in GrabActor).
	RestoreEquippedWeapon();

	// Detach KEEPING the world transform (it stays where the hand left it, not snapped back to origin).
	Target->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// Restore collision on EVERY primitive we disabled on grab (symmetric with GrabActor's all-primitives
	// disable), so the dropped object lands and is re-grabbable.
	Target->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors*/ true,
		[](UPrimitiveComponent* P)
		{
			P->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		});

	// The body that actually simulates (the mesh): the box root is a bare SceneComponent, so find the first
	// real primitive for the re-simulate + impulse. (Re-enabling physics on the bare root would no-op anyway.)
	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent());
	if (!Prim)
	{
		Prim = Target->FindComponentByClass<UPrimitiveComponent>();
	}
	if (Prim && ActivePolicy.bEnablePhysicsOnRelease)
	{
		// ORDER MATTERS: physics must be re-enabled BEFORE the impulse or AddImpulse no-ops on a non-simulating body.
		Prim->SetSimulatePhysics(true);

		// "Slight ragdoll": a modest impulse along the hero's view-forward+up, scaled by mass so heavy and light
		// objects feel consistent. Not a gravity-gun launch -- the object falls/tumbles and settles.
		const AActor* Owner = GetOwner();
		const FVector Fwd = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
		const FVector Up = FVector::UpVector;
		FVector Dir = (Fwd * ActivePolicy.ReleaseImpulseDirection.X) + (Up * ActivePolicy.ReleaseImpulseDirection.Z);
		Dir = Dir.GetSafeNormal();
		const float Mass = Prim->GetMass();
		Prim->AddImpulse(Dir * ActivePolicy.ReleaseImpulseMagnitude * Mass);

		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: detached, impulse applied (magnitude=%.0f, mass=%.1f)."),
			ActivePolicy.ReleaseImpulseMagnitude, Mass);
	}
	else
	{
		// 1E DIAGNOSTIC: the policy said no-physics-on-release. Print the value so we can see if ActivePolicy
		// is stale/default vs the grabbable's true value (instance reads true -> this should now be true too).
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: detached (no physics-on-release per policy; bEnablePhysicsOnRelease=%d)."),
			ActivePolicy.bEnablePhysicsOnRelease ? 1 : 0);
	}

	CarriedActor.Reset();
}

void UAFLInteractionComponent::HolsterEquippedWeapon()
{
	HolsteredWeaponActors.Reset();

	// EquipmentManager lives on the PlayerState for this hero (Lyra ranged-weapon pattern). Our owner is the
	// pawn -> reach the PS. Same shape as UAFLClimbMovementComponent's holster.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	AActor* PSActor = Pawn ? Cast<AActor>(Pawn->GetPlayerState()) : nullptr;
	ULyraEquipmentManagerComponent* EquipMgr =
		PSActor ? PSActor->FindComponentByClass<ULyraEquipmentManagerComponent>() : nullptr;
	if (!EquipMgr)
	{
		return; // no manager (pre-possession) -> nothing to holster; the carry still works.
	}

	int32 HiddenCount = 0;
	for (ULyraEquipmentInstance* Instance : EquipMgr->GetEquipmentInstancesOfType(ULyraEquipmentInstance::StaticClass()))
	{
		if (!Instance)
		{
			continue;
		}
		for (AActor* WeaponActor : Instance->GetSpawnedActors())
		{
			if (WeaponActor && WeaponActor->GetRootComponent() && WeaponActor->IsHidden() == false)
			{
				WeaponActor->SetActorHiddenInGame(true);
				HolsteredWeaponActors.Add(WeaponActor);
				++HiddenCount;
			}
		}
	}

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: rifle holstered (%d weapon actor(s)) for carry."), HiddenCount);
}

void UAFLInteractionComponent::RestoreEquippedWeapon()
{
	int32 RestoredCount = 0;
	for (const TWeakObjectPtr<AActor>& WeakActor : HolsteredWeaponActors)
	{
		if (AActor* WeaponActor = WeakActor.Get())
		{
			WeaponActor->SetActorHiddenInGame(false);
			++RestoredCount;
		}
	}
	HolsteredWeaponActors.Reset();

	if (RestoredCount > 0)
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: rifle restored (%d weapon actor(s)) after carry."), RestoredCount);
	}
}

UControlRig* UAFLInteractionComponent::ResolveOwnerControlRig()
{
	if (CachedControlRig.IsValid())
	{
		return CachedControlRig.Get();
	}

	// The rig runs on the AVATAR pawn's mesh (the hero's SKM_Manny). Our owner IS the pawn.
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return nullptr;
	}

	// Engine-canonical accessor (same pattern proven in the grab ability, whose resolve already logged
	// CR_AFL_IRONICS_C_0 in PIE): walk the anim instance class's anim-node properties, find the
	// FAnimNode_ControlRig struct, read its live UControlRig.
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
						UE_LOG(LogAFLMovement, Log, TEXT("AFL_HANDIK: resolved hand-IK Control Rig %s on %s."),
							*GetNameSafe(Rig), *GetNameSafe(GetOwner()));
						return Rig;
					}
				}
			}
		}
	}
	UE_LOG(LogAFLMovement, Warning, TEXT("AFL_HANDIK: no FAnimNode_ControlRig found on %s's anim instance."),
		*GetNameSafe(GetOwner()));
	return nullptr;
}

void UAFLInteractionComponent::PushHandIKToControlRig()
{
	UControlRig* Rig = CachedControlRig.Get();
	if (!Rig)
	{
		return;
	}
	// HandIKTarget is a world-space Position control; HandIKAlpha is the IK weight (0..1). The rig's additive
	// Basic IK (Sequence.E, after foot-plant) solves the right hand onto the target at this weight.
	Rig->SetControlValue<FVector3f>(HandIKTargetControl, FVector3f(HandIKTarget), /*bNotify*/ false);
	Rig->SetControlValue<float>(HandIKAlphaControl, bHandIKEnabled ? HandIKAlpha : 0.0f, /*bNotify*/ false);
}
