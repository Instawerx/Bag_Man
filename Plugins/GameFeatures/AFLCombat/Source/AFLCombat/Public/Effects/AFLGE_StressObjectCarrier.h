// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "AFLGE_StressObjectCarrier.generated.h"

/**
 * UAFLGE_StressObjectCarrier  (P2 close-out stress-object chaos -- S10 AFL-0801/0803)
 *
 * The multi-effect CarrierEffectClass the stress-object grabbable points at (FAFLGrabPolicy.
 * CarrierEffectClass). Infinite, applied/removed by the EXISTING grab handle path
 * (AFLGameplayAbility_Grab.cpp:301-308 apply / :524-528 remove) -- NO new carry code. Grants two tags:
 *  - State.Carrying.Vulnerable  -- the PROVEN x1.3 incoming-damage amplifier (UAFLDamageExecCalc step
 *    5b). The S10 "+30% damage taken" carrier multiplier IS this -- ZERO new damage code.
 *  - State.Carrying.StressObject -- the extract-bonus tag. UAFLAG_Extract reads it to pay
 *    afl.Chaos.ExtractMult x Watts (the S10 "1.5x extraction reward").
 *
 * The "2x score" multiplier is deferred (no score system -- named-debt; gate 4 does not require it).
 * Mirror of UGE_AFL_CarrierVulnerability's TargetTags ctor, just with the second tag.
 */
UCLASS()
class AFLCOMBAT_API UAFLGE_StressObjectCarrier : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAFLGE_StressObjectCarrier();
};
