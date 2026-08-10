// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLPayoutPreview.generated.h"

/** One paid place. Every figure here is an ESTIMATE and the UI must render it as one. */
USTRUCT(BlueprintType)
struct FAFLPayoutRung
{
	GENERATED_BODY()

	/** 1-based finishing position. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	int32 Place = 0;

	/** Multiple of one position's entry. The number a player actually reasons with. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	float Multiple = 0.f;

	/** Currency amount, rounded. First place carries the rounding remainder (see the solver). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	int32 Amount = 0;

	/** Share of the pool, 0..100. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	float SharePercent = 0.f;

	/**
	 * The last paid place, which is min cash.
	 *
	 * ⚠ MARKED NEUTRAL, NEVER NEON. The lobby page is explicit: it is ringed in white because it marks a
	 * THRESHOLD, and giving it the brand's lit edge would read as "best", which is the opposite of what that
	 * row means. It is also the number a player uses to decide whether cashing is worth playing for.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	bool bIsMinCash = false;
};

/** A solved ladder for one queue. */
USTRUCT(BlueprintType)
struct FAFLPayoutLadder
{
	GENERATED_BODY()

	/** Finishing positions -- 2 for a team series however many players, N for a BR field. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	int32 Positions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	int32 PaidPlaces = 0;

	/** Estimated pool after rake. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	int32 Pool = 0;

	/** Index 0 is 1st place. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	TArray<FAFLPayoutRung> Rungs;

	/**
	 * p == 1. **NOT A MODE FLAG -- IT FALLS OUT OF THE ARITHMETIC.** Match Play has exactly two finishing
	 * positions however many players are in it, so `ceil(0.15 x 2) = 1`; BR Squad at nine positions is under
	 * the small-field threshold, so also 1. R37's structural point, arrived at by division.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Payout")
	bool bWinnerTakesAll = false;

	bool IsValid() const { return Positions >= 2 && Rungs.Num() > 0; }
};

/**
 * FAFLPayoutSolver -- S2's payout ladder, solved from the generating rule.
 *
 * ══ WHY THIS IS SOLVED AND NOT FETCHED ════════════════════════════════════════════════════════════════
 *
 * Every figure S2 shows is an ESTIMATE, and the spec requires it be rendered as one -- prefixed `~`,
 * labelled `est.` -- because *"the ladder is a generating rule solved per exact field size, and the field
 * is not final until the match starts."* That is the difference from the stake band: a band is a promise
 * about which pool you enter (R59 puts it on the server and forbids the UI re-implementing it), whereas a
 * payout preview is explicitly a projection over a field size nobody has committed to yet. There is no
 * ticket to ask about.
 *
 * ⚠ THE RULE NEVERTHELESS LIVES TWICE, AND THAT IS THE RISK WORTH NAMING. The authority is
 * `Bag_Man_Backend/lambda/settle-match/payout.ts`, which settles real money with the same constants and the
 * same bisection. Two implementations of one rule can drift, and the drift would surface as a player being
 * paid something other than what this panel projected.
 *
 * WHAT HOLDS THEM TOGETHER is not discipline, it is `AFL.Payout.*`: the tests pin this solver to the
 * multiples `ssot/economy-store.md` §5.2 PUBLISHES -- 1.90x at 2 positions, 8.55x at 9, 8.10x at 10,
 * 11.66x at 18. Those figures are quoted in the design docs and in the settlement SSOT, so both
 * implementations are pinned to the same third thing rather than to each other.
 *
 * ⚠ RAKE IS A WORKING ASSUMPTION, NOT A RULING (`economy-store.md` §15.7). Every figure this produces moves
 * if it changes, which is why it is a named constant and why the UI never presents these as final.
 */
struct AFLGAMECORE_API FAFLPayoutSolver
{
	/** §15.7 working assumption. NOT a ruling -- do not harden, do not inline at a call site. */
	static constexpr float Rake = 0.05f;

	/** M, fixed in stake units. An INPUT to the solve, not an outcome of it (`economy-store.md` §5.3). */
	static constexpr float MinCash = 1.40f;

	/**
	 * Paid places for a field.
	 *
	 * ⚠ 10 IS WHERE THE SMALL-FIELD CLAUSE HANDS OFF, NOT WHERE `ceil(0.15N)` INCREMENTS. The handoff
	 * records that confusion being corrected: below ten there is one paid place, at ten the general rule
	 * takes over with two. Getting this backwards puts the threshold in the wrong place entirely.
	 */
	static int32 PaidPlaces(int32 Positions);

	/**
	 * Solve the ladder.
	 *
	 * @param Positions          Finishing positions -- 2 for Match Play, the field size for BR.
	 * @param EntryPerPosition   What ONE position pays in. For a team mode that is stake x team size, so a
	 *                           multiple reads the same per-player as per-position under an even split.
	 */
	static FAFLPayoutLadder Solve(int32 Positions, int32 EntryPerPosition);

	/**
	 * The common ratio r solving `M(r^p - 1)/(r - 1) = B`.
	 *
	 * Bisection, not Newton: the sum is monotonically increasing in r over (1, inf), so bisection cannot
	 * miss and needs no derivative or seed. Exposed for the tests.
	 *
	 * ⚠ p == 1 IS NOT SOLVABLE and must not be asked for -- the sum degenerates to M for ANY r. Solve()
	 * handles that case before it gets here.
	 */
	static float SolveRatio(int32 PaidPlaces, float BudgetUnits);
};
