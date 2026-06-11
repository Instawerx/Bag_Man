// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_EnergyGain_Small.generated.h"


/**
 * Compile-time gate for the CarriedEnergy attribute set. FLIPPED TO 1 by the energy-drops
 * cycle 1 (AFL-0701 landed: UAFLAttributeSet_Energy is real and this GE carries the live
 * Additive CarriedEnergy modifier, magnitude = SetByCaller Data.Energy.Gain). The gate is
 * retained (rather than deleted) so a platform that must compile energy out can still
 * flip it back to the documented no-op shape.
 */
#ifndef WITH_AFL_ENERGY_SET
	#define WITH_AFL_ENERGY_SET 1
#endif


/**
 * UGE_AFL_EnergyGain_Small
 *
 * AFL-0105 Instant GameplayEffect placeholder for small carried-energy
 * pickups. Modifier list intentionally empty until AFL-0701 introduces
 * UAFLAttributeSet_Energy::CarriedEnergy.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_EnergyGain_Small : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_EnergyGain_Small();
};
