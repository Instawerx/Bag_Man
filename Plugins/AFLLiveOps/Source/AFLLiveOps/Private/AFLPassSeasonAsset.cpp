// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLPassSeasonAsset.h"

int32 UAFLPassSeasonAsset::TierForXp(const int32 Xp) const
{
	if (Tiers.Num() == 0)
	{
		return INDEX_NONE;
	}

	// Walk DOWN and take the first threshold met. Walking up and breaking on the first UNmet
	// threshold gives the same answer only while the ladder is monotonic -- and a non-monotonic
	// ladder is exactly the authoring error ValidateSeason exists to catch, so the lookup should not
	// quietly depend on it being absent.
	for (int32 i = Tiers.Num() - 1; i >= 0; --i)
	{
		if (Xp >= Tiers[i].XpThreshold)
		{
			return i;
		}
	}

	// Below tier 0's threshold. Only reachable if tier 0 is not at 0, which ValidateSeason rejects.
	return 0;
}

int32 UAFLPassSeasonAsset::XpForTier(const int32 TierIndex) const
{
	if (Tiers.Num() == 0)
	{
		return 0;
	}
	const int32 Clamped = FMath::Clamp(TierIndex, 0, Tiers.Num() - 1);
	return Tiers[Clamped].XpThreshold;
}

TArray<FString> UAFLPassSeasonAsset::ValidateSeason() const
{
	TArray<FString> Fail;

	if (SeasonId.IsNone())
	{
		Fail.Add(TEXT("SeasonId is None -- progress records this id, so an unnamed season cannot be tracked."));
	}
	if (Tiers.Num() == 0)
	{
		Fail.Add(TEXT("Ladder is empty -- no tiers to advance through."));
		return Fail;   // every check below reads Tiers; stop rather than report noise
	}

	// --- window ---------------------------------------------------------------------------------
	if (EndUtc <= StartUtc)
	{
		Fail.Add(FString::Printf(
			TEXT("Season window is not ordered (Start=%s End=%s). IsActiveAt would be false forever, "
			     "which reads exactly like 'season has not started yet'."),
			*StartUtc.ToString(), *EndUtc.ToString()));
	}
	else
	{
		// RULED CADENCE: ~9-13 weeks (LEAGUE_ADVANCEMENT §3.3 -- one live-ops calendar, not two).
		// Bounds are inclusive and deliberately loose; this catches a typo, not a design choice.
		const double Weeks = (EndUtc - StartUtc).GetTotalDays() / 7.0;
		if (Weeks < 9.0 || Weeks > 13.0)
		{
			Fail.Add(FString::Printf(
				TEXT("Season is %.1f weeks; the ruled cadence is 9-13 (LEAGUE_ADVANCEMENT 3.3)."), Weeks));
		}
	}

	// --- ladder ---------------------------------------------------------------------------------
	if (Tiers[0].XpThreshold != 0)
	{
		Fail.Add(FString::Printf(
			TEXT("Tier 0 threshold is %d, not 0 -- a player would start below the ladder."),
			Tiers[0].XpThreshold));
	}

	for (int32 i = 1; i < Tiers.Num(); ++i)
	{
		if (Tiers[i].XpThreshold <= Tiers[i - 1].XpThreshold)
		{
			Fail.Add(FString::Printf(
				TEXT("Tier %d threshold (%d) is not greater than tier %d (%d) -- a flat or falling "
				     "curve makes the tier lookup ambiguous."),
				i, Tiers[i].XpThreshold, i - 1, Tiers[i - 1].XpThreshold));
		}
	}

	// --- rewards --------------------------------------------------------------------------------
	int32 FreeGrants = 0;
	for (int32 i = 0; i < Tiers.Num(); ++i)
	{
		const FAFLPassTier& T = Tiers[i];
		if (!T.FreeReward.IsEmpty())
		{
			++FreeGrants;
			if (T.FreeReward.Quantity < 1)
			{
				Fail.Add(FString::Printf(TEXT("Tier %d free reward quantity is %d -- must be >= 1."),
					i, T.FreeReward.Quantity));
			}
		}
		if (!T.PremiumReward.IsEmpty() && T.PremiumReward.Quantity < 1)
		{
			Fail.Add(FString::Printf(TEXT("Tier %d premium reward quantity is %d -- must be >= 1."),
				i, T.PremiumReward.Quantity));
		}
	}

	if (FreeGrants == 0)
	{
		// ECONOMY_SPEC §4: the free track grants every player a Watts pool and a baseline cosmetic
		// each season. An all-empty free track breaks that promise and nothing downstream would
		// notice -- the pass would simply appear to have no free content.
		Fail.Add(TEXT("Free track is entirely empty -- ECONOMY_SPEC 4 promises every player a "
		              "free-track grant each season."));
	}

	return Fail;
}
