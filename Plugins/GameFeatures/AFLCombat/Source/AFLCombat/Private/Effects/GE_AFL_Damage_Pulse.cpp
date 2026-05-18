// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Damage_Pulse.h"

#include "Attributes/AFLAttributeSet_Combat.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Damage_Pulse)


UGE_AFL_Damage_Pulse::UGE_AFL_Damage_Pulse()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Override Damage meta with 18. The value is consumed by UAFLDamageExecCalc
	// when the spec is applied; the ExecCalc captures Source.Damage and emits
	// Shield/Health output modifiers per master doc Sec. 8.3.
	FGameplayModifierInfo DamageMod;
	DamageMod.Attribute = UAFLAttributeSet_Combat::GetDamageAttribute();
	DamageMod.ModifierOp = EGameplayModOp::Override;
	DamageMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(18.0f));
	Modifiers.Add(DamageMod);

	// Grant Event.Damage.Pulse on application. UE5.5+ moved tag granting to
	// UTargetTagsGameplayEffectComponent, but the engine's templated
	// AddComponent<> calls NewObject(this, NAME_None, ...) which is illegal
	// during a UObject constructor. The supported in-ctor path is
	// CreateDefaultSubobject + manual GEComponents.Add. Listeners (FX hooks,
	// audio cues, AFL-0213 telemetry) subscribe via this tag.
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Event.Damage.Pulse")));
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
