// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_WallJump.h"

#include "AFLMovement.h"
#include "GameFramework/Character.h"
#include "Movement/AFLWallRunMovementComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_WallJump)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WallJump_State_Movement_WallRunning, "State.Movement.WallRunning");

UAFLGameplayAbility_WallJump::UAFLGameplayAbility_WallJump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Only fires during a wall-run; bound to the JUMP input via the ability set (alongside the stock jump,
	// which no-ops in MOVE_Flying). No required tag => normal jump; tag present => this wall-jump.
	ActivationRequiredTags.AddTag(TAG_WallJump_State_Movement_WallRunning);
}

void UAFLGameplayAbility_WallJump::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UAFLWallRunMovementComponent* WallRun = Character ? Character->FindComponentByClass<UAFLWallRunMovementComponent>() : nullptr;
	FVector Normal = WallRun ? WallRun->GetCurrentWallNormal() : FVector::ZeroVector;
	if (!Character || !WallRun || Normal.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Normal.Z = 0.0f;
	Normal.Normalize();
	const FVector Launch = Normal * AwayForce + FVector::UpVector * UpForce;

	// Launch off the wall. This pulls the pawn off the surface -> the wall-run ability loses the wall next
	// tick and exits itself (which restores gravity/mode). bXYOverride/bZOverride so the kick is deterministic.
	Character->LaunchCharacter(Launch, /*bXYOverride*/ true, /*bZOverride*/ true);

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_WALLJUMP: %s launched off wall (normal=%s)."),
		*GetNameSafe(Character), *Normal.ToCompactString());

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}
