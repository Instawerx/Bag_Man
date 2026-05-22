// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Heat_VentingComplete.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Heat_VentingComplete)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Combat_HeatVentingComplete_GE, "Event.Combat.HeatVentingComplete");


UGE_AFL_Heat_VentingComplete::UGE_AFL_Heat_VentingComplete()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_Event_Combat_HeatVentingComplete_GE);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
