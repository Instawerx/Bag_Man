// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_WallRun.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "Movement/AFLWallRunMovementComponent.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_WallRun)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallRun_Event_Detected, "Event.Movement.WallRun.Detected");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallRun_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallRun_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallRun_State_Extracting, "State.Extracting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallRun_State_Movement_WallRunning, "State.Movement.WallRunning");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallRun_State_Movement_Climbing, "State.Movement.Climbing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallRun_State_Carrying, "State.Carrying");

UAFLGameplayAbility_WallRun::UAFLGameplayAbility_WallRun(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// CONTEXTUAL activation: triggered by the component's wall-detect gameplay event (no input binding).
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = TAG_WallRun_Event_Detected;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);

	ActivationBlockedTags.AddTag(TAG_WallRun_State_Match_Warmup);
	ActivationBlockedTags.AddTag(TAG_WallRun_State_Match_Ended);
	ActivationBlockedTags.AddTag(TAG_WallRun_State_Extracting);
	ActivationBlockedTags.AddTag(TAG_WallRun_State_Movement_WallRunning); // no re-trigger while already running
	ActivationBlockedTags.AddTag(TAG_WallRun_State_Movement_Climbing);
	ActivationBlockedTags.AddTag(TAG_WallRun_State_Carrying);
}

void UAFLGameplayAbility_WallRun::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bExiting = false;
	MontageTask = nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WallRunComponent = Character->FindComponentByClass<UAFLWallRunMovementComponent>();
	if (!WallRunComponent.IsValid())
	{
		UE_LOG(LogAFLMovement, Warning, TEXT("AFL_WALLRUN: no UAFLWallRunMovementComponent on hero -> cancel."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Confirm a side wall is actually present (the detect event can arrive a frame stale) and orient to it.
	FRotator Facing;
	if (!FindWall(Facing))
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_WALLRUN: wall gone on activate -> cancel."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Character->SetActorRotation(Facing); // face along the wall so the run montage reads correctly (v1: set once)

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_WALLRUN: activate by %s."), *GetNameSafe(Character));

	// Apply the wall-run-active GE -> State.Movement.WallRunning -> the component flips gravity-0/flying and
	// drives velocity along the wall tangent.
	if (WallRunActiveEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(WallRunActiveEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// Exit when the component reports the wall is lost.
	SurfaceLostHandle = WallRunComponent->OnWallSurfaceLost.AddUObject(this, &UAFLGameplayAbility_WallRun::OnSurfaceLost);

	// Hard duration cap.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimeoutHandle, this, &UAFLGameplayAbility_WallRun::OnTimeout, MaxWallRunDuration, false);
	}

	// Looping wall-run montage (the component owns the velocity; this is the pose/tilt).
	if (WallRunMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, WallRunMontage, /*Rate*/ 1.0f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UAFLGameplayAbility_WallRun::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UAFLGameplayAbility_WallRun::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UAFLGameplayAbility_WallRun::OnMontageInterruptedOrCancelled);
			MontageTask->OnCancelled.AddDynamic(this, &UAFLGameplayAbility_WallRun::OnMontageInterruptedOrCancelled);
			MontageTask->ReadyForActivation();
		}
	}
}

bool UAFLGameplayAbility_WallRun::FindWall(FRotator& OutFacing) const
{
	const ACharacter* Character = GetCurrentActorInfo() ? Cast<ACharacter>(GetCurrentActorInfo()->AvatarActor.Get()) : nullptr;
	const UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World)
	{
		return false;
	}

	const FVector Start = Character->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLWallRunConfirmTrace), /*bTraceComplex*/ false, Character);

	FHitResult Hit;
	bool bHit = World->LineTraceSingleByChannel(Hit, Start, Start - Character->GetActorRightVector() * DetectSideDistance, ECC_Visibility, Params);
	if (!bHit)
	{
		bHit = World->LineTraceSingleByChannel(Hit, Start, Start + Character->GetActorRightVector() * DetectSideDistance, ECC_Visibility, Params);
	}
	if (!bHit)
	{
		return false;
	}

	// Tangent along the wall, oriented along the pawn's current forward.
	FVector Tangent = FVector::CrossProduct(Hit.ImpactNormal, FVector::UpVector).GetSafeNormal();
	FVector Fwd = Character->GetActorForwardVector();
	Fwd.Z = 0.0f;
	if (FVector::DotProduct(Tangent, Fwd) < 0.0f)
	{
		Tangent = -Tangent;
	}
	OutFacing = Tangent.Rotation();
	return true;
}

void UAFLGameplayAbility_WallRun::OnMontageCompleted()
{
	// The loop shouldn't naturally complete; if it blends out, treat as an exit.
	ExitWallRun(TEXT("montage-complete"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_WallRun::OnMontageInterruptedOrCancelled()
{
	ExitWallRun(TEXT("montage-interrupted"), /*bCancelled*/ true);
}

void UAFLGameplayAbility_WallRun::OnSurfaceLost()
{
	ExitWallRun(TEXT("surface-lost"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_WallRun::OnTimeout()
{
	ExitWallRun(TEXT("timeout"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_WallRun::ExitWallRun(const TCHAR* Reason, bool bCancelled)
{
	if (bExiting)
	{
		return;
	}
	bExiting = true;
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_WALLRUN: exit (reason=%s)."), Reason);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility*/ true, bCancelled);
}

void UAFLGameplayAbility_WallRun::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (WallRunComponent.IsValid() && SurfaceLostHandle.IsValid())
	{
		WallRunComponent->OnWallSurfaceLost.Remove(SurfaceLostHandle);
	}
	SurfaceLostHandle.Reset();
	WallRunComponent.Reset();

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutHandle);
	}

	// Remove the wall-run-active GE so State.Movement.WallRunning clears -> the component restores gravity/mode.
	if (WallRunActiveEffectClass && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(WallRunActiveEffectClass, ASC);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
