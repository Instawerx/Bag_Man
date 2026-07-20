// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Damage_Charge.generated.h"


/**
 * UGE_AFL_Damage_Charge
 *
 * Instant GameplayEffect for a CHARGE-weapon hit. Identical shape to UGE_AFL_Damage_Pulse: routes the
 * damage through UAFLDamageExecCalc via Source.Damage -- which UAFLAG_Laser_Charge seeds CHARGE-SCALED
 * (BaseDamage * ChargeCurve.Eval(Norm)) before MakeOutgoingSpec. A distinct class (vs reusing the Pulse
 * GE) so the charge weapon's damage/mitigation shape can be tuned independently on its BP child.
 *
 * Shape (master doc Sec. 8.3): DurationPolicy Instant; one UAFLDamageExecCalc execution; no captures
 * here -- the ExecCalc does armor -> shield -> health off the seeded Source.Damage.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Damage_Charge : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Damage_Charge();
};
