// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Heat_CoolingGate.generated.h"


/**
 * UGE_AFL_Heat_CoolingGate
 *
 * AFL-0207 one-shot HasDuration (0.5s) GameplayEffect granted alongside
 * GE_AFL_Heat_BeamTick on every heat-producing tick. It exists only to
 * carry the State.Combat.CoolingGate tag on the source for 500ms after the
 * most recent tick — UGE_AFL_Heat_Decay's ApplicationTagRequirements.IgnoreTags
 * includes this tag, so the passive decay is suppressed while the player is
 * still firing.
 *
 * Re-application via DurationPolicy + the engine's default
 * StackingType=AggregateBySource doesn't actually refresh the duration of an
 * existing active GE. That's fine here: the beam ability calls
 * RemoveActiveGameplayEffectBySourceEffect before each apply so each tick
 * gets a fresh 0.5s window (see UAFLAG_Laser_Beam::TickChannel).
 *
 * Shape:
 *   - DurationPolicy: HasDuration, fixed 0.5s
 *   - GrantedTags: State.Combat.CoolingGate on application for the duration
 *   - No modifiers (tag-only carrier).
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Heat_CoolingGate : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Heat_CoolingGate();
};
