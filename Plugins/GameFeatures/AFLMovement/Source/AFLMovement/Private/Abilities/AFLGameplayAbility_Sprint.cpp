// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Sprint.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Sprint)

// Match-state activation blockers (mirror the dash/climb GA pattern; native-declared to remove cross-plugin
// ini load-order fragility). Cross-move blocking (can't sprint while climbing/sliding, etc.) is deferred to
// the Phase-4 ability interaction matrix per the plan.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Sprint_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Sprint_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Sprint_State_Extracting, "State.Extracting");

// AI-3. A bot has no input component, so InputTag.Movement.Sprint is unreachable from a controller. This is
// the same seam BTS_Shoot uses to fire, and it conforms to the one working model in this module --
// AFLGameplayAbility_WallRun.cpp:35-37, Event.Movement.<Ability>.<Verb>.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Movement_Sprint_Requested, "Event.Movement.Sprint.Requested");

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

	// The bot entry point. Additive to the input path -- WhileInputActive above is untouched, so the human
	// hold-to-sprint lifecycle is bit-for-bit what it was.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = TAG_Event_Movement_Sprint_Requested;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
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

	// ============================ HOW THIS ENDS ============================
	// Two activation paths, two terminators, and they must not be confused. TriggerEventData is non-null only
	// when GAS activated us from a GameplayEvent -- the same fork AFLAG_Hitscan_Base uses to tell a bot shot
	// from a player shot.
	if (TriggerEventData == nullptr)
	{
		// HUMAN. Unchanged. bTestAlreadyReleased=FALSE so it waits for a genuine replicated InputReleased (the
		// =true first-frame-fire bug documented in GA_AFL_Climb).
		if (UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased*/ false))
		{
			ReleaseTask->OnRelease.AddDynamic(this, &UAFLGameplayAbility_Sprint::OnInputReleased);
			ReleaseTask->ReadyForActivation();
		}
		return;
	}

	// BOT. WaitInputRelease would never fire -- there is no input -- so hanging the bot path off it is exactly
	// the AI-0 latch: activate, and nothing ever ends it.
	//
	// A LEASE, NOT A STOP EVENT. The tempting design is Start/Stop events, and it is wrong: when the bot leaves
	// the Shoot And Move branch (enemy lost, out of ammo, killed) the service stops ticking and NEVER SENDS
	// STOP -- sprint latches forever, which is the bug with extra steps. A lease inverts the failure: the
	// requester must keep asking, and anything that stops the asking -- branch exit, death, BT abort, a dropped
	// event -- expires the sprint on its own. The failure mode of silence becomes "stops sprinting", which is
	// invisible, instead of "sprints forever", which we already shipped once.
	//
	// Renewals arrive through a task, not the trigger: the trigger cannot re-fire on an InstancedPerActor
	// ability that is already active, so a repeating WaitGameplayEvent on the SAME tag catches them.
	if (UAbilityTask_WaitGameplayEvent* RenewTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, TAG_Event_Movement_Sprint_Requested, /*OptionalExternalTarget*/ nullptr, /*OnlyTriggerOnce*/ false))
	{
		RenewTask->EventReceived.AddDynamic(this, &UAFLGameplayAbility_Sprint::OnSprintRenewed);
		RenewTask->ReadyForActivation();
	}
	RenewBotLease();
}

void UAFLGameplayAbility_Sprint::OnSprintRenewed(FGameplayEventData /*Payload*/)
{
	RenewBotLease();
}

void UAFLGameplayAbility_Sprint::RenewBotLease()
{
	if (UWorld* World = GetWorld())
	{
		// One-shot, re-armed from scratch each time. SetTimer on a live handle replaces it, so a renewal simply
		// pushes expiry out; no accumulation, no second timer.
		World->GetTimerManager().SetTimer(BotLeaseTimer,
			FTimerDelegate::CreateWeakLambda(this, [this] { OnBotLeaseExpired(); }),
			FMath::Max(0.1f, BotSprintLeaseSeconds), /*bLoop*/ false);
	}
}

void UAFLGameplayAbility_Sprint::OnBotLeaseExpired()
{
	if (bEnding)
	{
		return; // first exit wins
	}
	bEnding = true;
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_SPRINT: bot lease expired (%.2fs without a renewal) -> end."), BotSprintLeaseSeconds);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
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
	// The lease must die with the ability. A stale timer on an InstancedPerActor ability would fire into the
	// NEXT sprint and end it early -- the instance is reused, so the handle outlives the activation.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BotLeaseTimer);
	}

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
