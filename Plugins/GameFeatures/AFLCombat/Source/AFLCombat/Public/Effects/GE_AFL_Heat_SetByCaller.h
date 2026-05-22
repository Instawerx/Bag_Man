// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Heat_SetByCaller.generated.h"


/**
 * UGE_AFL_Heat_SetByCaller
 *
 * AFL-0207 cheat-only Instant GameplayEffect that Overrides Heat with a
 * SetByCaller magnitude tagged Data.Combat.Heat. Exists so AFL.Combat.Heat /
 * AFL.Combat.ForceOverheat / AFL.Combat.ResetHeat cheats can mutate Heat
 * through the GE pipeline (firing PostGameplayEffectExecute, which is what
 * grants / clears State.Overheated at the boundaries) rather than via
 * ApplyModToAttribute, which bypasses PostGameplayEffectExecute.
 *
 * Not exposed via AbilitySet — only used from UAFLCombatCheats. Production
 * Heat changes go through GE_AFL_Heat_BeamTick / GE_AFL_Heat_Decay.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Heat_SetByCaller : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Heat_SetByCaller();
};
