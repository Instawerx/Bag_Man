// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "AFLGE_InExtractionZone.generated.h"

/**
 * UAFLGE_InExtractionZone  (extraction cycle 1 -- the zone's tag dispenser)
 *
 * Infinite-duration GE granting State.InExtractionZone while a pawn stands inside an
 * AAFLExtractionZone. Applied per-pawn on overlap-begin and removed BY HANDLE on overlap-end /
 * zone EndPlay (the UGE_AFL_CarrierVulnerability apply/remove-pair shape, verbatim). The tag is
 * the entire zone->ability contract: UAFLAG_Extract requires it to activate and self-cancels
 * when it drops -- the zone never knows who is channeling (Contested layers a SECOND blocked-tag
 * GE later; this one never changes).
 */
UCLASS()
class AFLCOMBAT_API UAFLGE_InExtractionZone : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAFLGE_InExtractionZone();
};
