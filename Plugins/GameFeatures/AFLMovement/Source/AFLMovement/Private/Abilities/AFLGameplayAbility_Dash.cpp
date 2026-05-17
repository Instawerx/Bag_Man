// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Dash.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Dash)

UAFLGameplayAbility_Dash::UAFLGameplayAbility_Dash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	DashImpulse = FScalableFloat(1500.0f);

	// Block dash during match-flow gates and extraction channel. Tags owned
	// by future systems (match flow S9 AFL-0902, extraction S8 AFL-0803) —
	// declared in AFLCoreTags.ini so the contract is enforced from Sprint 3.
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Match.Warmup")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Match.Ended")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Extracting")));
}

void UAFLGameplayAbility_Dash::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
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

	// Resolve dash direction: pawn movement input first, fall back to actor forward.
	// Flatten to XY so a downward-aimed input does not bury the dash into the floor.
	FVector DashDir = Character->GetLastMovementInputVector();
	DashDir.Z = 0.0f;
	if (DashDir.IsNearlyZero())
	{
		DashDir = Character->GetActorForwardVector();
		DashDir.Z = 0.0f;
	}
	DashDir = DashDir.GetSafeNormal();

	if (DashDir.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Apply the active GE (grants State.Movement.Dashing for 0.12s). CMC listens
	// to this tag and swaps friction/air-control. Auto-removal on duration end
	// restores them via GAS rollback — no manual save/restore timers in source.
	if (DashActiveEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DashActiveEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// LaunchCharacter replicates natively (UCharacterMovementComponent::Launch).
	// XYOverride=true so the impulse fully replaces planar velocity (predictable
	// dash distance); ZOverride=false leaves jump/gravity arc intact.
	const FVector Impulse = DashDir * DashImpulse.GetValue();
	Character->LaunchCharacter(Impulse, /*bXYOverride*/ true, /*bZOverride*/ false);

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}
