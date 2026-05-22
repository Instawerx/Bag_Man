// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Damage_BeamTick.h"

#include "Attributes/AFLAttributeSet_Combat.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Damage_BeamTick)

// Native tag — same rationale as GE_AFL_Damage_Pulse: CDO construction
// happens at module load before ini scans, so RequestGameplayTag in the
// ctor would crash. Static native registration races first.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_BeamTick, "Event.Damage.BeamTick");


UGE_AFL_Damage_BeamTick::UGE_AFL_Damage_BeamTick()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Override Damage meta with 1.2 per tick (12 dps at the ability's 10 Hz
	// tick rate). Consumed by UAFLDamageExecCalc when the spec is applied.
	FGameplayModifierInfo DamageMod;
	DamageMod.Attribute = UAFLAttributeSet_Combat::GetDamageAttribute();
	DamageMod.ModifierOp = EGameplayModOp::Override;
	DamageMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.2f));
	Modifiers.Add(DamageMod);

	// Grant Event.Damage.BeamTick on application. Same UTargetTagsGameplayEffectComponent
	// pattern as Pulse — engine's templated AddComponent<> can't be called
	// from a ctor (NewObject with NAME_None), so we use CreateDefaultSubobject
	// + manual GEComponents.Add.
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_Event_Damage_BeamTick);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
