// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Dash.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Dash)

// Native tags — defensive even though State.Match.* and State.Extracting
// are declared in AFLCore's ini (which loads before AFLMovement). The
// cross-plugin load-order assumption is fragile; native declaration here
// removes it. Mirrors the pattern that proved necessary in AFLCombat's
// AFLAG_Laser_Pulse.cpp (same-plugin ini race → editor crash).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Extracting, "State.Extracting");
// AFL-0308: one-shot dash-activation cue (whoosh SFX). Tag declared in AFLCoreTags.ini
// (Cue.Movement.Dash.Activated); GCN_AFL_Dash_Activated receives it and plays the random
// MS_AFL_DashWoosh. Fired at dash start, mirroring UAFLAG_Laser_Pulse's fire cue.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_Movement_Dash_Activated, "Cue.Movement.Dash.Activated");

UAFLGameplayAbility_Dash::UAFLGameplayAbility_Dash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	DashImpulse = FScalableFloat(1500.0f);

	// Block dash during match-flow gates and extraction channel. Tags owned
	// by future systems (match flow S9 AFL-0902, extraction S8 AFL-0803) —
	// declared in AFLCoreTags.ini and natively above so the contract is
	// enforced from Sprint 3 without depending on ini scan order.
	ActivationBlockedTags.AddTag(TAG_State_Match_Warmup);
	ActivationBlockedTags.AddTag(TAG_State_Match_Ended);
	ActivationBlockedTags.AddTag(TAG_State_Extracting);
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

	// AFL-0308: fire the one-shot dash whoosh cue at dash start (location = the dasher,
	// direction = the dash vector). GCN_AFL_Dash_Activated plays MS_AFL_DashWoosh. Same
	// K2_ExecuteGameplayCueWithParams shape UAFLAG_Laser_Pulse uses for its fire cue;
	// on a LocalPredicted ability the cue predicts locally + replicates to others for free.
	{
		FGameplayCueParameters DashCueParams;
		DashCueParams.Location     = Character->GetActorLocation();
		DashCueParams.Normal       = DashDir;
		DashCueParams.Instigator   = Character;
		DashCueParams.SourceObject = Character;
		K2_ExecuteGameplayCueWithParams(TAG_Cue_Movement_Dash_Activated, DashCueParams);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}
