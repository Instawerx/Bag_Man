// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Damage_Pulse.h"

#include "AbilitySystem/AFLDamageExecCalc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Damage_Pulse)


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
}
