// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "AFLGE_OverdriveBuff.generated.h"

/**
 * UAFLGE_OverdriveBuff  (energy cycle 2 -- the threshold reward)
 *
 * Infinite-duration buff applied by UAFLOverdriveComponent when CarriedEnergy crosses
 * OverdriveThreshold upward (the Event.Energy.ThresholdReached listener -- NO GameplayAbility:
 * an automatic buff has no input/commit/cancel semantics a GA would provide; gates-are-GEs).
 *
 * Carries: TargetTags granting State.Energy.Overdrive (the ExecCalc +25% source seam and the
 * component's CMC speed swap both key off it) · the looping GameplayCue.Energy.Overdrive body
 * cue · a PERIODIC (1s) Additive CarriedEnergy drain, magnitude = SetByCaller Data.Energy.Drain
 * (negative, set by the component at apply from afl.Energy.DrainPerSecond -- the same
 * negative-SetByCaller rail as the death burst; no direct attribute writes).
 *
 * Exit = the component removes BY HANDLE at CarriedEnergy 0 (full consumption) or death; the
 * set's upward-crossing-only broadcast gives re-trigger hysteresis for free (must re-cross 80).
 */
UCLASS()
class AFLCOMBAT_API UAFLGE_OverdriveBuff : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAFLGE_OverdriveBuff();
};
