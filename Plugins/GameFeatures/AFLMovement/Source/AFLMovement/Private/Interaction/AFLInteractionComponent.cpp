// Copyright C12 AI Gaming. All Rights Reserved.

#include "Interaction/AFLInteractionComponent.h"

#include "AFLMovement.h"
#include "Abilities/AFLGameplayAbility_Grab.h"
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
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Interaction/AFLGrabbableComponent.h"
#include "Interaction/AFLObjectClassAnimSet.h"
#include "Messages/AFLHitConfirmMessage.h"   // AFLCore payload for the drop-on-damage listen
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLInteractionComponent)

// Same tag the climb GE grants. Listening here lets carry drop the held object when a climb starts.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Climbing_Interaction, "State.Movement.Climbing");

// The per-hit damage verb UAFLDamageExecCalc broadcasts (Event.Damage.Confirmed, EffectiveDamage > 0;
// fires BEFORE the shield split, so a fully-shielded hit still counts as "hit"). Native-define per the
// file convention so module init never races the per-plugin tag-ini scan.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Confirmed_Interaction, "Event.Damage.Confirmed");

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

	// Hand-IK drive (4c Path 1 + 4f alpha fade): the alpha FADES toward its goal via FInterpTo
	// (DeltaSeconds-driven -- the skill rule: never hard-switch an IK weight 0/1). While enabled OR still
	// fading out, resolve + push every frame; when a disable's fade reaches ~0, push the final 0 and release
	// the cached rig. The console cheat writes HandIKAlpha directly (Set=1 / Clear=0), which simply lands
	// the fade at its endpoint -- cheat behavior is unchanged.
	const float AlphaGoal = bHandIKEnabled ? HandIKAlphaGoal : 0.0f;
	if (bHandIKEnabled || HandIKAlpha > KINDA_SMALL_NUMBER)
	{
		HandIKAlpha = FMath::FInterpTo(HandIKAlpha, AlphaGoal, DeltaTime, HandIKInterpSpeed);
		if (ResolveOwnerControlRig())
		{
			PushHandIKToControlRig();
			bHandIKReleasedToRig = false;
		}
	}
	else if (!bHandIKReleasedToRig)
	{
		// Fade complete (or never started): push one final alpha=0 so the rig releases the hand to the clip
		// pose, then stop pushing every idle frame and drop the cached rig (a re-possess/mesh swap re-resolves
		// next enable).
		if (UControlRig* Rig = CachedControlRig.Get())
		{
			Rig->SetControlValue<float>(HandIKAlphaControl, 0.0f, /*bNotify*/ false);
		}
		bHandIKReleasedToRig = true;
		CachedControlRig.Reset();
	}

	// 4f carry diagnostic (1 Hz, Verbose): makes "attached" a readable log invariant instead of a visual
	// guess. One line per second while carrying: the held actor's root/prim world positions, the prim's
	// attach parent, and the pawn position. The stay-in-place bug class (engine detach-on-sim splitting the
	// held actor's hierarchy) is instantly legible here -- primParent=None while carrying is the tell.
	if (CarriedActor.IsValid())
	{
		CarryDiagAccum += DeltaTime;
		if (CarryDiagAccum >= 1.0f)
		{
			CarryDiagAccum = 0.0f;
			AActor* Held = CarriedActor.Get();
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Held->GetRootComponent());
			if (!Prim)
			{
				Prim = Held->FindComponentByClass<UPrimitiveComponent>();
			}
			const USceneComponent* PrimParent = Prim ? Prim->GetAttachParent() : nullptr;
			UE_LOG(LogAFLMovement, Verbose,
				TEXT("AFL_CARRY-DIAG: held=%s primParent=%s rootLoc=%s primLoc=%s pawnLoc=%s"),
				*Held->GetName(), *GetNameSafe(PrimParent),
				*Held->GetActorLocation().ToCompactString(),
				Prim ? *Prim->GetComponentLocation().ToCompactString() : TEXT("?"),
				*GetOwner()->GetActorLocation().ToCompactString());
		}
	}
	else
	{
		CarryDiagAccum = 0.0f;
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
		// Teardown while carrying (pawn death/destroy): the grab ability lives on the PlayerState ASC,
		// which OUTLIVES this pawn -- without the cancel, State.Carrying (+ the per-object carrier GE)
		// rides across the respawn. Safe here: EndAbility's avatar-side work is null-guarded, its
		// ReleaseActor() no-ops (!Target), and at world death CachedASC is stale so the helper early-outs
		// (GE removal moot when the whole ASC is going away).
		CancelOwningGrabAbility();
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

	// Drop-on-damage listen (the climb bind's sibling, registered at the same lifecycle site). The
	// broadcast side is WITH_SERVER_CODE in UAFLDamageExecCalc, so this fires on the server/listen-server
	// only -- exactly the authoritative side that owns the release; clients see the resulting drop through
	// the existing release replication (2-client carry-forward scope, unchanged).
	if (!DamageMessageHandle.IsValid())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
		DamageMessageHandle = MessageSubsystem.RegisterListener(
			TAG_Event_Damage_Confirmed_Interaction,
			this,
			&UAFLInteractionComponent::HandleDamageConfirmed);
	}

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: %s bound climb-tag + damage-confirmed listeners (ASC %s)."),
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

	// Drop-on-damage teardown (mirror of the climb unbind; the subsystem handles a dead world gracefully).
	if (DamageMessageHandle.IsValid())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
		MessageSubsystem.UnregisterListener(DamageMessageHandle);
	}
}

