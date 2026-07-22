// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Effects/GE_AFL_Damage_BeamTick.h"

#include "GE_AFL_Damage_CutterTick.generated.h"


/**
 * UGE_AFL_Damage_CutterTick
 *
 * Weapon #8 (Beam Cutter) per-tick Instant damage GE -- a child of the proven
 * UGE_AFL_Damage_BeamTick (the UAFLDamageExecCalc execution wiring is inherited
 * verbatim from the parent ctor) that raises the per-tick base to 2.1
 * (= 1.2 x 1.75): 21 dps on-target at 10 ticks/s, paid for by the heat window
 * (~4s continuous cut -> overheat -> vent). The cutter out-DPSes the loadout
 * ShotgunBeam ONLY while tracking -- every off-target tick is zero.
 *
 * The 2.1 rides in a Damage-meta carrier Modifier: UAFLAG_BeamChannel_v2 reads
 * DamageEffectClass's Modifiers[0] static magnitude to seed Source.Damage before
 * MakeOutgoingSpec -- its designed tune-point ("sourced from DamageEffectClass's
 * first Damage modifier so children that tune it stay in sync"; the parent has
 * NO modifier and takes the ability's 1.2 fallback). The modifier itself is
 * gameplay-inert on execute: Damage is a transit meta the AttributeSet's
 * PostGameplayEffectExecute zeroes without conversion (BM-0103) -- all real
 * damage flows through the inherited ExecCalc from the captured Source.Damage.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Damage_CutterTick : public UGE_AFL_Damage_BeamTick
{
	GENERATED_BODY()

public:

	UGE_AFL_Damage_CutterTick();
};
