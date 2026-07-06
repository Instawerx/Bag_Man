// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Heat_BeamTick.h"

#include "Attributes/AFLAttributeSet_Combat.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Heat_BeamTick)


UGE_AFL_Heat_BeamTick::UGE_AFL_Heat_BeamTick()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Override HeatPerBeamTick meta with 4.0. AttributeSet's
	// PostGameplayEffectExecute folds this into the persistent Heat
	// attribute, clamps to MaxHeat, and grants State.Overheated when
	// Heat hits MaxHeat. 12.5 ticks @ 100ms = ~1.25s to overheat from 0.
	FGameplayModifierInfo HeatMod;
	HeatMod.Attribute = UAFLAttributeSet_Combat::GetHeatPerBeamTickAttribute();
	HeatMod.ModifierOp = EGameplayModOp::Override;
	HeatMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(4.0f));
	Modifiers.Add(HeatMod);
}
