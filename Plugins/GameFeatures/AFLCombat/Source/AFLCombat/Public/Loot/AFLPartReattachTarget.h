// Copyright C12 AI Gaming. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AFLBodyZone.h"   // EAFLBodyZone (AFLCore) -- the SPECIFIC L/R zone the part returns to
#include "AFLPartReattachTarget.generated.h"

/**
 * The cross-module seam a SCATTERED loot pickup reads to REATTACH a part to its original owner (v7).
 *
 * When the OWNER walks over their own scattered part -- a head/limb token whose OwnerPlayerId == the collector's
 * player-id -- the part must REATTACH (the FULL RestoreZone: un-hide the bone + clear State.Decapitated + refill
 * the zone HP), NOT be re-looted. The pickup (AAFLLootCarryPickup) lives in AFLCombat; RestoreZone lives on
 * UAFLDismemberComponent in the AFLDismember GameFeature (which depends on AFLCombat) -- a direct call would be
 * circular. So the pickup reaches the gameplay-side component THROUGH THIS interface -- never a concrete type
 * (mirrors IAFLDismemberCosmeticTarget / IAFLLootable). The component IMPLEMENTS it; AFLDismember -> AFLCombat is
 * the correct dependency direction.
 *
 * Typed EAFLBodyZone (the token carries the specific L/R zone, so the part returns to the CORRECT slot).
 */
UINTERFACE(BlueprintType, MinimalAPI)
class UAFLPartReattachTarget : public UInterface
{
	GENERATED_BODY()
};

class AFLCOMBAT_API IAFLPartReattachTarget
{
	GENERATED_BODY()

public:
	/** Reattach the given zone to THIS pawn -- the full owner-reattach (un-hide + clear state + refill HP).
	 *  Called server-auth by a scattered part pickup when its original owner reclaims it (V7-2 uniform reattach). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Loot")
	void ReattachPart(EAFLBodyZone Zone);
};
