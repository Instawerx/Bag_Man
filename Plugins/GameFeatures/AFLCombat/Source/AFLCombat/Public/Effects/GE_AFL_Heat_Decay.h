// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Heat_Decay.generated.h"


/**
 * UGE_AFL_Heat_Decay
 *
 * AFL-0207 infinite-duration Duration GameplayEffect granted once on pawn
 * setup. Periodically (every 100ms) it subtracts (HeatDecayRate * 0.1) from
 * Heat — default -2.0 per tick = -20.0/s.
 *
 * Gated by State.Combat.CoolingGate: while the source carries the cooling-gate
 * tag (granted by GE_AFL_Heat_CoolingGate for 0.5s after every beam tick),
 * OngoingTagRequirements.IgnoreTags suppresses the period without removing the
 * GE itself — so the moment the gate expires the decay resumes its 100ms
 * cadence.
 *
 * The negative-Heat write triggers UAFLAttributeSet_Combat::PostGameplayEffectExecute,
 * which clears State.Overheated once Heat drops below MaxHeat * 0.3 (the
 * vent-complete boundary is emitted as the AFL_LOG: heat_vented log line).
 *
 * Shape:
 *   - DurationPolicy: Infinite
 *   - Period: 0.1s (executes the modifier list every 100ms)
 *   - Modifier: Heat (Additive), magnitude = -0.1 * captured HeatDecayRate (Target, live)
 *   - OngoingTagRequirements.IgnoreTags: State.Combat.CoolingGate
 *
 * Apply once per pawn from the C++ ASC init path (or via an AbilitySet GE
 * grant when AFL-0214 lands). Multiple applies are harmless — the modifier
 * stacks Additive on Heat but the clamp [0..MaxHeat] in PreAttributeChange
 * makes overstacked decay a no-op at the floor.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Heat_Decay : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Heat_Decay();
};