void UAFLInteractionComponent::HandleClimbTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Climb just STARTED (tag added) while we're carrying -> drop the held object (Decision H).
	if (NewCount > 0 && CarriedActor.IsValid())
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: forced-release (reason=climb-start)."));
		ReleaseActor();            // Drop-mode release with the policy impulse (semantics unchanged).
		CancelOwningGrabAbility(); // funnel: the grab's EndAbility clears State.Carrying (leak fix).
	}
}

void UAFLInteractionComponent::HandleDamageConfirmed(FGameplayTag Channel, const FAFLHitConfirmMessage& Msg)
{
	// Verbose receipt diagnostic: makes the filter path legible in a PIE log (whose hit, are we carrying,
	// what the policy says) -- the instrument that splits "filter mismatch" from "test never ran carrying".
	UE_LOG(LogAFLMovement, Verbose,
		TEXT("[AFLInteraction] damage-confirmed rx: target=%s owner=%s carrying=%d dropOnDamage=%d dmg=%.1f"),
		*GetNameSafe(Msg.Target), *GetNameSafe(GetOwner()),
		CarriedActor.IsValid() ? 1 : 0, ActivePolicy.bDropOnDamage ? 1 : 0, Msg.Damage);

	// Drop-on-damage (the final Scope-A verb): the carrier got HIT -> the carried object releases through
	// the same forced-drop funnel the climb uses. Ordered cheap-out-first so the global per-hit broadcast
	// costs nothing when it is not about us / we are not carrying.
	if (Msg.Target != GetOwner())
	{
		return; // someone else's hit.
	}
	if (!CarriedActor.IsValid())
	{
		return; // not carrying -- clean no-op, no log spam.
	}
	if (!ActivePolicy.bDropOnDamage)
	{
		return; // per-object opt-out (policy captured at grab time).
	}

	UE_LOG(LogAFLMovement, Log, TEXT("[AFLInteraction] forced-release (reason=damage, dmg=%.1f bone=%s)"),
		Msg.Damage, *Msg.BoneName.ToString());
	ReleaseActor();            // Drop-mode release with the policy impulse (the climb forced-drop shape).
	CancelOwningGrabAbility(); // funnel: the grab's EndAbility clears State.Carrying (leak fix).
}

void UAFLInteractionComponent::CancelOwningGrabAbility()
{
	UAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		return;
	}
	// The throw's cancel-by-class seam (UAFLGameplayAbility_Throw), reused: find the active grab spec and
	// cancel it so its EndAbility -- the single state-removal funnel -- runs exactly once. Safe re-entrant:
	// EndAbility's ReleaseActor() no-ops (CarriedActor already cleared by the release that preceded us).
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.IsActive() && Spec.Ability->IsA<UAFLGameplayAbility_Grab>())
		{
			ASC->CancelAbilityHandle(Spec.Handle);
		}
	}
}

UAFLObjectClassAnimSet* UAFLInteractionComponent::ResolveAndCacheAnimSet(const FAFLGrabPolicy& Policy)
{
	// 4e resolve, moved PRE-REACH in 4f: the grab ability calls this before anything plays, so the per-class
	// reach/carry montages are known before the attach. Policy's set, else the designer fallback.
	// LoadSynchronous is fine here (a small data asset; the montages it soft-references load when played).
	const TSoftObjectPtr<UAFLObjectClassAnimSet>& SetRef =
		Policy.ObjectAnimSet.IsNull() ? DefaultAnimSet : Policy.ObjectAnimSet;
	ActiveAnimSet = SetRef.LoadSynchronous();
	const FGameplayTag ResolvedClass = ActiveAnimSet ? ActiveAnimSet->ObjectClass : FGameplayTag();
	UE_LOG(LogAFLMovement, Log,
		TEXT("[AFLInteraction] Grab resolved ObjectClass=%s AnimSet=%s (pre-reach)"),
		*ResolvedClass.ToString(), *GetNameSafe(ActiveAnimSet));
#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			FString::Printf(TEXT("GRAB  class=%s  set=%s"),
				*ResolvedClass.ToString(), *GetNameSafe(ActiveAnimSet)));
	}
