// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Sprint.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Sprint)

// Match-state activation blockers (mirror the dash/climb GA pattern; native-declared to remove cross-plugin
// ini load-order fragility). Cross-move blocking (can't sprint while climbing/sliding, etc.) is deferred to
// the Phase-4 ability interaction matrix per the plan.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Sprint_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Sprint_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Sprint_State_Extracting, "State.Extracting");

UAFLGameplayAbility_Sprint::UAFLGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// WhileInputActive owns the held lifecycle: Lyra's ProcessAbilityInput activates ONCE while the input is
	// held (not per frame). The WaitInputRelease task ends it on release. (Same reasoning as GA_AFL_Climb.)
	ActivationPolicy = ELyraAbilityActivationPolicy::WhileInputActive;

	ActivationBlockedTags.AddTag(TAG_Sprint_State_Match_Warmup);
	ActivationBlockedTags.AddTag(TAG_Sprint_State_Match_Ended);
	ActivationBlockedTags.AddTag(TAG_Sprint_State_Extracting);
}

void UAFLGameplayAbility_Sprint::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// InstancedPerActor -> the same instance is reused for every sprint; clear per-activation state.
	bEnding = false;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}

	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Optional grounded gate: don't start a sprint mid-air (it would only take effect on landing anyway).
	if (bRequireGrounded)
	{
		const UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
		if (CMC && CMC->IsFalling())
		{
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_SPRINT: activate ignored — character is falling."));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	UE_LOG(LogAFLMovement, Log, TEXT("AFL_SPRINT: activate by %s."), *GetNameSafe(Character));

	// Apply the sprint-active GE -> grants State.Movement.Sprinting -> UAFLSprintMovementComponent swaps
	// MaxWalkSpeed/MaxAcceleration. EndAbility removes it (tag clears -> CMC restored).
	if (SprintActiveEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(SprintActiveEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}
	else
	{
		UE_LOG(LogAFLMovement, Warning, TEXT("AFL_SPRINT: no SprintActiveEffectClass set -> no speed swap (BP child unconfigured)."));
	}

	// Input-release listener -> end the sprint on release. bTestAlreadyReleased=FALSE so it waits for a genuine
	// replicated InputReleased (the =true first-frame-fire bug documented in GA_AFL_Climb).
	if (UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased*/ false))
	{
		ReleaseTask->OnRelease.AddDynamic(this, &UAFLGameplayAbility_Sprint::OnInputReleased);
		ReleaseTask->ReadyForActivation();
	}
}

void UAFLGameplayAbility_Sprint::OnInputReleased(float /*TimeHeld*/)
{
	if (bEnding)
	{
		return; // first exit wins
	}
	bEnding = true;
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_SPRINT: input released -> end."));
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

void UAFLGameplayAbility_Sprint::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Remove the sprint-active GE so State.Movement.Sprinting clears -> the component restores the CMC on
	// every exit path (release, cancel, death, match-end). Mirror GA_AFL_Climb's explicit removal.
	if (SprintActiveEffectClass && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(SprintActiveEffectClass, ASC);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
