// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "AFLMatchTierSource.generated.h"

UINTERFACE(MinimalAPI)
class UAFLMatchTierSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * IAFLMatchTierSource  (the always-loaded match-progress seam)
 *
 * Second interface on the same boundary IAFLRoundRestartPolicy already establishes, and for the same
 * reason: AAFLBotController lives in always-loaded AFLGameCore, the round FSM
 * (UAFLRoundManagerComponent) lives in the AFLCombat GameFeature, and the dependency may only point
 * GameFeature(implementer) -> always-loaded(interface). The controller queries this off the GameState's
 * components rather than naming the concrete type.
 *
 * WHAT IT EXPOSES, AND WHAT IT DELIBERATELY DOES NOT. Round number and score line only -- both symmetric
 * match state that either side can read. It exposes NOTHING about an individual player's performance,
 * because bot difficulty must never key off how well the human is doing. That is rubber-banding, it is
 * detectable, and with staking and MMR in the mode it is an integrity problem rather than a feel one.
 *
 * The score delta is here for a BRAKE, not a boost: difficulty may rise with round number and may only
 * ever be held back by a lopsided scoreline, symmetrically, so a blowout never becomes a comeback
 * mechanic. See UAFLBotAimTiers::BlowoutDamping (0 in v1).
 *
 * Every method is defaulted, so this is a non-breaking extension and a consumer that finds no
 * implementer simply sits at tier 0 -- the weakest, safest end of the curve.
 */
class IAFLMatchTierSource
{
	GENERATED_BODY()

public:
	/** 1-based round number of the match in progress. 0 before the first round begins. */
	virtual int32 GetCurrentRoundNumber() const { return 0; }

	/** Rounds needed to win -- the normaliser that turns a round number into a 0..1 tier. */
	virtual int32 GetRoundsToWin() const { return 1; }

	/** Absolute difference between the two teams' scores. SIGN-FREE on purpose: a brake keyed off this
	 *  behaves identically whether the player is stomping or being stomped. */
	virtual int32 GetScoreDelta() const { return 0; }
};
