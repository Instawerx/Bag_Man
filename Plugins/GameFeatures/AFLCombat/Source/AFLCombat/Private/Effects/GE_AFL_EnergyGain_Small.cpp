// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_EnergyGain_Small.h"

#if WITH_AFL_ENERGY_SET
#include "Attributes/AFLAttributeSet_Energy.h"
#include "NativeGameplayTags.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_EnergyGain_Small)

#if WITH_AFL_ENERGY_SET
// Native tag = safe in a CDO ctor (module-init registration precedes any CDO construction in
// this module -- the Pulse.cpp:29-36 rationale). The TAG form matches the existing setters:
// the AFL.Combat.EnergyGain cheat and the pickup both SetSetByCallerMagnitude by this tag.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Energy_Gain_GE, "Data.Energy.Gain");
#endif


UGE_AFL_EnergyGain_Small::UGE_AFL_EnergyGain_Small()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

#if WITH_AFL_ENERGY_SET
	// Energy-drops cycle 1: the live modifier. Additive CarriedEnergy, magnitude supplied per
	// application via SetByCaller(Data.Energy.Gain) -- positive for pickups/cheat gains, NEGATIVE
	// for the death-burst victim reduction (UAFLEnergyDropComponent). Clamping lives on the
	// attribute set (PreAttributeChange 0..MaxEnergy), threshold broadcast in its
	// PostGameplayEffectExecute.
	FGameplayModifierInfo Mod;
	Mod.Attribute = UAFLAttributeSet_Energy::GetCarriedEnergyAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAG_Data_Energy_Gain_GE;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(Mod);
#endif
}
