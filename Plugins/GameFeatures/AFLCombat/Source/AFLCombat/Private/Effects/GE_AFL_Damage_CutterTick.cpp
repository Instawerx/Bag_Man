// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Damage_CutterTick.h"

#include "Attributes/AFLAttributeSet_Combat.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Damage_CutterTick)


UGE_AFL_Damage_CutterTick::UGE_AFL_Damage_CutterTick()
{
	// Parent ctor already routed damage through UAFLDamageExecCalc (Executions[0]).
	// This child only carries the raised per-tick base: 2.1 = 1.2 x 1.75. The ability
	// reads Modifiers[0]'s static magnitude to seed Source.Damage (see class comment);
	// the modifier's own execute is inert (Damage meta is zeroed, never converted).
	FGameplayModifierInfo DamageCarrier;
	DamageCarrier.Attribute         = UAFLAttributeSet_Combat::GetDamageAttribute();
	DamageCarrier.ModifierOp        = EGameplayModOp::Override;
	DamageCarrier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.1f));
	Modifiers.Add(DamageCarrier);
}
