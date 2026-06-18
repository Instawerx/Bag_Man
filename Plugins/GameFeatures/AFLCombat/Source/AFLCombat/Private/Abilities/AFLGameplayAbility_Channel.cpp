// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Channel.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Effects/AFLGE_Channel.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Messages/AFLHitConfirmMessage.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Channel)

// Native tags (module-init-safe, like extract). State.Channeling is granted by UAFLGE_Channel + gates
// re-activation; the Event.Channel.* verbs are the generic progress broadcasts (a future channel HUD).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Channeling_Channel, "State.Channeling");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Channel_Start_Channel, "Event.Channel.Start");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Channel_Complete_Channel, "Event.Channel.Complete");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Channel_Interrupted_Channel, "Event.Channel.Interrupted");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Confirmed_Channel, "Event.Damage.Confirmed");

// Test/tuning knob (mirrors extract's afl.Extract.ChannelDuration): override the channel seconds for ANY
// UAFLGameplayAbility_Channel. -1 = use the ability's ChannelDuration. The carry-channel test runner
// (afl.LootCarry.Test.Run) sets a known value so it can time its mid-channel move/damage interrupts.
static TAutoConsoleVariable<float> CVarAFLChannelDurationOverride(
	TEXT("afl.Channel.DurationOverride"),
	-1.0f,
	TEXT("Override the channel duration (s) for any UAFLGameplayAbility_Channel (-1 = the ability default). Test/tuning knob."));

UAFLGameplayAbility_Channel::UAFLGameplayAbility_Channel()
{
	// Instance state (handles + listeners + timer) -> per-actor. LocalPredicted: the local State.Channeling
	// (ActivationOwnedTags) starts the UI bar instantly; every COMMIT is authority-gated so prediction can
	// never grant. Subclasses set AbilityTriggers (their GameplayEvent) -- the base is event-triggered (D).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationOwnedTags.AddTag(TAG_State_Channeling_Channel);
	// No double-channel (collect AND harvest are mutually exclusive while one is live).
	ActivationBlockedTags.AddTag(TAG_State_Channeling_Channel);

	ChannelEffectClass = UAFLGE_Channel::StaticClass();
}

void UAFLGameplayAbility_Channel::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	bChannelCompleted = false;
	// Decision D: the channeled object rides the trigger event (the grab ability set Target = the cache).
	ChannelTarget = TriggerEventData ? TriggerEventData->Target : nullptr;

	// Effective channel duration: the cvar test/tuning override (afl.Channel.DurationOverride), else the
	// ability default. Drives the START log/broadcast + the WaitDelay clock identically.
	const float DurationOverride = CVarAFLChannelDurationOverride.GetValueOnGameThread();
	const float Duration = (DurationOverride > 0.0f) ? DurationOverride : ChannelDuration;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		ChannelStartLocation = Avatar->GetActorLocation();
	}

	if (HasAuthority(&ActivationInfo) && ASC && World)
	{
		// Channel-window GE by handle (State.Channeling replicated; NO movement lock -- move-cancel per Decision 3,
		// so the MaxMoveRadius poll below is the LIVE "stand still or lose it" interrupt).
		if (ChannelEffectClass)
		{
			const FGameplayEffectSpecHandle SpecHandle =
				MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, ChannelEffectClass, 1.0f);
			if (SpecHandle.IsValid())
			{
				ChannelEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
			}
		}

		// Damage interrupt (extract's exact consumer pattern -- the ExecCalc broadcasts on the authority).
		DamageListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FAFLHitConfirmMessage>(
			TAG_Event_Damage_Confirmed_Channel,
			[this](FGameplayTag Channel, const FAFLHitConfirmMessage& Msg) { HandleDamageConfirmed(Channel, Msg); });

		// Movement interrupt (NEW -- no extract precedent): poll the move radius on a timer (not Tick).
		if (MaxMoveRadius > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				MoveCheckTimer, this, &UAFLGameplayAbility_Channel::CheckMoveRadius, MoveCheckInterval, /*bLoop=*/true);
		}

		BroadcastChannelEvent(TAG_Event_Channel_Start_Channel, Duration);
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHANNEL: %s START (%.1fs) target=%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()), Duration, *GetNameSafe(ChannelTarget.Get()));
	}

	// The channel clock. Task auto-ends with the ability on every exit path.
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	WaitTask->OnFinish.AddDynamic(this, &UAFLGameplayAbility_Channel::HandleChannelComplete);
	WaitTask->ReadyForActivation();
}

void UAFLGameplayAbility_Channel::HandleChannelComplete()
{
	// SUCCESS COMMIT -- authority only (the client instance just ends; the payoff replicates down).
	if (HasAuthority(&CurrentActivationInfo))
	{
		bChannelCompleted = true;
		OnChannelComplete(ChannelTarget.Get());   // subclass payoff (collect from the target / harvest)
		BroadcastChannelEvent(TAG_Event_Channel_Complete_Channel, ChannelDuration);
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHANNEL: %s COMPLETE target=%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(ChannelTarget.Get()));
	}
	K2_EndAbility();
}

void UAFLGameplayAbility_Channel::HandleDamageConfirmed(FGameplayTag /*Channel*/, const FAFLHitConfirmMessage& Msg)
{
	// DAMAGE INTERRUPT (authority -- the listener only exists there). Any confirmed hit ON the channeler
	// breaks the channel (extract's filter). No payoff (Event.Channel.Interrupted fires from EndAbility).
	if (Msg.Target != GetAvatarActorFromActorInfo())
	{
		return;
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHANNEL: %s INTERRUPTED by damage (%.1f)"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), Msg.Damage);
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateCancelAbility=*/true);
}

void UAFLGameplayAbility_Channel::CheckMoveRadius()
{
	// MOVEMENT INTERRUPT (NEW): cancel if the channeler walked beyond MaxMoveRadius from the start point.
	// Squared-distance (no sqrt). Authority drives the timer (set only on the authority path).
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}
	const float DistSq = FVector::DistSquared(Avatar->GetActorLocation(), ChannelStartLocation);
	if (DistSq > (MaxMoveRadius * MaxMoveRadius))
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_CHANNEL: %s CANCELLED -- moved %.0fcm (> %.0f radius)"),
			*GetNameSafe(Avatar), FMath::Sqrt(DistSq), MaxMoveRadius);
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateCancelAbility=*/true);
	}
}

void UAFLGameplayAbility_Channel::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// THE funnel -- success / damage / move / death / teardown all land here exactly once.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MoveCheckTimer);
	}
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (HasAuthority(&ActivationInfo) && ChannelEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(ChannelEffectHandle);
			ChannelEffectHandle.Invalidate();
		}
	}
	if (DamageListener.IsValid())
	{
		DamageListener.Unregister();
	}
	// Every non-complete end is an INTERRUPTED channel (the single broadcast home -- interrupt handlers
	// can never double-fire it).
	if (HasAuthority(&ActivationInfo) && !bChannelCompleted)
	{
		BroadcastChannelEvent(TAG_Event_Channel_Interrupted_Channel, 0.0f);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAFLGameplayAbility_Channel::BroadcastChannelEvent(const FGameplayTag& EventTag, float Magnitude) const
{
	// FLyraVerbMessage (extract's broadcast shape; Instigator = the channeling pawn).
	if (UWorld* World = GetWorld())
	{
		FLyraVerbMessage Message;
		Message.Verb = EventTag;
		Message.Instigator = GetAvatarActorFromActorInfo();
		Message.Magnitude = Magnitude;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(Message.Verb, Message);
	}
}
