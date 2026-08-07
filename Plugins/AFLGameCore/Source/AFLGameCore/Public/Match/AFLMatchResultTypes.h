// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "AFLMatchResultTypes.generated.h"

/**
 * THE MATCH RESULT SHAPE -- `ssot/matchmaking.md` §10.1, the interface matchmaking owes the league system.
 *
 * One struct, emitted once per match, consumed by everything downstream: rating (league), settlement
 * (economy), progression, achievements, telemetry. It exists in the ALWAYS-LOADED module rather than in the
 * AFLCombat GameFeature that produces it, because the consumers outlive the producer -- a result must still be
 * settleable when the feature that generated it is not loaded, and PRO MOD deliberately does not load the
 * dismember feature at all (`ssot/character-system.md` §13.1).
 *
 * ⚠ THE FIREWALL THIS FILE EXISTS TO ENFORCE, and it is structural rather than documentary:
 *
 *     THERE IS NO STAKE AMOUNT IN THIS STRUCT. There never will be.
 *
 * §10.1: *"stake size is not a field the rating consumes. Rank moves on outcome and opponent skill only. A
 * staked win and an unstaked win of the same result are the same rating event. Emitting the stake alongside
 * the result is fine; a rating that READS it would make rank buyable."* A comment asking future code not to
 * read a field is not a firewall -- an absent field is. Escrowed entries and payouts belong to the SETTLEMENT
 * shape (§10.2), which is a separate type with a separate consumer, joined only by MatchId.
 *
 * `bRanked` and `bStaked` are INDEPENDENT booleans, never collapsed into one enum (R77). They answer different
 * questions, they are consumed by different systems, and the tier table has a row where they diverge.
 */

/** Which ruleset the match ran. Rank input differs by ruleset (`ssot/match-modes.md` §2), so it rides here. */
UENUM(BlueprintType)
enum class EAFLRuleset : uint8
{
	/** Team series, best-of-N, two finishing positions (R41). */
	MatchPlay,
	/** N-way placement (R41). */
	BattleRoyale,
};

/** Which combat ruleset ("league") the match ran under. Staked play is PRO MOD only (R86). */
UENUM(BlueprintType)
enum class EAFLLeague : uint8
{
	Haywire,
	ProMod,
};

/** The three-tier player structure (R85). The FIRST choice in the lobby, and it scopes everything below it. */
UENUM(BlueprintType)
enum class EAFLPlayTier : uint8
{
	/** No buy-in, unrated, bots permitted; progression accrues only against humans (R87). */
	LeaguePlay,
	/** Staked in Watts. Pro Mod only, rated, no bots (R85/R86). */
	WattsPlay,
	/** Staked in Volts. Pro Mod only, rated, no bots (R85/R86). */
	VoltsPlay,
};

/**
 * One participant's outcome. The identity is the RECONCILE ID -- the same key
 * `UAFLReconcileIdComponent` stashes and the matchmaker roster matches on -- so the result joins to the
 * matchmaker ticket and to PlayFab without a second identity scheme.
 */
USTRUCT(BlueprintType)
struct FAFLMatchParticipant
{
	GENERATED_BODY()

	/** PlayFabId-shaped reconcile key. EMPTY for a bot, which is why bIsBot is not inferred from it. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	FString ReconcileId;

	/** Team as assigned at match start (`Teams/AFLTeamAssignmentTypes.h`). INDEX_NONE for a free-for-all. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	int32 TeamId = INDEX_NONE;

	/**
	 * 1-based finishing position. **The one field that lets a single payout curve serve both rulesets**
	 * (`ssot/match-modes.md` §3.2): over N under BATTLE ROYALE, over 2 under MATCH PLAY.
	 *
	 * SQUAD MEMBERS DELIBERATELY SHARE A POSITION (R92) -- a squad holds one finishing position and receives
	 * one payout, split evenly across the roster. So positions are NOT unique per participant, and any
	 * consumer that assumes uniqueness is wrong about squads.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	int32 FinishingPosition = INDEX_NONE;

	/**
	 * Bots are recorded, not omitted. Progression accrues only against HUMAN opponents (R87), which a consumer
	 * can only honour if it can see which participants were bots -- dropping them here would make League Play
	 * results indistinguishable from thin human ones.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	bool bIsBot = false;
};

/**
 * The per-match result. Emitted once, on the server, at match end.
 *
 * Plain UPROPERTYs only -- this is NOT a net-serialised GAS struct, so **N8 does not apply** and no
 * NetSerialize/NetDeltaSerialize is declared. It travels server-side to the settlement/rating consumers, not
 * down a replication channel to clients.
 */
USTRUCT(BlueprintType)
struct FAFLMatchResult
{
	GENERATED_BODY()

	/** The SAME id `UAFLRoundManagerComponent::MatchId` already mints -- not a second key (§10.1, §10.2). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	FGuid MatchId;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	EAFLRuleset Ruleset = EAFLRuleset::MatchPlay;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	EAFLLeague League = EAFLLeague::ProMod;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	EAFLPlayTier Tier = EAFLPlayTier::LeaguePlay;

	/** Does this match move a RATING. Independent of bStaked (R77). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	bool bRanked = false;

	/** Does this match move a BALANCE. Independent of bRanked (R77). NO amount accompanies it -- see the
	 *  file header; amounts live in the settlement shape (§10.2). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	bool bStaked = false;

	/** Every participant, bots included, with team and finishing position. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	TArray<FAFLMatchParticipant> Participants;

	/** Winning team under MATCH PLAY. INDEX_NONE under BATTLE ROYALE, where placement is the result. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Match")
	int32 WinningTeamId = INDEX_NONE;

	/**
	 * Check the result against the rulings that constrain it, BEFORE it reaches settlement or rating.
	 *
	 * This is not defensive noise. Every rule below was decided in the R85–R96 pass, and a struct that can
	 * represent an illegal state will eventually hold one -- at which point the symptom is a wrong payout or a
	 * moved rating, both of which are far more expensive than an early rejection. Validating at the seam is
	 * the cheapest place to catch a producer bug.
	 *
	 * Returns false and fills OutError with a specific, human-readable reason (never a generic "invalid").
	 */
	AFLGAMECORE_API bool Validate(FString& OutError) const;

	/** Convenience: the number of DISTINCT finishing positions -- the `N` the payout curve is solved over.
	 *  Distinct rather than participant count precisely because squads share a position (R92). */
	AFLGAMECORE_API int32 GetPaidPositionCount() const;
};
