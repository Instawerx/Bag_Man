// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "GE_AFL_Heat_VentingComplete.generated.h"


/**
 * UGE_AFL_Heat_VentingComplete
 *
 * AFL-0207 Instant marker GameplayEffect applied by
 * UAFLAttributeSet_Combat::PostGameplayEffectExecute when Heat decays below
 * MaxHeat * 0.3 while the target carries State.Overheated. It exists so the
 * vent-complete transition routes through a GE (matches the brief's
 * "cleared by a GE_AFL_Heat_VentingComplete that ... removes the tag" wording
 * and keeps the broadcast path uniform with the rest of the heat system).
 *
 * The attribute set removes the State.Overheated loose tag immediately
 * before applying this GE; this GE itself just grants
 * Event.Combat.HeatVentingComplete on application so listeners (HUD pulse,
 * audio cue, telemetry) subscribe to the verb tag without coupling to Heat.
 *
 * Shape:
 *   - DurationPolicy: Instant
 *   - GrantedTags: Event.Combat.HeatVentingComplete on application
 *   - No modifiers.
 */
UCLASS()
class AFLCOMBAT_API UGE_AFL_Heat_VentingComplete : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UGE_AFL_Heat_VentingComplete();
};
