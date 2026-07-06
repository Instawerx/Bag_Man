// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Damage_Pulse.generated.h"


/**
 * UGE_AFL_Damage_Pulse
 *
 * AFL-0105 Instant GameplayEffect that delivers a single Pulse-weapon hit.
 *
 * Authored as a native UGameplayEffect class (the corresponding /AFLCombat/Effects/
 * GE_AFL_Damage_Pulse.uasset is the BP child that ships with the GameFeature;
 * see AFL-0214). The native class is the canonical CDO so the C++ wiring on
 * UAFLAG_Laser_Pulse can reference the class directly without needing to load
 * a Blueprint at startup.
 *
 * Shape (per master doc Sec. 8.3):
 *   - DurationPolicy: Instant
 *   - Modifier: Damage meta, EGameplayModOp::Override, magnitude 18
 *
 * NOTE: the Damage modifier feeds UAFLDamageExecCalc which routes the value
 * through Armor -> Shield -> Health. This GE itself does NOT capture target
 * attributes — it just authors Source.Damage, the ExecCalc on the spec does
 * the rest.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Damage_Pulse : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Damage_Pulse();
};
