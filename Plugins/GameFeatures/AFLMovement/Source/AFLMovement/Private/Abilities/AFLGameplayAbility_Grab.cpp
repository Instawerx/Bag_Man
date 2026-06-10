// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Grab.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameplayEffect.h"
#include "Interaction/AFLGrabbableComponent.h"
#include "Interaction/AFLInteractionComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Grab)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Grab_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Grab_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Grab_State_Extracting, "State.Extracting");

UAFLGameplayAbility_Grab::UAFLGameplayAbility_Grab(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// TOGGLE lifecycle (Cycle 4d grab-hold fix): OnInputTriggered, NOT WhileInputActive. WhileInputActive is
	// auto-canceled by the Lyra ASC the instant the input TAG releases -- and IA_Ability_Grab's malformed
	// (null) trigger collapses press->release into one frame, so the ASC killed the grab the same frame it
	// activated (the "tap, shove, spin" symptom -- attach+detach+impulse all in one frame). OnInputTriggered
	// abilities live until WE end them. Grab now toggles: first press grabs+holds (stays active with input
	// released), second press drops (InputPressed -> EndAbility). Immune to the null-trigger (BM-DEBT-INPUT-001).
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

	ActivationBlockedTags.AddTag(TAG_Grab_State_Match_Warmup);
	ActivationBlockedTags.AddTag(TAG_Grab_State_Match_Ended);
	ActivationBlockedTags.AddTag(TAG_Grab_State_Extracting);
}

AActor* UAFLGameplayAbility_Grab::ResolveTargetActor(const FGameplayEventData* TriggerEventData) const
{
	// PATH 1 -- Lyra interaction grant: ULyraGameplayAbility_Interact::TriggerInteraction sets Payload.Target
	// to the grabbable's owning actor. Used when grab is activated through Lyra's look-at discovery (future
	// proximity/prompt cycle). Present its target if it gave one.
	if (TriggerEventData)
	{
		if (const AActor* T = Cast<AActor>(TriggerEventData->Target.Get()))
		{
			return const_cast<AActor*>(T);
		}
		if (const AActor* O = Cast<AActor>(TriggerEventData->OptionalObject))
		{
			return const_cast<AActor*>(O);
		}
	}

	// PATH 2 -- input-activated (press Grab with no interaction event): the ability finds its OWN target. A
	// directional eye-trace FLIES OVER floor objects (the player would have to look straight down at a box on
	// the ground). So instead we OVERLAP a big sphere centered in front of the player at mid-body height and
	// take the nearest grabbable -- height-agnostic, forgiving acquisition (the brand-theme), and it catches a
	// box on the floor whether the player is looking at it or over it.
	const ACharacter* Character = GetCurrentActorInfo() ? Cast<ACharacter>(GetCurrentActorInfo()->AvatarActor.Get()) : nullptr;
	const UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		return nullptr;
	}

	// Sphere center: GrabReachDistance forward of the actor, at the actor's base height (so the sphere spans
	// from the floor up to chest -- a box on the ground at the player's feet/front is inside it).
	const FVector ActorLoc = Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector();
	const FVector SphereCenter = ActorLoc + Forward * (GrabReachDistance * 0.5f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLGrabAcquire), /*bTraceComplex*/ false, Character);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps, SphereCenter, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(GrabReachDistance), Params);

	// Pick the NEAREST overlapping actor that has a grabbable component (and isn't already held).
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Actor = O.GetActor();
		if (!Actor)
		{
			continue;
		}
		UAFLGrabbableComponent* G = Actor->FindComponentByClass<UAFLGrabbableComponent>();
		if (!G || G->IsHeld())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(ActorLoc, Actor->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Actor;
		}
	}
	return Best;
}

