// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AFLLiveOpsTypes.h"
#include "Engine/DataAsset.h"
#include "AFLPassSeasonAsset.generated.h"

/**
 * One Battle Pass season: its identity, its window, and its ladder.
 *
 * A PRIMARY DATA ASSET so live-ops can ship a season without a code change, and so the AssetManager
 * can enumerate seasons for a rotation later without loading the reward content each one points at.
 *
 * SEASON LENGTH IS ~9-13 WEEKS (LEAGUE_ADVANCEMENT §3.3: seasons align to the Battle-Pass cadence --
 * "one live-ops calendar, not two"). The window is stored as explicit dates rather than a duration
 * because a duration needs an epoch to mean anything, and the epoch is what drifts.
 *
 * ⚠ NO PRICE FIELD, DELIBERATELY. The pass is $5/month, real money (IRONICS_PRICING_SSOT §6). A
 * price on a season asset would be a second place for it to live, and a superseded ~8,000 V figure
 * already survived in a doc long enough to cause three false reconciliations. Entitlement is asked
 * of the existing conditional path; it is never authored here.
 */
UCLASS(BlueprintType)
class AFLLIVEOPS_API UAFLPassSeasonAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id, e.g. "S1". Progress records this, so renaming a shipped season orphans progress. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Season")
	FName SeasonId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Season")
	FText DisplayName;

	/** Season window, UTC. Inclusive start, exclusive end. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Season")
	FDateTime StartUtc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Season")
	FDateTime EndUtc;

	/**
	 * The ladder, index 0 .. N-1. ~100 tiers per ECONOMY_SPEC §4.
	 *
	 * Authored as an array rather than generated from a curve so a designer can break the curve
	 * deliberately at a milestone tier. ValidateSeason() enforces what must hold regardless.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Pass|Season")
	TArray<FAFLPassTier> Tiers;

	/** Tier count, for readers that do not want the whole array. */
	UFUNCTION(BlueprintPure, Category = "AFL|Pass|Season")
	int32 GetTierCount() const { return Tiers.Num(); }

	/**
	 * The tier index a given cumulative XP total sits in.
	 *
	 * Returns the HIGHEST tier whose threshold has been met, clamped to the last tier. -1 only for an
	 * empty ladder, which ValidateSeason() rejects -- so a caller seeing -1 has an unvalidated asset,
	 * not a player at "no tier".
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Pass|Season")
	int32 TierForXp(int32 Xp) const;

	/** Cumulative XP needed to enter @TierIndex. INDEX_NONE-safe: out of range returns the last. */
	UFUNCTION(BlueprintPure, Category = "AFL|Pass|Season")
	int32 XpForTier(int32 TierIndex) const;

	/** Is @NowUtc inside the season window? */
	UFUNCTION(BlueprintPure, Category = "AFL|Pass|Season")
	bool IsActiveAt(const FDateTime& NowUtc) const
	{
		return NowUtc >= StartUtc && NowUtc < EndUtc;
	}

	/**
	 * Everything that must be true of a shippable season, as a list of failures.
	 *
	 * RETURNS THE FAILURES, does not log them. A validator that only logs is one nobody can write a
	 * failing test against, and this system was told to prove each piece with arms that can fail.
	 *
	 * Checks, and why each exists:
	 *   - non-empty id / non-empty ladder      -- an unnamed or empty season is not shippable
	 *   - window ordered and non-zero          -- End <= Start makes IsActiveAt always false, which
	 *                                             reads exactly like "season not started yet"
	 *   - window within 9-13 weeks             -- the ruled cadence; a 400-week season is a typo
	 *   - tier 0 threshold is 0                -- otherwise a player starts below the ladder
	 *   - thresholds strictly increasing       -- a flat or falling curve makes TierForXp ambiguous
	 *   - every reward quantity >= 1           -- a 0 or negative quantity reaches a grant
	 *   - free track not entirely empty        -- ECONOMY_SPEC §4 promises every player a free-track
	 *                                             grant each season; an all-empty free track silently
	 *                                             breaks that promise and nothing else would catch it
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Pass|Season")
	TArray<FString> ValidateSeason() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("AFLPassSeason"), GetFName());
	}
};
