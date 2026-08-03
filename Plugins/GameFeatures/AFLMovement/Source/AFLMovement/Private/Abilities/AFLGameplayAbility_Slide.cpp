// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Slide.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "MotionWarpingComponent.h"
#include "Movement/AFLMovementPathScope.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Slide)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Slide_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Slide_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Slide_State_Extracting, "State.Extracting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Slide_State_Movement_Climbing, "State.Movement.Climbing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Movement_Slide_Requested, "Event.Movement.Slide.Requested");

UAFLGameplayAbility_Slide::UAFLGameplayAbility_Slide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Press-to-slide (one-shot). NOTE: we intentionally do NOT block the fire/ADS abilities — slide-and-shoot.
	ActivationBlockedTags.AddTag(TAG_Slide_State_Match_Warmup);
	ActivationBlockedTags.AddTag(TAG_Slide_State_Match_Ended);
	ActivationBlockedTags.AddTag(TAG_Slide_State_Extracting);
	ActivationBlockedTags.AddTag(TAG_Slide_State_Movement_Climbing);

	// AI-3 bot entry point, additive to InputTag.Movement.Slide. Conforms to WallRun.cpp:35-37.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = TAG_Event_Movement_Slide_Requested;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UAFLGameplayAbility_Slide::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// InstancedPerActor -> clear per-activation state.
	bExiting = false;

	// PATH SCOPE. The slide montage owns the body via root motion + MotionWarping; path following would
	// fight it every frame. Paired with Resume in EndAbility, which GAS funnels every exit through.
	// No-op for humans (resolved through AAIController). See AFLMovementPathScope.h.
	AFLMovementPath::Suspend(ActorInfo);
	bWarpApplied = false;
	MontageTask = nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* CMC = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !CMC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Require grounded + enough horizontal speed (no slide from a standstill or mid-air).
	if (CMC->IsFalling() || CMC->Velocity.Size2D() < MinSlideSpeed)
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_SLIDE: activate ignored (falling=%d, speed2D=%.0f < %.0f)."),
			CMC->IsFalling() ? 1 : 0, CMC->Velocity.Size2D(), MinSlideSpeed);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_SLIDE: activate by %s (speed2D=%.0f)."), *GetNameSafe(Character), CMC->Velocity.Size2D());

	// 1. Motion Warping -> skew the fixed-distance slide montage to a geometry-aware stop point (mirror Climb).
	if (UMotionWarpingComponent* MotionWarping = Character->FindComponentByClass<UMotionWarpingComponent>())
	{
		FVector WarpLoc;
		FRotator WarpRot;
		if (ComputeSlideStopTarget(WarpLoc, WarpRot))
		{
			MotionWarping->bSearchForWindowsInAnimsWithinMontages = true; // warp windows live on sub-anims (idempotent)
			MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(SlideStopWarpTargetName, WarpLoc, WarpRot);
			bWarpApplied = true;
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_SLIDE: warp target added (%s, loc=%s)."),
				*SlideStopWarpTargetName.ToString(), *WarpLoc.ToCompactString());
		}
	}
	else
	{
		UE_LOG(LogAFLMovement, Warning, TEXT("AFL_SLIDE: no MotionWarpingComponent on hero -> slide uses raw montage distance."));
	}

	// 2. Apply the slide-active GE -> grants State.Movement.Sliding -> UAFLSlideMovementComponent swaps
	//    friction/braking + ducks the capsule. EndAbility removes it.
	if (SlideActiveEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(SlideActiveEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}
	else
	{
		UE_LOG(LogAFLMovement, Warning, TEXT("AFL_SLIDE: no SlideActiveEffectClass set (BP child unconfigured)."));
	}

	// 3. Input-release early-out -> stand up before the montage finishes. bTestAlreadyReleased=FALSE (the
	//    Climb first-frame-fire fix).
	if (UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased*/ false))
	{
		ReleaseTask->OnRelease.AddDynamic(this, &UAFLGameplayAbility_Slide::OnInputReleased);
		ReleaseTask->ReadyForActivation();
	}

	// 4. Play the root-motion slide montage. If none authored, the GE (friction/duck) still applied; without
	//    root motion the slide is just the low-friction carry until the input releases.
	if (SlideMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, SlideMontage, /*Rate*/ 1.0f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UAFLGameplayAbility_Slide::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UAFLGameplayAbility_Slide::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UAFLGameplayAbility_Slide::OnMontageInterruptedOrCancelled);
			MontageTask->OnCancelled.AddDynamic(this, &UAFLGameplayAbility_Slide::OnMontageInterruptedOrCancelled);
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_SLIDE: no SlideMontage set -> friction/duck applied, no root motion (placeholder)."));
	}
}

