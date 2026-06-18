// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Grab.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
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
#include "Interaction/AFLLootRetrievalRouter.h"   // C3: relationship-gated routing (owner-vs-enemy) before the mechanism fork
#include "Interaction/AFLObjectClassAnimSet.h"
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

	// 1a. ROUTING FORK -- before any hand-grab machinery. Decide channel-vs-instant:
	//   * DEFAULT (Phase B): the static GrabKind -- CollectLoot (caches) -> channel; CarryObject (map objects,
	//     stress-object, mini-game props) -> the proven hand-grab below, UNCHANGED.
	//   * (Loot-Carry Phase C) RELATIONSHIP ROUTER: if the target exposes IAFLLootRetrievalRouter (the loot's
	//     grant component), it resolves owner-vs-enemy (its SSOT) and OVERRIDES the mechanism so the fork happens
	//     AFTER the relationship is known: OWNER -> the instant hand-grab (dismember reattach, byte-for-byte
	//     untouched); ENEMY -> the collect-channel; INELIGIBLE -> cancel. No hard cast (queried by interface, per
	//     ue5-interaction-ik-expert). The owner never channels; the enemy never hand-attaches-then-despawns.
	bool bRouteToChannel = (Grabbable->GetGrabKind() == EAFLGrabKind::CollectLoot);
	if (const IAFLLootRetrievalRouter* Router = Cast<IAFLLootRetrievalRouter>(Target))
	{
		const EAFLRetrievalMode Mode = Router->ResolveRetrievalMode(Avatar);
		if (Mode == EAFLRetrievalMode::Ineligible)
		{
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: %s ineligible to retrieve %s -> cancel."),
				*GetNameSafe(Avatar), *GetNameSafe(Target));
			CancelAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateCancelAbility*/ true);
			return;
		}
		// OWNER -> false -> falls through to the hand-grab (instant reattach, untouched); ENEMY -> channel.
		bRouteToChannel = (Mode == EAFLRetrievalMode::EnemyCollect);
	}

	if (bRouteToChannel)
	{
		// CollectLoot / EnemyCollect: route to the collect-channel (UAFLAG_CollectChannel, AFLCombat) + the
		// carried pool (NOT hand-occupied). The channel grants from the target's UAFLLootGrantComponent on
		// complete (Decision D) -> +pool + despawn; then end this grab ability.
		const FGameplayTag CollectTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Loot.CollectChannel")), /*ErrorIfNotFound*/ false);
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		if (CollectTag.IsValid() && ASC)
		{
			FGameplayEventData Payload;
			Payload.EventTag = CollectTag;
			Payload.Instigator = Avatar;
			Payload.Target = Target;   // the loot -- the channel resolves its grant component from this
			ASC->HandleGameplayEvent(CollectTag, &Payload);
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: collect-loot %s -> collect-channel event sent."), *GetNameSafe(Target));
		}
		else
		{
			UE_LOG(LogAFLMovement, Warning,
				TEXT("AFL_GRAB: collect-loot but Event.Loot.CollectChannel unregistered / no ASC -- no collect (check AFLCombatTags.ini)."));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
		return;
	}

	// 1b. (Cycle 4d grab-hold fix) -- the orientation snap is GONE. It did SetActorRotation(TeleportPhysics) +
	// bUseControllerRotationYaw=false to "face the target," which teleport-snapped the whole body to a different
	// yaw on every grab (the quarter-spin / "backwards-inverted" symptom). The box attaches to hand_r regardless
	// of body facing, so the snap bought nothing. No orientation change on grab now.

	// 2. The hero's interaction component (owns the physical attach/hold; 4f -- the attach itself is now
	//    DEFERRED to DoAttachAndCarry, fired by the reach montage's contact notify).
	InteractionComponent = Avatar ? Avatar->FindComponentByClass<UAFLInteractionComponent>() : nullptr;
	if (!InteractionComponent.IsValid())
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: avatar has no UAFLInteractionComponent -> cancel."));
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}

	// 2b. (4f) Resolve the per-class anim set BEFORE anything plays -- the reach/carry montages live on it.
	UAFLObjectClassAnimSet* AnimSet = InteractionComponent->ResolveAndCacheAnimSet(Grabbable->GetGrabPolicy());
	UAnimMontage* ReachMontage = AnimSet ? AnimSet->GrabReachMontage.LoadSynchronous() : nullptr;

	// Stash what the deferred attach needs -- the contact notify / montage-end callbacks run frames from now.
	PendingGrabTarget = Target;
	PendingGrabPolicy = Grabbable->GetGrabPolicy();
	bAttachDone = false;
	ActiveCarryPoseMontage = nullptr;
	CarrierEffectHandle.Invalidate(); // fresh activation -- no carrier effect applied yet.

	if (ReachMontage)
	{
		// 3. REACH-THEN-ATTACH (4f). Order matters: hand-IK on first (the hand starts pulling toward the box
		//    at rest -- alpha fades in via the component's FInterpTo, never a hard switch), the event listener
		//    BEFORE the montage (an early notify must not race past an unbound listener), then the reach
		//    montage. The attach happens AT the contact notify (Event.Interaction.GrabAttach, fired by
		//    UAFLAnimNotify_GameplayEvent on the montage); montage end without the event is a logged fallback
		//    attach; an interrupt before contact aborts the grab cleanly (no attach, no State.Carrying).
		//    This supersedes 4c/4d decision W1 -- grab and hand-IK are now wired together; the console
		//    afl.HandIK.* path stays independent (it writes the same component fields).
		if (!GrabAttachEventTag.IsValid())
		{
			GrabAttachEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Interaction.GrabAttach")), /*ErrorIfNotFound*/ false);
		}

		InteractionComponent->SetHandIKTarget(Target->GetActorLocation());
		InteractionComponent->SetHandIKEnabled(true);

		if (UAbilityTask_WaitGameplayEvent* WaitEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
				this, GrabAttachEventTag, /*OptionalExternalTarget*/ nullptr, /*OnlyTriggerOnce*/ true, /*OnlyMatchExact*/ true))
		{
			WaitEvent->EventReceived.AddDynamic(this, &UAFLGameplayAbility_Grab::OnGrabAttachEvent);
			WaitEvent->ReadyForActivation();
		}

		UAbilityTask_PlayMontageAndWait* ReachTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, ReachMontage, /*Rate*/ 1.0f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true);
		if (!ReachTask)
		{
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: reach task creation failed -> instant attach."));
			DoAttachAndCarry();
			return;
		}
		ReachTask->OnCompleted.AddDynamic(this, &UAFLGameplayAbility_Grab::OnReachMontageCompleted);
		ReachTask->OnBlendOut.AddDynamic(this, &UAFLGameplayAbility_Grab::OnReachMontageCompleted);
		ReachTask->OnInterrupted.AddDynamic(this, &UAFLGameplayAbility_Grab::OnReachMontageInterrupted);
		ReachTask->OnCancelled.AddDynamic(this, &UAFLGameplayAbility_Grab::OnReachMontageInterrupted);
		ReachTask->ReadyForActivation();

		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: reach started (montage=%s, attach on %s)."),
			*GetNameSafe(ReachMontage), *GrabAttachEventTag.ToString());
		return; // continues in OnGrabAttachEvent / OnReachMontageCompleted / OnReachMontageInterrupted.
	}

	// 3'. No reach montage (null anim set or empty field): the legacy 4d instant-attach path.
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: no reach montage, instant attach."));
	DoAttachAndCarry();
}

