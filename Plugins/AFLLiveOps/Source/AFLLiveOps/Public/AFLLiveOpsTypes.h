// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AFLLiveOpsTypes.generated.h"

/**
 * Which track a reward sits on.
 *
 * TWO TRACKS, NOT A TIER LADDER OF ITS OWN. ECONOMY_SPEC §4 (tier structure; its PRICE is superseded
 * in full by IRONICS_PRICING_SSOT §6 and must not be read from there): ~100 tiers, free + premium,
 * unlocked by play. The free track grants a meaningful Watts pool and a baseline cosmetic to EVERY
 * player each season -- it is not a teaser, and a season with an empty free track is a data error.
 */
UENUM(BlueprintType)
enum class EAFLPassTrack : uint8
{
	Free    UMETA(DisplayName = "Free"),
	Premium UMETA(DisplayName = "Premium")
};

/**
 * What a tier hands over on one track.
 *
 * A reward is an ADDRESS plus a quantity, never a resolved asset. Catalog rows already carry
 * `EAFLAcquisition::BattlePass`, so a pass reward names a CosmeticId the catalog owns and the pass
 * never becomes a second content registry. That is the single existing hook this system was told to
 * build on, and it is the reason there is no reward-asset pointer here.
 *
 * Currency rewards use the SAME shape: Watts and Volts are addressed by id like anything else, so a
 * tier that pays currency and a tier that pays a cosmetic travel one code path. Pricing is not
 * modelled anywhere in this plugin -- the pass costs $5/month in real money and that lives in
 * IRONICS_PRICING_SSOT, not in a data asset that could drift from it.
 */
USTRUCT(BlueprintType)
struct AFLLIVEOPS_API FAFLPassReward
{
	GENERATED_BODY()

	/** Catalog id. Empty means this track gives nothing at this tier, which is legal and common. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Reward")
	FName CosmeticId;

	/**
	 * How many. Meaningful for counted things (currency, credits); 1 for a wearable.
	 *
	 * SIGNED, and validated as > 0 rather than assumed: an unsigned type would turn a mis-authored
	 * negative into a huge positive silently, and this value eventually reaches a grant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Reward", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	bool IsEmpty() const { return CosmeticId.IsNone(); }
};

/**
 * One tier of the ladder.
 *
 * XP IS CUMULATIVE-TO-ENTER, not per-tier-earned. Storing "XP required to reach this tier" makes the
 * tier lookup a single search over a monotonic array and makes a mis-authored curve visible as a
 * non-monotonic sequence. Storing per-tier deltas would push that error into a running total where
 * nothing can see it.
 */
USTRUCT(BlueprintType)
struct AFLLIVEOPS_API FAFLPassTier
{
	GENERATED_BODY()

	/** Total season XP needed to have ENTERED this tier. Tier 0 is always 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Tier", meta = (ClampMin = "0"))
	int32 XpThreshold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Tier")
	FAFLPassReward FreeReward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Tier")
	FAFLPassReward PremiumReward;
};

/**
 * A player's progress through one season.
 *
 * REPLICATED WHOLE. The three fields change together on a grant and a partial application would let
 * a client render a tier its XP has not reached.
 *
 * ⚠ SeasonId IS PART OF THE STATE, not context. Without it, progress from a previous season reads as
 * progress in the current one -- the client would show tier 47 of a season the player has not played.
 * Any read must check the season matches before trusting the numbers.
 */
USTRUCT(BlueprintType)
struct AFLLIVEOPS_API FAFLPassProgress
{
	GENERATED_BODY()

	/** Which season these numbers describe. None = no progress yet. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Progress")
	FName SeasonId;

	/** Cumulative season XP. Server-authoritative; never written from a client. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Progress")
	int32 Xp = 0;

	/**
	 * Does this player hold the premium track this season?
	 *
	 * DERIVED FROM ENTITLEMENT, MIRRORED HERE FOR REPLICATION -- never the source of truth. The pass
	 * is a $5/month real-money subscription and its entitlement lives on the proven conditional path
	 * (AwaitingActivation is fail-closed there). A bool that could be set independently of that path
	 * would be a second, weaker gate on a paid product.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Progress")
	bool bPremiumHeld = false;

	bool IsForSeason(const FName InSeasonId) const
	{
		return !SeasonId.IsNone() && SeasonId == InSeasonId;
	}
};