bool UAFLGameplayAbility_Slide::ComputeSlideStopTarget(FVector& OutLocation, FRotator& OutRotation) const
{
	const ACharacter* Character = GetCurrentActorInfo() ? Cast<ACharacter>(GetCurrentActorInfo()->AvatarActor.Get()) : nullptr;
	const UWorld* World = Character ? Character->GetWorld() : nullptr;
	const UCharacterMovementComponent* CMC = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !World || !CMC)
	{
		return false;
	}

	// Slide direction = current horizontal velocity (fallback to actor forward).
	FVector Dir = CMC->Velocity;
	Dir.Z = 0.0f;
	if (!Dir.Normalize())
	{
		Dir = Character->GetActorForwardVector();
		Dir.Z = 0.0f;
		Dir.Normalize();
	}

	const FVector Origin = Character->GetActorLocation();
	float Distance = SlideStopDistance;

	// Forward trace -> shorten the slide against a wall so the warp never drives the character into geometry.
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLSlideForwardTrace), /*bTraceComplex*/ false, Character);
		FHitResult Hit;
		const FVector End = Origin + Dir * SlideStopDistance;
		if (World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params))
		{
			Distance = FMath::Max(0.0f, Hit.Distance - 40.0f); // capsule pad
		}
	}

	FVector Target = Origin + Dir * Distance;

	// Down trace -> snap the stop point to the ground so the warp keeps the slide grounded on slopes/steps.
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLSlideGroundTrace), /*bTraceComplex*/ false, Character);
		FHitResult Hit;
		const FVector Start = Target + FVector::UpVector * 60.0f;
		const FVector End = Target - FVector::UpVector * 200.0f;
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			// Snap the stop point's Z so the character's root (capsule center) rests on the ground contact.
			Target.Z = Hit.ImpactPoint.Z + Character->GetSimpleCollisionHalfHeight();
		}
	}

	OutLocation = Target;
	OutRotation = Dir.Rotation(); // keep facing the slide direction (no snap-turn)
	return true;
}

void UAFLGameplayAbility_Slide::OnMontageCompleted()
{
	ExitSlide(TEXT("complete"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_Slide::OnMontageInterruptedOrCancelled()
{
	ExitSlide(TEXT("interrupted"), /*bCancelled*/ true);
}

void UAFLGameplayAbility_Slide::OnInputReleased(float /*TimeHeld*/)
{
	// Released the slide key before the montage finished -> stand up early.
	ExitSlide(TEXT("input-release"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_Slide::ExitSlide(const TCHAR* Reason, bool bCancelled)
{
	if (bExiting)
	{
		return; // first exit wins
	}
	bExiting = true;
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_SLIDE: exit (reason=%s)."), Reason);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility*/ true, bCancelled);
}

void UAFLGameplayAbility_Slide::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// THE OTHER HALF OF THE PATH SCOPE. First statement in EndAbility on purpose: every exit path -- normal
	// completion, cancel, death, match end, a failed activation after commit -- funnels through here, and a
	// path paused and never resumed is a bot frozen for the rest of the round.
	AFLMovementPath::Resume(ActorInfo);

	// Clear the warp target this ability added (mirror Climb).
	if (bWarpApplied)
	{
		if (const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
		{
			if (UMotionWarpingComponent* MotionWarping = Character->FindComponentByClass<UMotionWarpingComponent>())
			{
				MotionWarping->RemoveAllWarpTargets();
			}
		}
		bWarpApplied = false;
	}

	// Remove the slide-active GE so State.Movement.Sliding clears -> the component restores friction/braking
	// and un-ducks the capsule on every exit path.
	if (SlideActiveEffectClass && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(SlideActiveEffectClass, ASC);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