#endif
	return ActiveAnimSet;
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
		[&PrimCount, Target](UPrimitiveComponent* P)
		{
			if (P->IsSimulatingPhysics())
			{
				P->SetPhysicsLinearVelocity(FVector::ZeroVector);
				P->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			}
			P->SetSimulatePhysics(false);              // sim OFF first (collision still valid -> no warning)
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision); // then drop collision on the now-inert body
			++PrimCount;

			// 4f tripwire: a NON-ROOT primitive with no attach parent is detach-on-sim residue -- the engine
			// permanently detaches a child prim from its parent the moment SetSimulatePhysics(true) hits it
			// (BodyInstance.cpp: "we detach the component"), which was the 4f stay-in-place bug. Prim-as-root
			// grabbables never trip this; any future child-prim authoring self-reports here instead of
			// shipping a box that silently stays behind.
			if (P != Target->GetRootComponent() && P->GetAttachParent() == nullptr)
			{
				UE_LOG(LogAFLMovement, Warning,
					TEXT("AFL_GRAB: non-root primitive %s on %s has no attach parent (detach-on-sim residue) -- the visual will NOT follow the hand."),
					*P->GetName(), *GetNameSafe(Target));
			}
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

	// 4f: the per-class anim set is resolved+cached BEFORE the reach by ResolveAndCacheAnimSet (the ability
	// calls it pre-reach so the montages are known before this attach runs). GrabActor rides that cache --
	// no resolve here. ReleaseActor clears it.

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

void UAFLInteractionComponent::ReleaseActor(EAFLReleaseMode Mode, const FVector& ThrowDirection)
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
		// ORDER MATTERS: physics must be re-enabled BEFORE any velocity/impulse call or they no-op on a
		// non-simulating body.
		Prim->SetSimulatePhysics(true);

		const AActor* Owner = GetOwner();

		// Skill momentum hand-off (BOTH modes): seed the freed body with the carrier's CURRENT velocity
		// before the impulse, so a moving thrower's momentum carries into the object (a sprint-throw
		// visibly outranges a standing throw). AddImpulse then stacks the launch on top. The hero mesh is
		// kinematic, so the pawn velocity IS the practical hand-velocity source.
		Prim->SetPhysicsLinearVelocity(Owner ? Owner->GetVelocity() : FVector::ZeroVector);

		// Direction + magnitude by mode. Drop = the policy's actor-forward+up "slight ragdoll" shape (modest,
		// NOT a gravity-gun launch). Throw = the caller's aim direction, FULL 3D -- pitch included, never
		// flattened -- at the policy's throw power. Both are mass-scaled so heavy and light feel consistent.
		const FVector Fwd = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
		const FVector DropDir =
			((Fwd * ActivePolicy.ReleaseImpulseDirection.X) + (FVector::UpVector * ActivePolicy.ReleaseImpulseDirection.Z)).GetSafeNormal();

		FVector Dir = DropDir;
		float Magnitude = ActivePolicy.ReleaseImpulseMagnitude;
		if (Mode == EAFLReleaseMode::Throw)
		{
			Magnitude = ActivePolicy.ThrowImpulseMagnitude;
			const FVector AimDir = ThrowDirection.GetSafeNormal();
			if (AimDir.IsNearlyZero())
			{
				UE_LOG(LogAFLMovement, Warning,
					TEXT("AFL_GRAB: Throw release with a zero direction -- falling back to the Drop direction."));
			}
			else
			{
				Dir = AimDir;
			}
		}

		const float Mass = Prim->GetMass();
		Prim->AddImpulse(Dir * Magnitude * Mass);

		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: detached, %s impulse applied (magnitude=%.0f, mass=%.1f, dir=%s)."),
			Mode == EAFLReleaseMode::Throw ? TEXT("THROW") : TEXT("drop"), Magnitude, Mass, *Dir.ToCompactString());
	}
	else
	{
		// 1E DIAGNOSTIC: the policy said no-physics-on-release. Print the value so we can see if ActivePolicy
		// is stale/default vs the grabbable's true value (instance reads true -> this should now be true too).
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: detached (no physics-on-release per policy; bEnablePhysicsOnRelease=%d)."),
			ActivePolicy.bEnablePhysicsOnRelease ? 1 : 0);
	}

	CarriedActor.Reset();
	ActiveAnimSet = nullptr; // 4e: drop the per-class anim-set cache with the carried actor.
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
