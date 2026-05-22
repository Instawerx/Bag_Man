// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Cooldown_Beam.generated.h"


/**
 * UGE_AFL_Cooldown_Beam
 *
 * AFL-0206 placeholder cooldown GE applied to the firing pawn when the
 * Beam channel ends. Grants Cooldown.Weapon.Beam for 3 seconds; designer
 * tunes the duration in S5 alongside heat (AFL-0207).
 *
 * Shape:
 *   - DurationPolicy: HasDuration, fixed 3.0s
 *   - GrantedTags: Cooldown.Weapon.Beam for the duration
 *
 * This GE has no modifiers — its only job is to drop the cooldown tag onto
 * the source ASC so CanActivateAbility's HasMatchingGameplayTag block path
 * gates re-activation. Real cooldown spec (designer-tuned scalable
 * duration, BP child for variant tuning) lands in AFL-0214 like the Pulse
 * cooldown does.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Cooldown_Beam : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Cooldown_Beam();
};
