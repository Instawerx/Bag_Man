// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "InputAction.h"

#include "AFLInputAction_Fire.generated.h"


/**
 * UAFLInputAction_Fire
 *
 * AFL-0107 native UInputAction for the Pulse weapon's primary fire input. The
 * ULyraInputConfig binds this action's CDO to InputTag.Weapon.Fire, and Lyra's
 * input wiring routes Triggered events to the ability with the matching ability
 * input tag (UAFLAG_Laser_Pulse, set up by the AbilitySet in AFL-0214).
 *
 * Shape (per AFL-0107 brief):
 *   - ValueType: Boolean (digital).
 *   - Triggers: UInputTriggerPressed — fires once on press, supporting both the
 *     Started and Triggered events on the same frame (UInputTriggerPressed
 *     reports ETriggerEventsSupported::Instant, which the Enhanced Input system
 *     translates into a Started→Triggered transition the first frame the input
 *     actuates above threshold). This matches Lyra's existing ability-fire
 *     binding pattern.
 *
 * Authored as a native UInputAction subclass so:
 *   1. The ULyraInputConfig data asset (UAFLInputConfig_Combat) can reference
 *      the CDO directly without loading a .uasset at module startup.
 *   2. The corresponding /AFLCombat/Input/IA_AFL_Fire.uasset is a BP child of
 *      this class authored alongside DA_AFL_InputConfig_Combat in AFL-0214.
 */
UCLASS(NotBlueprintable)
class AFLCOMBAT_API UAFLInputAction_Fire : public UInputAction
{
	GENERATED_BODY()

public:

	UAFLInputAction_Fire();
};
