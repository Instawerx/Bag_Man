// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AFLPassRewardSink.h"
#include "UObject/Object.h"
#include "AFLPassSpineTests.generated.h"

/**
 * A reward sink that RECORDS instead of granting, and can be told to refuse.
 *
 * A UOBJECT because TScriptInterface requires one -- a plain struct cannot satisfy the seam, and
 * discovering that at link time is cheaper than discovering it in a test that silently never ran.
 *
 * WHY NOT TEST AGAINST THE REAL WALLET: that would prove the wallet works, not that the CLAIM does.
 * More importantly it could not produce the case that matters most -- a grant that FAILS. bRefuse is
 * what lets the "failed grant must leave the tier unclaimed" arm exist, and that arm is the one
 * standing between a player and a permanently lost reward.
 *
 * Test-only, and compiled out of shipping with the rest of the suite.
 */
UCLASS()
class UAFLPassSpineSinkHolder : public UObject, public IAFLPassRewardSink
{
	GENERATED_BODY()

public:
	/** Every grant that got through, in order. */
	TArray<TPair<FName, int32>> Granted;

	/** When true, every grant is refused -- the failure path the real wallet cannot be asked for. */
	bool bRefuse = false;

	virtual bool GrantPassReward(const FName CosmeticId, const int32 Quantity, const TCHAR*) override
	{
		if (bRefuse)
		{
			return false;
		}
		Granted.Emplace(CosmeticId, Quantity);
		return true;
	}
};
