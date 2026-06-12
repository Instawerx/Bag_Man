// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_StressObjectCarrier.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_StressObjectCarrier)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_Vulnerable_StressGE, "State.Carrying.Vulnerable");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_StressObject_StressGE, "State.Carrying.StressObject");


UAFLGE_StressObjectCarrier::UAFLGE_StressObjectCarrier()
{
	// Infinite: the carry has no fixed duration -- the grab ability owns the apply/remove pair by
	// handle (the carrier-vulnerability lifetime model verbatim).
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_State_Carrying_Vulnerable_StressGE);   // the proven x1.3 (S10 +30% dmg)
	Granted.Added.AddTag(TAG_State_Carrying_StressObject_StressGE); // the extract-bonus tag (S10 1.5x)
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