void UAFLGameplayAbility_Grab::OnGrabAttachEvent(FGameplayEventData Payload)
{
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: contact notify received (%s) -> attach."), *Payload.EventTag.ToString());
	DoAttachAndCarry();
}

void UAFLGameplayAbility_Grab::OnReachMontageCompleted()
{
	if (bAttachDone || bExiting)
	{
		return; // normal end of an already-attached reach (or a dead ability) -- nothing to do.
	}
	UE_LOG(LogAFLMovement, Warning, TEXT("AFL_GRAB: GrabAttach notify never fired -- fallback attach on montage end."));
	DoAttachAndCarry();
}

void UAFLGameplayAbility_Grab::OnReachMontageInterrupted()
{
	if (bAttachDone || bExiting)
	{
		return; // the carry-pose montage interrupting the finished reach is EXPECTED; only pre-contact interrupts abort.
	}
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: reach interrupted before contact -> aborted grab (no attach, no state)."));
	if (InteractionComponent.IsValid())
	{
		InteractionComponent->SetHandIKEnabled(false);
	}
	ExitGrab(TEXT("reach interrupted"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_Grab::DoAttachAndCarry()
{
	if (bAttachDone || bExiting)
	{
		return; // contact notify and montage-end can both arrive -- first wins; never attach on a dead ability.
	}
	bAttachDone = true;

	// The reach is over either way. HARD-CUT the reach IK at the attach frame (named debt closed): the fade
	// kept pulling the arm toward the box's PRE-GRAB floor spot for ~0.2s while the carry pose pulled it to
	// the waist -- the attach-frame arm wrench, scaling with object size. A cut here is fully masked by the
	// box snapping into the hand this same frame. (EndAbility's mid-reach abort deliberately KEEPS the fade
	// -- an abort has no box-snap to mask a cut.)
	if (InteractionComponent.IsValid())
	{
		InteractionComponent->HandIKAlpha = 0.0f; // hard cut -- next tick releases the rig immediately
		InteractionComponent->SetHandIKEnabled(false);
	}

	// Re-validate -- a second of reach happened since ActivateAbility's check (target may be gone/taken).
	AActor* Target = PendingGrabTarget.Get();
	UAFLGrabbableComponent* Grabbable = Target ? Target->FindComponentByClass<UAFLGrabbableComponent>() : nullptr;
	if (!Target || !Grabbable || Grabbable->IsHeld() || !InteractionComponent.IsValid())
	{
		ExitGrab(TEXT("attach target lost during reach"), /*bCancelled*/ true);
		return;
	}

	// 4. The attach (unchanged inside GrabActor: all-prims inert -> snap to hand_r -> hold offset).
	if (!InteractionComponent->GrabActor(Target, PendingGrabPolicy))
	{
		ExitGrab(TEXT("GrabActor failed"), /*bCancelled*/ true);
		return;
	}

	// 5. Grant State.Carrying (the future penalty cycle + climb-block read this; mirror climb's GE shape).
	if (CarryingEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CarryingEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), SpecHandle);
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: state applied -> carrying."));
		}
	}

	// 5b. Per-object carrier effect (stress-object cycle): the policy names a GE class (e.g.
	//     UGE_AFL_CarrierVulnerability -> State.Carrying.Vulnerable, read by the damage ExecCalc).
	//     Tracked BY HANDLE because the class varies per object; removed by handle in EndAbility,
	//     which every release path reaches after the forced-path unification. Null = no effect.
	if (!PendingGrabPolicy.CarrierEffectClass.IsNull())
	{
		if (UClass* CarrierGEClass = PendingGrabPolicy.CarrierEffectClass.LoadSynchronous())
		{
			const FGameplayEffectSpecHandle CarrierSpec = MakeOutgoingGameplayEffectSpec(CarrierGEClass, GetAbilityLevel());
			if (CarrierSpec.IsValid())
			{
				CarrierEffectHandle = ApplyGameplayEffectSpecToOwner(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), CarrierSpec);
				UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: carrier effect applied (%s)."), *GetNameSafe(CarrierGEClass));
			}
		}
	}

	// 6. TOGGLE (4d): stays active (carrying) with the input released; a second press routes to
	//    InputPressed() -> EndAbility (the drop).
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: carrying -- press again to drop."));

	// 7. Hold stance.
	StartCarryPose();
}

