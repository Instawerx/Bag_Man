// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Effects/GE_AFL_Heat_BeamTick.h"

#include "GE_AFL_Heat_CutterTick.generated.h"


/**
 * UGE_AFL_Heat_CutterTick
 *
 * Weapon #8 (Beam Cutter) per-tick heat -- a child of UGE_AFL_Heat_BeamTick
 * retuned for the cutter's ruled window: 2.5 heat/tick (parent: 4.0) at
 * 10 ticks/s against MaxHeat 100 = ~4.0s of continuous cut before the
 * AttributeSet grants State.Overheated and UAFLAG_BeamChannel_v2 force-ends
 * the channel. Same meta-attribute plumbing (HeatPerBeamTick Override ->
 * PostGameplayEffectExecute fold), one retuned number.
 *
 * Vent math at current AttributeSet defaults (HeatDecayRate 20, clear at
 * MaxHeat*0.3): 0.5s CoolingGate tail + 70 heat / 20 per s = ~4.0s to re-fire.
 * The ~3s-vent dial, if PIE wants it: HeatDecayRate 20 -> 28 (0.5 + 2.5s).
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Heat_CutterTick : public UGE_AFL_Heat_BeamTick
{
	GENERATED_BODY()

public:

	UGE_AFL_Heat_CutterTick();
};
