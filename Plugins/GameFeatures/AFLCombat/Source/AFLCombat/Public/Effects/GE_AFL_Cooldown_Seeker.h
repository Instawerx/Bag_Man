// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Cooldown_Seeker.generated.h"


/**
 * UGE_AFL_Cooldown_Seeker
 *
 * BLOCK-171 FIX 2: per-weapon cooldown GE for GA_AFL_Seeker. Splits the accidental 12-ability share of
 * GE_AFL_Charge_Cooldown (which granted Cooldown.Weapon.Charge, so one shot gated all twelve) into a
 * distinct per-weapon gate. Conforms to UGE_AFL_Cooldown_Beam.
 *
 * Shape: DurationPolicy HasDuration, 1.0s (INHERITED from the shared GE_AFL_Charge_Cooldown, NOT
 * independently authored -- designer to tune in S5); GrantedTags Cooldown.Weapon.Seeker; no modifiers.
 *
 * WIRING (BP half, in-editor lane -- NOT this block): GA_AFL_Seeker's CooldownGameplayEffectClass must be
 * repointed from GE_AFL_Charge_Cooldown to this class.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Cooldown_Seeker : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Cooldown_Seeker();
};
