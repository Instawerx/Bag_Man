// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Heat_BeamTick.generated.h"


/**
 * UGE_AFL_Heat_BeamTick
 *
 * AFL-0207 per-tick Instant GameplayEffect that adds 4.0 to the source's
 * HeatPerBeamTick meta. UAFLAttributeSet_Combat::PostGameplayEffectExecute
 * folds the delta into the persistent Heat attribute, clamps to [0..MaxHeat],
 * and grants State.Overheated when Heat hits MaxHeat. At 1 tick/100ms this
 * runs 100 -> 0..100 in 12.5 ticks ≈ 1.25 seconds of held fire (matches the
 * brief).
 *
 * Shape (parallel to UGE_AFL_Damage_BeamTick):
 *   - DurationPolicy: Instant
 *   - Modifier: HeatPerBeamTick meta, EGameplayModOp::Override, magnitude 4.0
 *   - GrantedTags: Event.Combat.HeatVentingStart on application (designers /
 *     listeners can subscribe to the overheat boundary without coupling to
 *     the Heat attribute directly).
 *
 * Applied source-to-source by UAFLAG_Laser_Beam from the same delegate that
 * dispatches GE_AFL_Damage_BeamTick to the hit target.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Heat_BeamTick : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Heat_BeamTick();
};
