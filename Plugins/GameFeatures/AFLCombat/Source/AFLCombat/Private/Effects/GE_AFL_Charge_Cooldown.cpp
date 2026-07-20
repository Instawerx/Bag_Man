// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Charge_Cooldown.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Charge_Cooldown)

// Native tag -- module-load-safe (declared before any CDO of this class is constructed; the AFLCombat
// tag rationale). The ini declares the same tag as the SSOT; UE dedups native+ini.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Weapon_Charge, "Cooldown.Weapon.Charge");


UGE_AFL_Charge_Cooldown::UGE_AFL_Charge_Cooldown()
{
	DurationPolicy    = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));

	// Grant Cooldown.Weapon.Charge for the duration. CreateDefaultSubobject (templated AddComponent<> can't
	// run in a UObject ctor) -- same pattern as UGE_AFL_Cooldown_Beam.
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_Cooldown_Weapon_Charge);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
