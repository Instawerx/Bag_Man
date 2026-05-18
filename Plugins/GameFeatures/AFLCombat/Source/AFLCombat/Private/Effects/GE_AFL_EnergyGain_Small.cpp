// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_EnergyGain_Small.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_EnergyGain_Small)


UGE_AFL_EnergyGain_Small::UGE_AFL_EnergyGain_Small()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

#if WITH_AFL_ENERGY_SET
	// AFL-0701 lands the CarriedEnergy attribute and wires the +N Additive
	// modifier here. For now this branch is dead so the asset can ship as a
	// no-op that the cheat manager still treats as OK.
#endif
}
