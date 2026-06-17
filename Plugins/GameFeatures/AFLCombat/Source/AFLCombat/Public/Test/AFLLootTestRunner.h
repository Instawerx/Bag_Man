// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "AFLLootTestRunner.generated.h"

class UWorld;

/**
 * UAFLLootTestRunner  (Loot Phase 1 -- afl.Loot.Test.Run, HOST/authority, deterministic)
 *
 * The AUTOMATED regression instrument for the generalized loot grant (UAFLLootGrantComponent) -- the
 * keystone the head/limb/caches share. Replaces the manual 4-case dismember eyeball with a one-command
 * PASS N/N verdict (the deterministic-assertion doctrine). SYNCHRONOUS: the grant path has no timed steps
 * (TryGrant + the delegates fire immediately), so unlike the Extract runner this is not a FTickableGameObject
 * -- the whole run completes inside RunInWorld.
 *
 * It drives the REAL path: a transient UAFLLootGrantComponent + the LOCAL pawn's REAL wallet as the
 * retriever, with a transient actor standing in as a "different owner" so the local pawn reads as a
 * non-owner. Asserts:
 *   T1 Anyone     -> TryGrant grants + the wallet ticks the EXACT value + IsSpent + OnLootGranted once.
 *   T2 grant-once -> a second TryGrant on the spent component is a no-op (the double-pay bug guard).
 *   T3 owner-seam -> EnemyOnly + the OWNER retrieving -> OnOwnerRetrieved fires, NO grant, NOT spent, wallet flat.
 *   T4 enemy      -> EnemyOnly + a NON-owner -> grant + wallet delta.
 * NON-DESTRUCTIVE: snapshots the wallet at start and DebugSetBalance-restores it at the end, so the run
 * leaves the player's balance exactly as it found it.
 */
UCLASS()
class AFLCOMBAT_API UAFLLootTestRunner : public UObject
{
	GENERATED_BODY()

public:
	/** afl.Loot.Test.Run entry -- HOST/authority only (grant + EarnWattsAuthority are authority ops). */
	static void RunInWorld(UWorld* World);

private:
	UFUNCTION()
	void OnOwnerRetrievedProbe(AActor* Retriever);

	UFUNCTION()
	void OnLootGrantedProbe(AActor* Retriever, int32 Value);

	void Check(bool bPass, const FString& What);

	int32 OwnerRetrievedCount = 0;
	int32 LootGrantedCount = 0;
	int32 ChecksTotal = 0;
	int32 ChecksFailed = 0;
};
