// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Cooldown_VoltArcLob.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Cooldown_VoltArcLob)

// Native tag -- same module-load-vs-ini-scan rationale as the rest of the AFLCombat tags; the ini declares
// the same tag as the SSOT (UE dedups native+ini). BLOCK-171 FIX 2.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Weapon_VoltArcLob, "Cooldown.Weapon.VoltArcLob");


UGE_AFL_Cooldown_VoltArcLob::UGE_AFL_Cooldown_VoltArcLob()
{
	// 1.0s INHERITED from the shared GE_AFL_Charge_Cooldown -- NOT independently authored (BLOCK-171 FIX 2);
	// designer to tune. Preserving the only known value rather than inventing one.
	DurationPolicy    = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));

	// Grant Cooldown.Weapon.VoltArcLob for the duration. CreateDefaultSubobject (templated AddComponent<> can't
	// run in a UObject ctor) -- same pattern as UGE_AFL_Cooldown_Beam.
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_Cooldown_Weapon_VoltArcLob);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
