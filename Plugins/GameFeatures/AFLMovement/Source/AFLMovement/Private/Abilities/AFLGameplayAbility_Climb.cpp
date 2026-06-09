// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Climb.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Movement/AFLClimbMovementComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Climb)

// Native ability tag (mirrors the dash GA's native-tag pattern; removes cross-plugin ini load-order
// fragility). The ability's own AbilityTags should include Ability.Movement.Climb (set on the BP child
// or here if desired); the activation-block tags mirror dash.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Movement_Climb, "Ability.Movement.Climb");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Climb_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Climb_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Climb_State_Extracting, "State.Extracting");

UAFLGameplayAbility_Climb::UAFLGameplayAbility_Climb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// WhileInputActive owns the held lifecycle (the working-sibling pattern from UAFLAG_BeamChannel_v2):
	// Lyra's ProcessAbilityInput activates the ability ONCE while the input is held -- it does NOT re-fire
	// every frame. WITHOUT this (default policy + a no-trigger IA), the input re-triggered ~every frame and
	// climb re-activated repeatedly (the "robot repeatedly lunges into the wall" symptom). The IA keeps its
	// InputTriggerPressed; this policy is what makes the hold a single sustained activation. The
	// WaitInputRelease task still ends it on release (nothing else calls CancelInputActivatedAbilities --
	// its sole caller is the CheatManager; same reason Beam_v2 keeps the task).
	ActivationPolicy = ELyraAbilityActivationPolicy::WhileInputActive;

	ActivationBlockedTags.AddTag(TAG_Climb_State_Match_Warmup);
	ActivationBlockedTags.AddTag(TAG_Climb_State_Match_Ended);
	ActivationBlockedTags.AddTag(TAG_Climb_State_Extracting);
}

void UAFLGameplayAbility_Climb::ActivateAbility(
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

	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: Activate by %s."), *GetNameSafe(Character));

	// 1. Surface validation: forward trace; no wall -> not a valid climb, cancel.
	if (const UWorld* World = Character->GetWorld())
	{
		const FVector Start = Character->GetActorLocation();
		const FVector End = Start + Character->GetActorForwardVector() * SurfaceTraceDistance;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLClimbActivateTrace), false, Character);
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		if (!bHit)
		{
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: no surface within %.0fcm -> cancel."), SurfaceTraceDistance);
			CancelAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateCancelAbility*/ true);
			return;
		}
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: surface valid (normal=%s)."), *Hit.ImpactNormal.ToCompactString());
	}

	// 2. Apply the climb-active GE -> grants State.Movement.Climbing -> UAFLClimbMovementComponent flips
	//    the stock CMC to gravity-0 / MOVE_Flying. EndAbility removes it (tag clears -> CMC restored).
	if (ClimbActiveEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(ClimbActiveEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// 3. Bind the component's surface-loss delegate (exit when the wall is lost mid-climb).
	ClimbComponent = Character->FindComponentByClass<UAFLClimbMovementComponent>();
	if (ClimbComponent.IsValid())
	{
		SurfaceLostHandle = ClimbComponent->OnClimbSurfaceLost.AddUObject(this, &UAFLGameplayAbility_Climb::OnSurfaceLost);
	}

	// 4. Input-release listener -> cancel on release.
	// bTestAlreadyReleased=FALSE: do NOT fire on the initial-state check. With =true the task's Activate()
	// immediately fired OnReleaseCallback when GAS read the input as not-currently-held on that frame, so
	// EVERY climb exited reason=input-release on the first frame (never reaching montage-complete or
	// surface-loss). =false waits for a genuine InputReleased replicated event -> the other exit paths can win.
	if (UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased*/ false))
	{
		ReleaseTask->OnRelease.AddDynamic(this, &UAFLGameplayAbility_Climb::OnInputReleased);
		ReleaseTask->ReadyForActivation();
	}

	// 5. Play the root-motion climb montage; its completion places the character at the ledge top.
	if (ClimbMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, ClimbMontage, /*Rate*/ 1.0f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UAFLGameplayAbility_Climb::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UAFLGameplayAbility_Climb::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UAFLGameplayAbility_Climb::OnMontageInterruptedOrCancelled);
			MontageTask->OnCancelled.AddDynamic(this, &UAFLGameplayAbility_Climb::OnMontageInterruptedOrCancelled);
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: montage start (%s)."), *GetNameSafe(ClimbMontage));
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		// No montage authored yet -> the climb-state GE still applied; without root motion there is no
		// up-translation, so we rely on input-release / surface-loss to exit. Log so this is visible.
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: no ClimbMontage set -> CMC state applied, no root motion (placeholder pending)."));
	}
}

void UAFLGameplayAbility_Climb::OnMontageCompleted()
{
	ExitClimb(TEXT("complete"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_Climb::OnMontageInterruptedOrCancelled()
{
	ExitClimb(TEXT("interrupted"), /*bCancelled*/ true);
}

void UAFLGameplayAbility_Climb::OnInputReleased(float TimeHeld)
{
	ExitClimb(TEXT("input-release"), /*bCancelled*/ true);
}

void UAFLGameplayAbility_Climb::OnSurfaceLost()
{
	ExitClimb(TEXT("surface-loss"), /*bCancelled*/ true);
}

void UAFLGameplayAbility_Climb::ExitClimb(const TCHAR* Reason, bool bCancelled)
{
	if (bExiting)
	{
		return; // first exit wins; the others are stale signals from tasks ending.
	}
	bExiting = true;

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_CLIMB: exit (reason=%s)."), Reason);

	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility*/ true, bCancelled);
}

void UAFLGameplayAbility_Climb::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Unbind the surface-loss delegate before the component/ability tear down.
	if (ClimbComponent.IsValid() && SurfaceLostHandle.IsValid())
	{
		ClimbComponent->OnClimbSurfaceLost.Remove(SurfaceLostHandle);
	}
	SurfaceLostHandle.Reset();
	ClimbComponent.Reset();

	// Remove the climb-active GE so State.Movement.Climbing clears -> the component restores the CMC.
	// (Auto-removed too when the ability ends if the GE was applied with this as the source, but explicit
	// removal here guarantees the tag clears immediately on every exit path.)
	if (ClimbActiveEffectClass && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(ClimbActiveEffectClass, ASC);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
