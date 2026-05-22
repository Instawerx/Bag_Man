// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Damage_BeamTick.generated.h"


/**
 * UGE_AFL_Damage_BeamTick
 *
 * AFL-0206 per-tick Instant GameplayEffect for the Beam channeling weapon.
 * Applied by UAFLAG_Laser_Beam every 100ms while the player holds the
 * trigger. 1.2 damage per tick at 10 ticks/sec = 12 dps baseline; designer
 * tunes in S5 alongside the heat system (AFL-0207).
 *
 * Shape (parallel to UGE_AFL_Damage_Pulse):
 *   - DurationPolicy: Instant
 *   - Modifier: Damage meta, EGameplayModOp::Override, magnitude 1.2
 *   - GrantedTags: Event.Damage.BeamTick on application
 *
 * The Damage modifier feeds UAFLDamageExecCalc, which routes through
 * Armor -> Shield -> Health (master doc Sec. 8.3). This GE does NOT
 * capture target attributes itself — it authors Source.Damage and the
 * ExecCalc on the spec handles the rest.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Damage_BeamTick : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Damage_BeamTick();
};
