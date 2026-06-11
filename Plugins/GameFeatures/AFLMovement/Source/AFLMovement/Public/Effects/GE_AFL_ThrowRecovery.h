// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_ThrowRecovery.generated.h"

/**
 * UGE_AFL_ThrowRecovery  (throw cycle / drop-on-damage close)
 *
 * Short Duration GE (0.4s) granting State.Weapon.ThrowRecovery, applied by UAFLGameplayAbility_Throw at
 * the moment of the throw. The fire abilities (Pulse/Beam/BeamChannel_v2) carry the tag in their
 * ActivationBlockedTags alongside State.Carrying, so the press (or hold) that THREW cannot also fire the
 * holstered-then-restored weapon. Time-based on purpose: input-side press-scoping is unreliable here --
 * IA_Weapon_Fire's trigger reports Completed one frame after the press even while the button is held
 * (the same trigger semantics behind the climb WaitInputRelease trap), so an ability lifetime keyed to
 * InputReleased collapses immediately. A GE window is stateless, input-agnostic, and GAS-canonical
 * (gates are GameplayEffects -- the cooldown doctrine). Holding fire PAST the window deliberately starts
 * the weapon (empty hands + held trigger = fire), which reads as throw recovery.
 */
UCLASS()
class AFLMOVEMENT_API UGE_AFL_ThrowRecovery : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_AFL_ThrowRecovery();
};
