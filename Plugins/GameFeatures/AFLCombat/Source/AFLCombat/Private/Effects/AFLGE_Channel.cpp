// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_Channel.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_Channel)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Channeling_ChannelGE, "State.Channeling");

UAFLGE_Channel::UAFLGE_Channel()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	// State.Channeling ONLY -- deliberately NOT Gameplay.MovementStopped. The CARRY collect-channel is the
	// REQUIRED divergence from extract: Decision 3 is "stand-and-channel, MOVING AWAY cancels it (exposing
	// you)" -- moving must be POSSIBLE, so no movement lock. The base ability's MaxMoveRadius poll is the live
	// "stand still or lose it" interrupt (the lock was what made that move-interrupt dead code). HARVEST
	// (Phase 4) can add its own lock GE if it wants a hard stand-still; the collect-channel does not.
	Granted.Added.AddTag(TAG_State_Channeling_ChannelGE);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
