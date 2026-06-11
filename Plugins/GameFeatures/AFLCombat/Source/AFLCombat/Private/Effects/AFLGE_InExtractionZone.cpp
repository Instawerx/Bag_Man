// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_InExtractionZone.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_InExtractionZone)

// Native tag -- same module-load-vs-ini-scan rationale as the rest of the AFL tags.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_InExtractionZone_GE, "State.InExtractionZone");


UAFLGE_InExtractionZone::UAFLGE_InExtractionZone()
{
	// Infinite: presence has no fixed duration -- lifetime is owned entirely by the zone's
	// handle-tracked overlap apply/remove pair (the carrier-vulnerability ctor pattern).
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_State_InExtractionZone_GE);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
