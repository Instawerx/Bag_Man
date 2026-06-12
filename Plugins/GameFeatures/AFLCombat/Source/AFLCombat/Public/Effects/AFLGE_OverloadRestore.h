// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "AFLGE_OverloadRestore.generated.h"

/**
 * UAFLGE_OverloadRestore  (P2 close-out overload -- S7 AFL-0706)
 *
 * Instant GE that restores Health toward a floor when a player OVERLOADS (survives a killing blow
 * while carrying energy, instead of dying). Additive Health modifier driven by SetByCaller
 * Data.Health.Restore -- the intercept computes magnitude = max(0, floor(0.5*MaxHealth) - current)
 * at apply time and supplies it (the negative-SetByCaller energy rail run POSITIVE; NO direct
 * attribute write -- the doctrine). Clamping lives on UAFLAttributeSet_Combat::PreAttributeChange.
 *
 * Mirror of UGE_AFL_EnergyGain_Small's SetByCaller-modifier ctor.
 */
UCLASS()
class AFLCOMBAT_API UAFLGE_OverloadRestore : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAFLGE_OverloadRestore();
};