void UAFLGameplayAbility_Grab::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bExiting = false;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: Activate by %s."), *GetNameSafe(Avatar));

	// 1. Resolve + validate the target: still exists, still grabbable, not already held.
	AActor* Target = ResolveTargetActor(TriggerEventData);
	UAFLGrabbableComponent* Grabbable = Target ? Target->FindComponentByClass<UAFLGrabbableComponent>() : nullptr;
	if (!Grabbable || Grabbable->IsHeld())
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: no valid grabbable target (target=%s) -> cancel."), *GetNameSafe(Target));
		CancelAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateCancelAbility*/ true);
		return;
	}
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: validated (target=%s)."), *GetNameSafe(Target));

	// 1b. (Cycle 4d grab-hold fix) -- the orientation snap is GONE. It did SetActorRotation(TeleportPhysics) +
	// bUseControllerRotationYaw=false to "face the target," which teleport-snapped the whole body to a different
	// yaw on every grab (the quarter-spin / "backwards-inverted" symptom). The box attaches to hand_r regardless
	// of body facing, so the snap bought nothing. No orientation change on grab now.

	// 2. The hero's interaction component performs the attach/hold.
	InteractionComponent = Avatar ? Avatar->FindComponentByClass<UAFLInteractionComponent>() : nullptr;
	if (!InteractionComponent.IsValid())
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: avatar has no UAFLInteractionComponent -> cancel."));
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}
	if (!InteractionComponent->GrabActor(Target, Grabbable->GetGrabPolicy()))
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: GrabActor failed -> cancel."));
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	// 3. Grant State.Carrying (the future penalty cycle + climb-block read this; mirror climb's GE shape).
	if (CarryingEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CarryingEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: state applied -> carrying."));
		}
	}

	// 4. (Cycle 4d grab-hold fix) TOGGLE -- no WaitInputRelease. The grab is NOT input-release-driven anymore;
	// it stays active (carrying) after ActivateAbility returns, with the input released. A SECOND press routes
	// to InputPressed() -> EndAbility (the drop). This is what makes "hold the box with G released" work.
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: carrying -- press again to drop."));

	// 4b. Hand-IK (Cycle 4c, Control Rig path): the hero's UAFLInteractionComponent OWNS the per-frame push of
	//     HandIKTarget/HandIKAlpha into CR_AFL_IRONICS (Path 1 -- its TickComponent resolves the rig and drives
	//     it whenever bHandIKEnabled, so the IK works standalone via afl.HandIK.Set, not only during a grab).
	//     Cycle 4d / decision W1: the grab DELIBERATELY does NOT enable the hand-IK. The box snaps to hand_r at
	//     attach, so an IK target "on the box" is geometrically where the hand already is -- driving it would
	//     fight the snap for no visible reach. Wiring grab -> hand-reaches-the-box-at-rest needs a reach-then-
	//     attach flow and belongs to the 4f grab-flow restructure. The console afl.HandIK.* path stays a fully
	//     independent capability (it writes the component's fields directly). No rig code lives in the ability.

	// 5. Play the grab reach on the UPPER-BODY slot (Lyra-canonical layered-upper-body: arms reach,
	//    legs keep following locomotion via the AnimGraph's Layered Blend Per Bone). The slot binding
	//    lives here in gameplay, not baked into a montage asset, and it sidesteps montage-slot
	//    registration. PlaySlotAnimationAsDynamicMontage drives the named slot directly.
	//    DefaultSlot is NOT routed in Lyra's AnimGraph, which is why the old montage played into the
	//    void (log fired, body showed nothing). See the slot audit in the grab-composition lane.
	if (UAnimInstance* AnimInstance = GetGrabAnimInstance(ActorInfo))
	{
		if (GrabReachAnim)
		{
			// Self-validating: list the slot names the SKELETON actually registers (the registry the
			// engine validates against) so a PIE run PROVES whether GrabReachSlot ("UpperBody") is a
			// real registered slot, rather than us guessing from editor reflection.
			TStringBuilder<256> SlotList;
			if (const USkeletalMeshComponent* DiagMesh = AnimInstance->GetSkelMeshComponent())
			{
				if (const USkeletalMesh* SkelMesh = DiagMesh->GetSkeletalMeshAsset())
				{
					if (const USkeleton* Skeleton = SkelMesh->GetSkeleton())
					{
						for (const FAnimSlotGroup& Group : Skeleton->GetSlotGroups())
						{
							for (const FName& SlotName : Group.SlotNames)
							{
								SlotList << SlotName << TEXT(" ");
							}
						}
					}
				}
			}

			// Cycle 4d -- SUSTAINED carry pose. The grab-composition lane proved the UpperBody slot reaches the
			// pose at full weight (global=local=1.000) layered over locomotion (legs free), but the clip is a
			// reach-and-RETURN gesture (~2s) so the carry stance relaxed after the reach. The 4d fix FREEZES the
			// clip at its reach-peak frame for the whole carry: play at rate 0 starting at GrabHoldTime, so the
			// montage holds that single pose until EndAbility's StopSlotAnimation releases it. Rate 0 + the
			// default BlendOutTriggerTime=-1 means no auto blend-out -- the pose holds indefinitely.
			// (bGrabHoldPose=false restores the 4c transient one-shot reach: rate 1.0, plays through once.)
			const float HoldRate = bGrabHoldPose ? 0.0f : 1.0f;
			const float StartAt = bGrabHoldPose ? GrabHoldTime : 0.0f;

			// PlaySlotAnimationAsDynamicMontage returns the created montage -- non-null is the authoritative
			// success signal (it creates the transient montage and starts it on the slot). It returns null
			// only on a bad asset/slot. IsSlotActive is informational corroboration (may lag a frame on the
			// very first tick, so it is NOT part of the pass/fail verdict).
			const UAnimMontage* DynMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
				GrabReachAnim, GrabReachSlot, GrabReachBlendTime, GrabReachBlendTime,
				/*InPlayRate*/ HoldRate, /*LoopCount*/ 1, /*BlendOutTriggerTime*/ -1.0f,
				/*InTimeToStartMontageAt*/ StartAt);
			const bool bSlotActive = AnimInstance->IsSlotActive(GrabReachSlot);

			UE_LOG(LogAFLMovement, Log,
				TEXT("AFL_GRAB: carry-pose play (anim=%s slot=%s hold=%d freezeAt=%.2fs) montage=%s slotActive=%d%s | registered slots=[%s]."),
				*GetNameSafe(GrabReachAnim), *GrabReachSlot.ToString(), bGrabHoldPose ? 1 : 0, StartAt,
				DynMontage ? TEXT("created") : TEXT("NULL"), bSlotActive ? 1 : 0,
				DynMontage ? TEXT(" OK") : TEXT(" FAILED(bad-anim-or-slot)"),
				SlotList.ToString());
		}
		else if (GrabMontage)
		{
			// Legacy fallback (only if GrabReachAnim unset): the montage path. Note this plays on the
			// montage's authored slot -- if that's DefaultSlot it will NOT show (the original bug).
			if (UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
					this, NAME_None, GrabMontage, /*Rate*/ 1.0f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true))
			{
				UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: montage start (fallback %s)."), *GetNameSafe(GrabMontage));
				MontageTask->ReadyForActivation();
			}
		}
	}
}

