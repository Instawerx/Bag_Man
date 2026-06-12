// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "AFLGE_OverloadStun.generated.h"

/**
 * UAFLGE_OverloadStun  (P2 close-out overload -- S7 AFL-0706)
 *
 * Duration GE (~1s) applied to the player at the moment of OVERLOAD (survive-at-zero-while-carrying).
 * One GE, three tags (the warmup O1 + carrier-vulnerability precedents):
 *  - State.Overloaded         -- the re-overload LOCKOUT (the intercept skips overload while present)
 *    + the announce/HUD hook.
 *  - Gameplay.MovementStopped  -- the brief stun (Lyra CMC zeroes speed natively -- the warmup precedent).
 *  - State.Carrying.Vulnerable -- amplified incoming damage during the window (the proven x1.3 -- so the
 *    overloaded player is punishable, the "vulnerable after overload" feel; ZERO new damage code).
 *
 * Duration is the StunSeconds CDO default (a BP child / cvar could tune; the GE itself carries the
 * SetByDuration). Mirror of UGE_AFL_CarrierVulnerability's TargetTags ctor.
 */
UCLASS()
class AFLCOMBAT_API UAFLGE_OverloadStun : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAFLGE_OverloadStun();
};
