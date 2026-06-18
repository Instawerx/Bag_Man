// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_CollectChannel.h"

#include "AFLCombat.h"
#include "GameFramework/Actor.h"
#include "Loot/AFLLootGrantComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_CollectChannel)

// The trigger event the grab ability fires for a CollectLoot grabbable (Piece 1). Native-defined here;
// the spec row also lives in AFLCombatTags.ini (reload-at-relaunch) so AFLMovement's grab ability resolves
// the SAME tag when it sends the event (the broadcast/listen-tag-in-ini rule).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Loot_CollectChannel, "Event.Loot.CollectChannel");

UAFLAG_CollectChannel::UAFLAG_CollectChannel()
{
	// Event-triggered (Decision D): no input/zone gate -- the grab ability sends the event with the cache.
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = TAG_Event_Loot_CollectChannel;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);

	// Short, exposed collect-channel (v3 decision 3). Movement/damage cancels it (the base's interrupts).
	ChannelDuration = 1.5f;
}

void UAFLAG_CollectChannel::OnChannelComplete(const AActor* TargetActor)
{
	// Authority-only (base gates this). Pay from the CHANNELED cache: its grant component's TryGrant is the
	// guarded, grant-once, owner-seam entry -- identical to the cache's old OnGrabbedBy path, just driven by
	// the channel's completion instead of the hand-grab. The retriever is the channeler (the pawn).
	if (!TargetActor)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_CHANNEL: collect-complete with no target -- nothing granted."));
		return;
	}
	if (UAFLLootGrantComponent* Grant = TargetActor->FindComponentByClass<UAFLLootGrantComponent>())
	{
		AActor* Channeler = GetAvatarActorFromActorInfo();
		Grant->TryGrant(Channeler);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_CHANNEL: collect-complete -> TryGrant on %s by %s"),
			*GetNameSafe(TargetActor), *GetNameSafe(Channeler));
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_CHANNEL: collect target %s has no UAFLLootGrantComponent."),
			*GetNameSafe(TargetActor));
	}
}
