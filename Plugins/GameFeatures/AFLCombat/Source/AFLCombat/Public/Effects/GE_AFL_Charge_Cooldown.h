// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Charge_Cooldown.generated.h"


/**
 * UGE_AFL_Charge_Cooldown
 *
 * Cooldown GE applied to the firing pawn when a charge weapon FIRES (release >= MinChargeToFire). Grants
 * Cooldown.Weapon.Charge for the duration so CanActivateAbility's blocked-tag path gates re-fire. Same
 * shape as UGE_AFL_Cooldown_Beam; C++-wired on UAFLAG_Laser_Charge (CooldownGameplayEffectClass) so the
 * power weapon can never ship un-gated. Duration is designer-tunable on the BP child.
 *
 * Shape: DurationPolicy HasDuration, fixed 1.0s (on TOP of the up-to-MaxChargeTime charge, so the real
 * cycle is charge + cooldown); GrantedTags Cooldown.Weapon.Charge; no modifiers.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Charge_Cooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Charge_Cooldown();
};
