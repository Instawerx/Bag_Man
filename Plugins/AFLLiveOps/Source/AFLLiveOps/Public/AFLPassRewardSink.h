// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "AFLPassRewardSink.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UAFLPassRewardSink : public UInterface
{
	GENERATED_BODY()
};

/**
 * Where a claimed pass reward is actually granted.
 *
 * WHY AN INTERFACE AND NOT A DIRECT CALL. UAFLWalletComponent lives in AFLCombat, a GameFeature.
 * AFLLiveOps is a Default-phase runtime plugin precisely so its season asset is loadable when
 * AssetManager scans at engine startup -- taking a dependency on a GameFeature would recreate the
 * load-order problem this plugin is shaped to avoid, and would make the pass unloadable at scan
 * time. So AFLLiveOps declares the seam and AFLCombat implements it.
 *
 * ONE GRANT PATH, NOT A FOURTH MECHANISM. The implementation is required to route into the wallet's
 * CommitMutation funnel -- the single authority commit point that adds to OwnedCosmeticIds AND
 * resolves the catalog row's CountedKey/GrantQuantity into GrantCountedEntitlement. Its own comment
 * records why both purchase entries were funnelled there: "Two call sites would drift, and the one
 * that drifted would be the shipping one." A pass claim that granted directly would be that third
 * call site.
 */
class AFLLIVEOPS_API IAFLPassRewardSink
{
	GENERATED_BODY()

public:
	/**
	 * Grant one pass reward. AUTHORITY ONLY -- the caller has already checked.
	 *
	 * @return false if the reward could not be granted, which the caller MUST treat as "not claimed".
	 *         A claim that marks a tier consumed on a failed grant is how a player loses a reward
	 *         permanently, so this returns a result rather than being void.
	 *
	 * The id addresses a CATALOG ROW. Currency rewards are catalog rows carrying a CountedKey like
	 * any other counted entitlement -- there is deliberately no separate currency parameter, because
	 * a second shape here is how the pass would grow its own grant path.
	 */
	virtual bool GrantPassReward(FName CosmeticId, int32 Quantity, const TCHAR* Reason) = 0;
};
