// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Match/AFLEscrowLedger.h"
#include "Match/AFLMatchResultTypes.h"

/**
 * FAFLMatchReporter -- the bridge from the in-engine match result to the three backend endpoints.
 *
 *   match START  -> POST /escrow-entry   (one per participant; DEBITS the stake)
 *   match END    -> POST /settle-match   (pays the §5.2 curve against the escrow rows)
 *                -> POST /update-rating  (moves the OpenSkill ladder)
 *   match CANCEL -> POST /settle-match   ('cancelled-refund' -- exact entries back, no rake, NO rating)
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
	 * Build the POST /settle-match body. Returns false if the result cannot be settled, with OutError saying
	 * why -- an unstaked result has nothing to settle, and that is a legitimate answer, not a failure.
	 *
	 * `StakeAmountPerPosition` is what ONE finishing position staked (a squad funds one unit between its
	 * members), matching the backend's uniform-stake-per-position requirement. Per-player entry is that
	 * amount divided across the position's roster.
	 */
	static bool BuildSettleBody(const FAFLMatchResult& Result, int32 StakeAmountPerPosition,
		const FString& CurrencyCode, const FString& TerminalState, FString& OutJson, FString& OutError);

	/**
	 * Build the POST /settle-match body for a match that DID NOT HAPPEN -- terminalState 'cancelled-refund'.
	 *
	 * ⚠ IT DOES NOT TAKE AN FAFLMatchResult, AND IT MUST NOT BE MADE TO. A cancelled match has no winner, and
	 * `FAFLMatchResult::Validate` rejects a MATCH PLAY result whose WinningTeamId is unset -- correctly, because
	 * for a match that was PLAYED "no winner" is a producer bug. A cancelled match is not a result with a hole
	 * in it; it is not a result at all. It is a refund, and the escrow ledger is everything a refund needs.
	 *
	 * FINISHING POSITIONS IN THIS BODY CARRY NO MEANING. `computePayouts` returns exact entries for
	 * 'cancelled-refund' without ever consulting the curve, so position is not read. Positions are synthesised
	 * (one per team, ascending by team id) for one reason only: `validateRequest` runs BEFORE terminalState is
	 * considered and requires positions to be DENSE FROM 1 with a uniform stake per position. Do not read
	 * position 1 here as "the winner of the abandoned match" -- there isn't one.
	 *
	 * THE FULL PLAN IS SENT, including entries whose escrow POST never came back OK. That is deliberate. An
	 * unconfirmed entry may have been debited anyway (a timed-out POST can still land), so dropping it would
	 * either refuse a refund the players were owed or, worse, quietly under-describe a pot the ledger holds.
	 * Sending the plan as it was intended lets `verifyAgainstEscrow` be the arbiter it was written to be, and
	 * its refusal message then names the real problem -- an escrow row that is missing -- rather than a derived
	 * one about unequal position totals.
	 */
	static bool BuildCancelBody(const FAFLEscrowLedger& Ledger, FString& OutJson, FString& OutError);

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
	 *
	 * ⚠ RETURNS WHETHER THE POT WAS DEALT WITH, AND A CALLER HOLDING ESCROW MUST READ IT. False means NOTHING
	 * WAS SENT -- the result was refused as invalid, the economy is unwired, or the settle body could not be
	 * built -- so a staked pot is still sitting in escrow and whatever backstop that caller has must still fire.
	 *
	 * It was void, and that cost a real defect: UAFLBattleRoyaleComponent latched `bEconomySettled` on
	 * BuildFieldResult SUCCESS, which is a different question. A battle royale that loses a player to a
	 * disconnect builds a result perfectly well -- every remaining PlayerArray member has a placement -- but the
	 * leaver takes their rung with them, so the positions are NOT DENSE and this function refuses them one step
	 * later. The latch was already set, the settlement never posted, and the EndPlay refund backstop was
	 * suppressed by the very flag that was supposed to guard it. One disconnect stranded the whole pot.
	 *
	 * TRUE does NOT mean the money moved. The POSTs are asynchronous and their completions are logged, not
	 * awaited. True means "validated and dispatched, do not refund behind me" -- which is exactly the question a
	 * teardown backstop is asking, and the only one that can be answered synchronously.
	 */
	static bool ReportMatchEnd(const UObject* WorldContext, const FAFLMatchResult& Result,
		int32 StakeAmountPerPosition, const FString& CurrencyCode);

	/**
	 * Fire the CANCELLED-match report: refund the escrow, and stop.
	 *
	 * ⚠ IT SENDS NO RATING, AND THAT IS THE POINT OF IT BEING A SEPARATE FUNCTION. `/settle-match` makes a
	 * cancelled match economically invisible ("taking a rake on a match that did not happen is the house
	 * charging for its own failure"); a cancelled match must be COMPETITIVELY invisible for the same reason.
	 * Nobody outplayed anybody in a lobby that emptied, and a ladder that moves on an abandoned match is a
	 * ladder you can farm by disconnecting. Routing a cancellation through ReportMatchEnd would post one --
	 * that is the mistake this signature exists to make impossible.
	 *
	 * A no-op when the ledger is unstaked (LEAGUE PLAY has no buy-in) or the economy is not wired -- both are
	 * ordinary states, not failures, and both are logged as one line rather than one per player.
	 *
	 * `ReasonText` is for the log only. It never reaches the wire: the backend's terminal state is
	 * 'cancelled-refund' whatever ended the match, and a reason field would be a second thing to disagree.
	 */
	static void ReportMatchCancelled(const UObject* WorldContext, const FAFLEscrowLedger& Ledger,
		const FString& ReasonText);

	/**
	 * PURE. The refusals a free-for-all escrow makes BEFORE anyone is debited. Returns false with a reason.
	 *
	 * Extracted so the refusals are testable without a world. The escrow itself needs a GameState to enumerate,
	 * which means the only way to prove "a staked BR containing bots is refused" through the live function is to
	 * stand up a match containing bots -- which R85 exists to prevent anyone doing. A pure core is the only
	 * place that check can be watched failing.
	 */
	static bool ValidateFreeForAllEscrow(int32 HumanCount, int32 BotCount, int32 StakePerPosition, FString& OutError);

	/**
	 * Escrow a FREE-FOR-ALL at match start -- one stake unit per PLAYER, because in a solo field each player
	 * IS a finishing position.
	 *
	 * ⚠ REPLACES EscrowAll, WHICH WAS DELETED RATHER THAN UPGRADED. That function took a flat per-player
	 * amount, returned void, posted in a loop with no all-or-nothing gate, and refused nothing -- it would
	 * happily have escrowed a staked match containing bots. It had ZERO callers and its own comment said
	 * "prefer EscrowTeamSeries". Leaving an unvalidated primitive beside a validated sibling is an invitation
	 * to call the wrong one from the staked path, so it is gone rather than deprecated.
	 *
	 * SAME CONTRACT AS EscrowTeamSeries, deliberately: all-or-nothing, bots refused (R85), returns the ledger
	 * to hold for the match lifetime, null when there is no pot. The DIFFERENCE is only the divisor -- a team
	 * splits one unit between its members, a solo player funds a whole unit alone, so there is no divisibility
	 * question here at all.
	 *
	 * ⚠ SQUAD BATTLE ROYALE (R92) IS OUT OF SCOPE AND WOULD REINTRODUCE ONE. If squads ever land, a BR position
	 * becomes a squad rather than a player, and this needs EscrowTeamSeries's `StakePerPosition % Size` check
	 * back. Meet that here rather than discover it at settlement.
	 *
	 * ⚠ LEDGER TeamId IS NOT THE RESULT TeamId, and they legitimately differ. `FAFLEscrowedEntry::TeamId` is
	 * the key BuildCancelBody GROUPS BY to synthesise finishing positions, and it REFUSES INDEX_NONE outright.
	 * So each solo entry carries its live runtime team id, which BR assigns uniquely per player. The RESULT
	 * struct carries INDEX_NONE, because that is what a free-for-all outcome MEANS. Do not "fix" one to match
	 * the other.
	 */
	static TSharedPtr<FAFLEscrowLedger> EscrowFreeForAll(const UObject* WorldContext, const FGuid& MatchId,
		const FMatchEconomics& Economics);

	/**
	 * Escrow a TWO-TEAM series at match start, per team.
	 *
	 * ⚠ NOT A FLAT PER-PLAYER AMOUNT, and the difference is load-bearing. The backend requires every
	 * finishing POSITION to have staked the same total, because the payout curve is denominated in stake
	 * units and a unit is what one position staked. A team IS a position here, so each team's members split
	 * that team's one unit: `StakePerPosition / (humans on that team)`. A flat per-player figure would make
	 * a 5v4 stake 5 units against 4 and the whole settlement would be rejected -- correctly, but confusingly.
	 *
	 * Refuses (loudly, escrowing NOTHING) rather than escrow a match that cannot later settle:
	 *   - a team whose size does not divide the stake evenly -- position totals would differ;
	 *   - a team with no human players -- nothing to fund its unit with;
	 *   - any bot in a staked match (R85 bars them), which would otherwise silently short the pot.
	 *
	 * ALL-OR-NOTHING BY DESIGN: everything is validated before the first debit, because a half-escrowed match
	 * charges some players for a match that settlement will then refuse.
	 *
	 * RETURNS THE LEDGER -- the snapshot of who was debited what. **Hold it for the lifetime of the match.**
	 * It is the only thing that can still describe the pot once the players have disconnected, which is the
	 * one case (abandonment) where a refund is both necessary and impossible to reconstruct from live state.
	 * Null when nothing was escrowed: an unstaked match, an unwired economy, or a refused validation -- in
	 * every one of those there is no pot, so there is nothing a later cancellation would need to refund.
	 *
	 * The escrow POSTs complete asynchronously and mark `bConfirmed` on the returned ledger as they land, so
	 * it is shared rather than copied.
	 */
	static TSharedPtr<FAFLEscrowLedger> EscrowTeamSeries(const UObject* WorldContext, const FGuid& MatchId,
		const FMatchEconomics& Economics);


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

	/**
	 * Build the result for a FREE-FOR-ALL field (BATTLE ROYALE) from live player state plus the placements.
	 *
	 * ⚠ THE PLACEMENTS ARE PASSED IN, NOT READ. They live on UAFLBattleRoyaleComponent, which is in the
	 * AFLCombat GameFeature; this file is in the always-loaded module and consumers outlive the producer. A
	 * reporter that reached for a GameFeature type would run the dependency backwards and would stop linking
	 * the moment PRO MOD declined to load that feature.
	 *
	 * EVERY PARTICIPANT MUST APPEAR IN THE MAP. A player in PlayerArray with no placement is refused rather
	 * than defaulted, because the alternative is a hole in the position sequence and Validate() would reject
	 * the whole result one step later with a less useful message. This is also the exact shape of the defect
	 * the arrival gate fixed -- a player seated in the match and never placed -- so it is worth naming here.
	 *
	 * TeamId is INDEX_NONE on every participant. Operator ruling: a free-for-all has no teams ECONOMICALLY,
	 * even though the runtime assigns 1..N for spawning and damage. Writing the live ids would claim a
	 * structure the mode does not have, and would make a future squad BR indistinguishable from solo.
	 *
	 * ⚠ `Departed` CARRIES THE PLAYERS WHO ARE NO LONGER IN PlayerArray, AND IT IS NOT OPTIONAL. A forfeiting
	 * player books a placement at the moment they leave and is then destroyed with their controller, so by
	 * match end PlayerArray cannot describe them and their PlayerState pointer is stale. Enumerating live
	 * players alone would drop them, and dropping them removes their rung -- leaving the ladder non-dense and
	 * the whole result unsettleable. The caller captures their identity at disconnect, while it still exists.
	 */
	static bool BuildFieldResult(const UObject* WorldContext, const FGuid& MatchId,
		const TMap<TWeakObjectPtr<APlayerState>, int32>& Placements,
		const TArray<FAFLMatchParticipant>& Departed,
		const FMatchEconomics& Economics, FAFLMatchResult& OutResult, FString& OutError);

	/** The wire spelling of the ruleset. MUST match the backend's `Ruleset` union exactly -- there are only
	 *  two ladders, and a third spelling is a rejected request, not a new ladder. */
	static FString RulesetToWire(EAFLRuleset Ruleset);

	/** The canonical match-id string. ONE spelling shared by all three endpoints and by the round manager,
	 *  because escrow, settlement and rating join on it -- a different format is a different match. */
	static FString MatchIdToWire(const FGuid& MatchId);
};
