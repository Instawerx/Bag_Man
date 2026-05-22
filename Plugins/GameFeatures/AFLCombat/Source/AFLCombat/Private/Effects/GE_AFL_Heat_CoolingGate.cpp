// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Heat_CoolingGate.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Heat_CoolingGate)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Combat_CoolingGate_Carrier, "State.Combat.CoolingGate");


UGE_AFL_Heat_CoolingGate::UGE_AFL_Heat_CoolingGate()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.5f));

	// Carry State.Combat.CoolingGate on the source for the full duration.
	// GE_AFL_Heat_Decay's OngoingTagRequirements.IgnoreTags references this
	// tag, so the decay is suppressed while CoolingGate is granted.
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_State_Combat_CoolingGate_Carrier);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
