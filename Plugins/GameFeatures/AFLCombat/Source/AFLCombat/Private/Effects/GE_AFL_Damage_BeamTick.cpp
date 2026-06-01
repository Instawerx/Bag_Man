// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Damage_BeamTick.h"

#include "AbilitySystem/AFLDamageExecCalc.h"
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

	// Route damage through UAFLDamageExecCalc -- IDENTICAL shape to GE_AFL_Damage_Pulse.
	// BM-0103 BUG FIX: this GE previously used a DIRECT Override Modifier writing the Damage
	// META-attribute (1.2). But Damage is a transit meta the AttributeSet's
	// PostGameplayEffectExecute just ZEROES -- with NO ExecCalc execution, NOTHING converted
	// Damage->Health, so the beam logged 1.2/tick while the target's Health never moved (and
	// the ability's Source.Damage seed was irrelevant, because this GE didn't read Source.Damage
	// -- it wrote the meta directly). The fix is to mirror Pulse: drop the direct Modifier, run
	// the ExecCalc. The ExecCalc captures Source.Damage (now seeded by UAFLAG_Laser_Beam before
	// MakeOutgoingSpec, = 1.2/tick), applies the SetByCaller multipliers + armor mitigation, and
	// emits the Shield/Health output modifiers. Per-tick value lives on the ability, like Pulse.
	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UAFLDamageExecCalc::StaticClass();
	Executions.Add(ExecDef);

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