UAnimInstance* UAFLGameplayAbility_Grab::GetGrabAnimInstance(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// The reach must play on the AVATAR pawn's mesh (the hero's SKM_Manny), not the PlayerState.
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

void UAFLGameplayAbility_Grab::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// (Cycle 4d grab-hold fix) TOGGLE drop. While the grab is active (carrying), GAS routes a re-press of the
	// grab input here instead of re-activating the ability. So the SECOND press drops: end the active instance,
	// not cancelled (a clean, intentional drop). EndAbility runs the single cleanup path (stop carry pose,
	// detach + drop impulse, clear State.Carrying).
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: second press -> drop."));
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

void UAFLGameplayAbility_Grab::ExitGrab(const TCHAR* Reason, bool bCancelled)
{
	if (bExiting)
	{
		return; // first exit wins.
	}
	bExiting = true;

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: exit (reason=%s)."), Reason);

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility*/ true, bCancelled);
}

void UAFLGameplayAbility_Grab::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Cycle 4d -- release the sustained carry pose. The UpperBody slot holds the frozen reach-peak pose for the
	// whole carry; stop it (blended) on EVERY exit path (input-release, cancel, forced-drop) so the upper body
	// returns to the clip-driven pose. StopSlotAnimation targets the slot by name, so it cleans up the
	// dynamic montage without us caching a handle, and is a safe no-op if nothing is playing on the slot.
	if (UAnimInstance* AnimInstance = GetGrabAnimInstance(ActorInfo))
	{
		AnimInstance->StopSlotAnimation(GrabReachBlendTime, GrabReachSlot);
	}

	// (Cycle 4d grab-hold fix) the orientation snap is gone, so there is no controller-yaw flag to restore here.

	// Release the held actor on EVERY exit path (input-release, cancel, forced). If the interaction component
	// already released it (e.g. climb forced a drop), ReleaseActor() is a safe no-op.
	if (InteractionComponent.IsValid())
	{
		InteractionComponent->ReleaseActor();
	}
	InteractionComponent.Reset();

	// Remove State.Carrying so the tag clears immediately on every exit (mirror climb's explicit GE removal).
	if (CarryingEffectClass && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(CarryingEffectClass, ASC);
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: state restored."));
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
