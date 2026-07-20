// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Damage_Charge.h"

#include "AbilitySystem/AFLDamageExecCalc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Damage_Charge)


UGE_AFL_Damage_Charge::UGE_AFL_Damage_Charge()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Route through UAFLDamageExecCalc: it captures Source.Damage (seeded charge-scaled by
	// UAFLAG_Laser_Charge::ServerApplyTargetData before MakeOutgoingSpec), applies the SetByCaller
	// Headshot/Weakpoint/Distance multipliers, runs armor mitigation, and emits Shield/Health output
	// modifiers -- the exact proven pipeline the Pulse/Beam damage GEs use.
	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UAFLDamageExecCalc::StaticClass();
	Executions.Add(ExecDef);
}
