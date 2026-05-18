// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_EnergyGain_Small.generated.h"


/**
 * Compile-time gate for the CarriedEnergy attribute set. Flipped to 1 by
 * AFL-0701 when the Energy/Carry attribute set lands. Until then this GE is
 * an Instant no-op that exists only so:
 *   1. The Pulse weapon and downstream systems can reference the class by
 *      type without dragging in an as-yet-unauthored attribute.
 *   2. AFLCombatCheats can route `AFL.Combat.EnergyGain` through it and log
 *      the AFLCombatCheats: OK EnergyGain token the orchestrator's cheat
 *      matrix expects (the OK token gates the build regardless of whether
 *      energy is wired yet).
 */
#ifndef WITH_AFL_ENERGY_SET
	#define WITH_AFL_ENERGY_SET 0
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