void UAFLGameplayAbility_Grab::StartCarryPose()
{
	UAnimInstance* AnimInstance = GetGrabAnimInstance(GetCurrentActorInfo());
	if (!AnimInstance)
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: no anim instance -- carry continues with no pose."));
		return;
	}

	// 7a. (4f) Per-class hold: the anim set's CarryPose montage, started at its settle time. CarryPose
	// montages are authored with bEnableAutoBlendOut=false, so on reaching the end the pose HOLDS until
	// EndAbility's Montage_Stop -- a real settle-then-hold, no rate-0 freeze plumbing on this path.
	UAFLObjectClassAnimSet* AnimSet = InteractionComponent.IsValid() ? InteractionComponent->GetActiveAnimSet() : nullptr;
	UAnimMontage* CarryPose = AnimSet ? AnimSet->CarryPose.LoadSynchronous() : nullptr;
	if (CarryPose)
	{
		// 2-client cycle 1: route through the ASC (not a direct AnimInstance->Montage_Play) so SIM PROXIES
		// receive the pose via GAS montage replication. Static read of FGameplayAbilityRepAnimMontage says
		// this path replicates cleanly: it carries the montage ASSET ref + PlayRate + Position, this play is
		// rate-1 settle-then-hold (never rate-0), and at the held end the server position pins at montage
		// length while bEnableAutoBlendOut=false is an ASSET property -- so proxies reach the end and hold
		// exactly like the authority. The REACH already replicates (PlayMontageAndWait is ASC-routed). The
		// rate-0 LEGACY fallback below CANNOT replicate by construction (PlaySlotAnimationAsDynamicMontage
		// builds a TRANSIENT montage object -- nothing for RepAnimMontage to reference) and stays local-only:
		// it only runs when an OCAS ships no CarryPose, which shipping content always sets.
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		const float PlayResult = ASC
			? ASC->PlayMontage(this, GetCurrentActivationInfo(), CarryPose, /*InPlayRate*/ 1.0f,
				NAME_None, /*StartTimeSeconds*/ CarryPoseStartTime)
			: AnimInstance->Montage_Play(CarryPose, /*InPlayRate*/ 1.0f,
				EMontagePlayReturnType::MontageLength, /*InTimeToStartMontageAt*/ CarryPoseStartTime);
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: carry-pose montage (asset=%s start=%.2fs holdAtEnd=1 route=%s) %s."),
			*GetNameSafe(CarryPose), CarryPoseStartTime, ASC ? TEXT("ASC") : TEXT("local"),
			(PlayResult > 0.0f) ? TEXT("OK") : TEXT("FAILED -> legacy fallback"));
		if (PlayResult > 0.0f)
		{
			ActiveCarryPoseMontage = CarryPose;
			return;
		}
	}

	// 7b. Legacy/proven fallback (4d): freeze the raw reach clip on the UPPER-BODY slot (Lyra-canonical
	// layered-upper-body: arms hold, legs keep locomotion via the AnimGraph's Layered Blend Per Bone).
	// DefaultSlot is NOT routed in Lyra's AnimGraph -- a montage/anim on it plays into the void.
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

		// Cycle 4d -- SUSTAINED carry pose: play at rate 0 starting at GrabHoldTime so the montage holds that
		// single frame until EndAbility's StopSlotAnimation releases it. Rate 0 + the default
		// BlendOutTriggerTime=-1 means no auto blend-out -- the pose holds indefinitely.
		// (bGrabHoldPose=false restores the 4c transient one-shot reach: rate 1.0, plays through once.)
		const float HoldRate = bGrabHoldPose ? 0.0f : 1.0f;
		const float StartAt = bGrabHoldPose ? GrabHoldTime : 0.0f;

		// PlaySlotAnimationAsDynamicMontage returns the created montage -- non-null is the authoritative
		// success signal. IsSlotActive is informational corroboration (may lag a frame on the very first
		// tick, so it is NOT part of the pass/fail verdict).
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
		// Last-resort fallback (only if GrabReachAnim unset): the legacy montage path. Note this plays on
		// the montage's authored slot -- if that's DefaultSlot it will NOT show (the original bug).
		if (UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this, NAME_None, GrabMontage, /*Rate*/ 1.0f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true))
		{
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: montage start (fallback %s)."), *GetNameSafe(GrabMontage));
			MontageTask->ReadyForActivation();
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
	// 4f: straggler reach callbacks (contact notify / montage delegates landing after the end) must no-op.
	bExiting = true;

	// 4f: fade the reach IK out on EVERY exit (covers mid-reach aborts where DoAttachAndCarry never ran;
	// idempotent when it did -- alpha is already cut to 0 there). This path KEEPS the smooth fade
	// deliberately: a mid-reach abort has no box-snap to mask a hard cut (the cut lives at the attach frame).
	if (InteractionComponent.IsValid())
	{
		InteractionComponent->SetHandIKEnabled(false);
	}

	// Stop whichever hold is active, on EVERY exit path (toggle-drop, cancel, forced-drop). The per-class
	// carry-pose montage (autoBlendOut=false) holds its pose until told to stop; the legacy frozen slot pose
	// needs StopSlotAnimation. Both calls are safe no-ops when that path isn't playing.
	if (UAnimInstance* AnimInstance = GetGrabAnimInstance(ActorInfo))
	{
		if (ActiveCarryPoseMontage)
		{
			// ASC-routed stop FIRST (2-client cycle 1, matches the ASC-routed play): on authority this
			// replicates the stop so sim proxies blend out with us. The direct AnimInstance stop below stays
			// as belt-and-braces -- it covers the local-fallback play path and any frame where another
			// ASC montage already replaced ours (the guard keeps us from stopping someone else's montage).
			if (UAbilitySystemComponent* EndASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
			{
				if (EndASC->GetCurrentMontage() == ActiveCarryPoseMontage)
				{
					EndASC->CurrentMontageStop(GrabReachBlendTime);
				}
			}
			AnimInstance->Montage_Stop(GrabReachBlendTime, ActiveCarryPoseMontage);
		}
		AnimInstance->StopSlotAnimation(GrabReachBlendTime, GrabReachSlot);
	}
	ActiveCarryPoseMontage = nullptr;

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

	// Per-object carrier effect: removal BY HANDLE (the class varies per object -- see DoAttachAndCarry 5b).
	// Handle removal is a safe no-op if the effect was never applied or already expired.
	if (CarrierEffectHandle.IsValid() && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffect(CarrierEffectHandle);
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_GRAB: carrier effect removed."));
		}
	}
	CarrierEffectHandle.Invalidate();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
