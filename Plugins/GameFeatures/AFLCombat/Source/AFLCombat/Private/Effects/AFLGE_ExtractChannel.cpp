// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_ExtractChannel.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_ExtractChannel)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Extracting_ChannelGE, "State.Extracting");
// Lyra's native CMC tag (UE_DEFINE_GAMEPLAY_TAG in LyraCharacterMovementComponent.cpp) -- we
// declare our own static handle to the SAME tag name rather than linking the Lyra symbol.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Gameplay_MovementStopped_ChannelGE, "Gameplay.MovementStopped");


UAFLGE_ExtractChannel::UAFLGE_ExtractChannel()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_State_Extracting_ChannelGE);
	Granted.Added.AddTag(TAG_Gameplay_MovementStopped_ChannelGE);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
