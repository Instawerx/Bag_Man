// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Cooldown_Beam.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Cooldown_Beam)

// Native tag — same module-load-vs-ini-scan rationale as the rest of the
// AFLCombat tags. UE_DEFINE_GAMEPLAY_TAG_STATIC at file scope registers
// before any CDO of this class is constructed.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Weapon_Beam, "Cooldown.Weapon.Beam");


UGE_AFL_Cooldown_Beam::UGE_AFL_Cooldown_Beam()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.0f));

	// Grant Cooldown.Weapon.Beam on application for the GE duration. Same
	// CreateDefaultSubobject pattern as the damage GEs — templated
	// AddComponent<> can't be called from a UObject ctor.
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_Cooldown_Weapon_Beam);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
