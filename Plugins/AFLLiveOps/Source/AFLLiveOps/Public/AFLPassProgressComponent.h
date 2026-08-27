// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AFLLiveOpsTypes.h"
#include "Components/ActorComponent.h"
#include "AFLPassProgressComponent.generated.h"

class UAFLPassSeasonAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAFLPassProgressChanged);

/**
 * Per-player Battle Pass progress. Lives on PlayerState, mirroring UAFLCosmeticLoadoutComponent.
 *
 * SERVER-AUTHORITATIVE, AND THE ASYMMETRY IS THE POINT. There is no client-callable "add XP": XP is
 * granted by server-side gameplay, and a Server RPC that accepted an XP amount from a client would
 * be a client-authored progression grant on a paid product. Clients READ replicated state and
 * nothing else. When claim lands (slice 2) it will be a Server RPC that names a TIER, never a
 * reward or a quantity, so the server resolves what is owed from the season asset it already holds.
 *
 * ⚠ PREMIUM IS MIRRORED, NEVER SET HERE. bPremiumHeld reflects the conditional entitlement for the
 * $5/month subscription (IRONICS_PRICING_SSOT §6), which is fail-closed on the proven path --
 * AwaitingActivation means paid-but-not-live and grants nothing. A setter on this component would be
 * a second, weaker gate on a paid product, so there isn't one.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLLIVEOPS_API UAFLPassProgressComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLPassProgressComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Fires on the owning client when replicated progress lands, and on the server after a change. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Pass")
	FAFLPassProgressChanged OnProgressChanged;

	/** The active season this component is scoring against. Server sets it; clients read it. */
	UFUNCTION(BlueprintPure, Category = "AFL|Pass")
	UAFLPassSeasonAsset* GetSeason() const { return Season; }

	/**
	 * Progress, or a zeroed struct if it belongs to a DIFFERENT season.
	 *
	 * The season check is here rather than left to callers because stale progress does not look
	 * stale: tier 47 of last season renders identically to tier 47 of this one.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Pass")
	FAFLPassProgress GetProgressForCurrentSeason() const;

	/** Current tier, or INDEX_NONE with no season / no ladder. */
	UFUNCTION(BlueprintPure, Category = "AFL|Pass")
	int32 GetCurrentTier() const;

	/** XP into the current tier, and what the next tier costs. Both 0 at the ladder's end. */
	UFUNCTION(BlueprintPure, Category = "AFL|Pass")
	void GetTierProgress(int32& OutXpIntoTier, int32& OutXpForNextTier) const;

	// ===== SERVER =================================================================================

	/** Point this component at a season and reset progress if it is a different one. Server only. */
	void ServerSetSeason(UAFLPassSeasonAsset* InSeason);

	/**
	 * Add season XP. SERVER ONLY, and there is deliberately no client entry point.
	 *
	 * Returns the tier after the grant. Refuses a non-positive amount rather than clamping it: a
	 * caller passing 0 or a negative has a bug, and silently treating it as "no-op" hides it.
	 */
	int32 ServerGrantXp(int32 Amount);

	/** Mirror the conditional entitlement. Server only; see the class note on why there is no setter. */
	void ServerSetPremiumHeld(bool bHeld);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Progress)
	FAFLPassProgress Progress;

	/**
	 * Replicated so clients can render the ladder without a second fetch.
	 *
	 * A raw pointer to a UPrimaryDataAsset replicates as an object reference; the season asset is
	 * always loaded on both ends because the plugin is Default-phase, which is the load-order reason
	 * this is not a GameFeature.
	 */
	UPROPERTY(Replicated)
	TObjectPtr<UAFLPassSeasonAsset> Season = nullptr;

	UFUNCTION()
	void OnRep_Progress();
};
