// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Damage_Pulse.h"

#include "AbilitySystem/AFLDamageExecCalc.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Damage_Pulse)

// Native tag — see AFLAG_Laser_Pulse.cpp for the rationale. CDO construction
// happens at module load before ini scans, so RequestGameplayTag here would
// crash. Static native registration races first.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Pulse, "Event.Damage.Pulse");


UGE_AFL_Damage_Pulse::UGE_AFL_Damage_Pulse()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Route damage through UAFLDamageExecCalc. The ExecCalc captures
	// Source.Damage (seeded by the firing ability before MakeOutgoingSpec),
	// applies SetByCaller Headshot/Weakpoint/Distance multipliers, runs the
	// armor mitigation curve, and emits Shield/Health output modifiers. The
	// per-shot damage value lives on the ability (UAFLAG_Laser_Pulse::BaseDamage),
	// matching the working pattern in UAFLGameplayAbility_DamageTest.
	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UAFLDamageExecCalc::StaticClass();
	Executions.Add(ExecDef);

	// Grant Event.Damage.Pulse on application. UE5.5+ moved tag granting to
	// UTargetTagsGameplayEffectComponent, but the engine's templated
	// AddComponent<> calls NewObject(this, NAME_None, ...) which is illegal
	// during a UObject constructor. The supported in-ctor path is
	// CreateDefaultSubobject + manual GEComponents.Add. Listeners (FX hooks,
	// audio cues, AFL-0213 telemetry) subscribe via this tag.
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_Event_Damage_Pulse);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
