// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Match/AFLMatchResultTypes.h"

/**
 * FAFLMatchReporter -- the bridge from the in-engine match result to the three backend endpoints.
 *
 *   match START  -> POST /escrow-entry   (one per participant; DEBITS the stake)
 *   match END    -> POST /settle-match   (pays the §5.2 curve against the escrow rows)
 *                -> POST /update-rating  (moves the OpenSkill ladder)
 *
 * SERVER-ONLY, and structurally so: every call routes through UAFLOnlineSubsystem::PostServerSigned, which
 * holds the HMAC key only on a dedicated server or in the editor. A cooked client has no key, so these are
 * no-ops there rather than a security hole. **N11 holds: the client asserts nothing about a match outcome.**
 *
 * ⚠ THE BODIES ARE BUILT HERE AND NOWHERE ELSE, deliberately. Three endpoints with three near-identical
 * payloads is exactly where a field drifts -- a `teamId` that should have been omitted, a `currencyCode`
 * that must NOT appear on the rating call. Building them in one file with one set of tests makes the
 * differences visible instead of scattered across call sites.
 *
 * The rating body carries NO stake or currency field. That is not an oversight to be "fixed" later: the
 * endpoint REJECTS such a field outright, because a rating that reads stake size would make rank buyable
 * (`ssot/matchmaking.md` §10.1). If you find yourself adding one here, the firewall is being eroded.
 */
class AFLGAMECORE_API FAFLMatchReporter
{
public:
	/**
	 * Build the POST /settle-match body. Returns false if the result cannot be settled, with OutError saying
	 * why -- an unstaked result has nothing to settle, and that is a legitimate answer, not a failure.
	 *
	 * `StakeAmountPerPosition` is what ONE finishing position staked (a squad funds one unit between its
	 * members), matching the backend's uniform-stake-per-position requirement. Per-player entry is that
	 * amount divided across the position's roster.
	 */
	static bool BuildSettleBody(const FAFLMatchResult& Result, int32 StakeAmountPerPosition,
		const FString& CurrencyCode, const FString& TerminalState, FString& OutJson, FString& OutError);

	/** Build the POST /update-rating body. False (with a reason) when the match is not rated. */
	static bool BuildRatingBody(const FAFLMatchResult& Result, FString& OutJson, FString& OutError);

	/** Build one POST /escrow-entry body. One call per participant; the backend is idempotent per pair. */
	static FString BuildEscrowBody(const FGuid& MatchId, const FString& ReconcileId,
		const FString& CurrencyCode, int32 Amount);

	/**
	 * Fire the end-of-match reports. Validates first and REFUSES to send an invalid result -- a settlement
	 * built from a malformed result is worse than none, because the backend would accept it.
	 *
	 * Sends settle only when the result is staked, rating only when it is rated. Under R85 those coincide,
	 * but they are checked independently because R77 keeps them independent booleans.
	 */
	static void ReportMatchEnd(const UObject* WorldContext, const FAFLMatchResult& Result,
		int32 StakeAmountPerPosition, const FString& CurrencyCode);

	/** Escrow every participant at match start. No-op (logged) for an unstaked match. */
	static void EscrowAll(const UObject* WorldContext, const FGuid& MatchId,
		const TArray<FString>& ReconcileIds, const FString& CurrencyCode, int32 AmountPerPlayer);

	/**
	 * The match economics, read from the server's LAUNCH OPTIONS. Server-side by construction: a client
	 * cannot set these, which is what keeps N11 intact -- the client asserts nothing about what a match was
	 * worth.
	 *
	 *   ?Tier=LeaguePlay|WattsPlay|VoltsPlay   default LeaguePlay (unstaked, unrated)
	 *   ?League=Haywire|ProMod                 default ProMod
	 *   ?Stake=<int>                           what ONE finishing position stakes
	 *   ?StakeCurrency=VO|WA                   default VO
	 *
	 * ⚠ INTERIM SOURCE, AND DELIBERATELY NARROW. The right home for stake and currency is the matchmaker
	 * payload -- `GameSessionData` today carries only matchId and members, so there is nowhere else for the
	 * server to learn them. When the queue registry lands and the allocator carries stake, this reads from
	 * MatchmakerData instead and nothing else in this file changes.
	 */
	struct FMatchEconomics
	{
		EAFLPlayTier Tier = EAFLPlayTier::LeaguePlay;
		EAFLLeague League = EAFLLeague::ProMod;
		int32 StakePerPosition = 0;
		FString CurrencyCode = TEXT("VO");

		bool IsStaked() const { return Tier != EAFLPlayTier::LeaguePlay; }
	};
	static FMatchEconomics ReadEconomics(const UObject* WorldContext);

	/**
	 * Build the result for a TWO-TEAM series (MATCH PLAY) from live player state.
	 *
	 * Positions fall out of the winner: the winning team is 1, everyone else is 2 -- which is the whole of
	 * "MATCH PLAY resolves over exactly 2" and is why one payout curve serves both rulesets. BATTLE ROYALE
	 * needs its own builder, because placement there is a per-participant fact rather than a derived one.
	 *
	 * Returns false with a reason when the world cannot produce a settleable result (no players, a human
	 * with no reconcile id, and so on) rather than emitting a half-built one.
	 */
	static bool BuildTeamSeriesResult(const UObject* WorldContext, const FGuid& MatchId, int32 WinningTeamId,
		const FMatchEconomics& Economics, FAFLMatchResult& OutResult, FString& OutError);

	/** The wire spelling of the ruleset. MUST match the backend's `Ruleset` union exactly -- there are only
	 *  two ladders, and a third spelling is a rejected request, not a new ladder. */
	static FString RulesetToWire(EAFLRuleset Ruleset);

	/** The canonical match-id string. ONE spelling shared by all three endpoints and by the round manager,
	 *  because escrow, settlement and rating join on it -- a different format is a different match. */
	static FString MatchIdToWire(const FGuid& MatchId);
};
