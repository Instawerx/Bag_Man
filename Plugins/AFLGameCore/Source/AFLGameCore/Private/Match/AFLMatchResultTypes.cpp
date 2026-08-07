// Copyright C12 AI Gaming. All Rights Reserved.

#include "Match/AFLMatchResultTypes.h"

#include "Containers/Set.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchResultTypes)

int32 FAFLMatchResult::GetPaidPositionCount() const
{
	// DISTINCT positions, not participants -- a 4-player squad holds ONE finishing position (R92), so counting
	// participants would inflate `N` and shift the whole payout curve.
	TSet<int32> Distinct;
	for (const FAFLMatchParticipant& P : Participants)
	{
		if (P.FinishingPosition > 0)
		{
			Distinct.Add(P.FinishingPosition);
		}
	}
	return Distinct.Num();
}

bool FAFLMatchResult::Validate(FString& OutError) const
{
	OutError.Reset();

	// ---- 1. identity + population ----
	if (!MatchId.IsValid())
	{
		OutError = TEXT("MatchId is unset -- settlement and rating both key off it, so an unset id cannot be joined to anything");
		return false;
	}
	if (Participants.Num() == 0)
	{
		OutError = TEXT("no participants -- a match with no participants cannot be settled or rated");
		return false;
	}

	// ---- 2. THE TIER TABLE (R85/R86/R87). Each tier fixes ranked/staked/league/bots; a result that
	//         contradicts its own tier is a producer bug, and every one of these is silently expensive. ----
	const bool bStakedTier = (Tier == EAFLPlayTier::WattsPlay || Tier == EAFLPlayTier::VoltsPlay);

	if (bStakedTier != bStaked)
	{
		OutError = FString::Printf(
			TEXT("tier/stake mismatch: tier %s but bStaked=%s (R85 -- LEAGUE PLAY has no buy-in; WATTS/VOLTS always do)"),
			bStakedTier ? TEXT("is staked") : TEXT("is LEAGUE PLAY"), bStaked ? TEXT("true") : TEXT("false"));
		return false;
	}
	if (bStakedTier && !bRanked)
	{
		OutError = TEXT("a staked tier must be rated (R85) -- WATTS PLAY and VOLTS PLAY are both rated");
		return false;
	}
	if (Tier == EAFLPlayTier::LeaguePlay && bRanked)
	{
		OutError = TEXT("LEAGUE PLAY is UNRATED (R87) -- it is bot-fillable, so rating it would let bots move a ladder");
		return false;
	}
	if (bStaked && League != EAFLLeague::ProMod)
	{
		OutError = TEXT("staked play is PRO MOD ONLY (R86) -- Haywire is a LEAGUE PLAY feature and was cut from both staked tiers");
		return false;
	}

	// ---- 3. bots. §6.3's test is result-scoped: does this match's outcome move a balance or a rating? ----
	int32 BotCount = 0;
	for (const FAFLMatchParticipant& P : Participants)
	{
		if (P.bIsBot) { ++BotCount; }
	}
	if (BotCount > 0 && (bStaked || bRanked))
	{
		OutError = FString::Printf(
			TEXT("%d bot participant(s) in a %s%s match -- bots are barred wherever the outcome moves a balance or a rating (R85, ai-bots §6.3)"),
			BotCount, bStaked ? TEXT("staked ") : TEXT(""), bRanked ? TEXT("rated") : TEXT(""));
		return false;
	}

	// ---- 4. identity. A human with no reconcile id cannot be paid or rated; a bot must not carry one. ----
	for (const FAFLMatchParticipant& P : Participants)
	{
		if (!P.bIsBot && P.ReconcileId.IsEmpty())
		{
			OutError = TEXT("a human participant has an empty ReconcileId -- it could be neither paid nor rated");
			return false;
		}
		if (P.bIsBot && !P.ReconcileId.IsEmpty())
		{
			OutError = TEXT("a bot participant carries a ReconcileId -- that id would resolve to a real account at settlement");
			return false;
		}
	}

	// ---- 5. finishing positions: 1-based, and DENSE. A gap shifts every position below it onto the wrong
	//         rung of the payout curve, which is a real money error rather than a cosmetic one. ----
	TSet<int32> Distinct;
	for (const FAFLMatchParticipant& P : Participants)
	{
		if (P.FinishingPosition < 1)
		{
			OutError = FString::Printf(TEXT("finishing position %d is not 1-based -- positions index the payout curve directly"), P.FinishingPosition);
			return false;
		}
		Distinct.Add(P.FinishingPosition);
	}
	for (int32 Expected = 1; Expected <= Distinct.Num(); ++Expected)
	{
		if (!Distinct.Contains(Expected))
		{
			OutError = FString::Printf(
				TEXT("finishing positions are not dense: %d distinct positions but %d is missing -- a gap pays every position below it from the wrong rung"),
				Distinct.Num(), Expected);
			return false;
		}
	}

	// ---- 6. per-ruleset shape ----
	if (Ruleset == EAFLRuleset::MatchPlay)
	{
		// "the series outcome over 2" (§10.1). Two teams, therefore exactly two finishing positions.
		if (Distinct.Num() != 2)
		{
			OutError = FString::Printf(TEXT("MATCH PLAY resolved over %d finishing positions; it is a two-team series and resolves over exactly 2"), Distinct.Num());
			return false;
		}
		if (WinningTeamId == INDEX_NONE)
		{
			OutError = TEXT("MATCH PLAY has no WinningTeamId -- the series outcome IS the result");
			return false;
		}
		// The winning team must actually be the one in first.
		bool bWinnerIsFirst = false;
		for (const FAFLMatchParticipant& P : Participants)
		{
			if (P.TeamId == WinningTeamId && P.FinishingPosition == 1) { bWinnerIsFirst = true; break; }
		}
		if (!bWinnerIsFirst)
		{
			OutError = FString::Printf(TEXT("WinningTeamId %d holds no participant in finishing position 1 -- the winner and the payout order disagree"), WinningTeamId);
			return false;
		}
	}
	else // BattleRoyale
	{
		// Placement IS the result, so a winning team is meaningless and would be a second source of truth.
		if (WinningTeamId != INDEX_NONE)
		{
			OutError = TEXT("BATTLE ROYALE carries a WinningTeamId -- placement is the result, and a winner field would be a second, disagreeing source of truth");
			return false;
		}
	}

	return true;
}
