// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Heat_SetByCaller.h"

#include "Attributes/AFLAttributeSet_Combat.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Heat_SetByCaller)


UGE_AFL_Heat_SetByCaller::UGE_AFL_Heat_SetByCaller()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Override Heat with the SetByCaller magnitude. The PostGameplayEffectExecute
	// path on Heat fires the vent-complete transition (if applicable) but does
	// NOT auto-grant Overheated on this code path — Overheated grants only
	// come from the HeatPerBeamTick meta route. Cheats that want to set
	// Overheated do so explicitly via the loose-tag API after applying this GE.
	FGameplayModifierInfo HeatMod;
	HeatMod.Attribute    = UAFLAttributeSet_Combat::GetHeatAttribute();
	HeatMod.ModifierOp   = EGameplayModOp::Override;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = FGameplayTag::EmptyTag; // Resolved at apply time via DataName.
	SetByCaller.DataName = TEXT("Data.Combat.Heat");

	FGameplayEffectModifierMagnitude Magnitude;
	Magnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	HeatMod.ModifierMagnitude = Magnitude;
	Modifiers.Add(HeatMod);
}
