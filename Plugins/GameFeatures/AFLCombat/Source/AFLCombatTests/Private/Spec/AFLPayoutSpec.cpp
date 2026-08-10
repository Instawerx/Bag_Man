// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.Payout — S2's ladder, pinned to the figures the SSOT PUBLISHES.
//
// The payout rule lives twice: authoritatively in `Bag_Man_Backend/lambda/settle-match/payout.ts`, which
// settles real balances, and here, which previews them in S2. Two implementations of one rule can drift,
// and the drift surfaces as a player being paid something other than what the panel projected.
//
// These tests are what stop that, and the mechanism matters: they do NOT compare this solver to the
// TypeScript one. They compare it to the multiples `ssot/economy-store.md` §5.2 states in prose and that
// the design docs quote independently — 1.90× at 2 positions, 8.55× at 9, 8.10× at 10, 11.66× at 18. Both
// implementations are pinned to the same THIRD thing, so neither can quietly become the definition.

#include "Misc/AutomationTest.h"
#include "Online/AFLPayoutPreview.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLPayout_PublishedMultiples, "AFL.Payout.MatchesPublishedMultiples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLPayout_PublishedMultiples::RunTest(const FString&)
{
	// The spot-check table. Entry is 1000 per position throughout so the multiples are readable directly.
	struct FCase { int32 Positions; float TopMultiple; int32 ExpectedPaid; const TCHAR* Source; };
	const FCase Cases[] = {
		{  2,  1.90f, 1, TEXT("Match Play -- two positions however many players, so ceil(0.15x2)=1") },
		{  9,  8.55f, 1, TEXT("BR Squad -- nine positions is under ten, small-field clause") },
		{ 10,  8.10f, 2, TEXT("N=10 -- where the small-field clause HANDS OFF, one paid place becoming two") },
		{ 18, 11.66f, 3, TEXT("BR_36 duo -- the handoff's worked example") },
	};

	for (const FCase& C : Cases)
	{
		const FAFLPayoutLadder L = FAFLPayoutSolver::Solve(C.Positions, 1000);
		if (!TestTrue(FString::Printf(TEXT("ladder solves at %d positions"), C.Positions), L.IsValid()))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%d positions -> %d paid (%s)"), C.Positions, C.ExpectedPaid, C.Source),
			L.PaidPlaces, C.ExpectedPaid);

		// 0.01 tolerance: the published figures are quoted to two decimals.
		TestTrue(FString::Printf(TEXT("%d positions tops at %.2fx (got %.4f)"), C.Positions, C.TopMultiple, L.Rungs[0].Multiple),
			FMath::IsNearlyEqual(L.Rungs[0].Multiple, C.TopMultiple, 0.01f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLPayout_StructuralInvariants, "AFL.Payout.LadderInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLPayout_StructuralInvariants::RunTest(const FString&)
{
	// Swept rather than spot-checked: a rule that holds at the four published sizes and breaks at 27 is
	// exactly the kind of thing a spot-check ships.
	for (int32 Positions = 2; Positions <= 36; ++Positions)
	{
		const FAFLPayoutLadder L = FAFLPayoutSolver::Solve(Positions, 1000);
		if (!L.IsValid())
		{
			AddError(FString::Printf(TEXT("no ladder at %d positions"), Positions));
			continue;
		}

		// THE POOL MUST EQUAL THE SUM OF ITS PAYOUTS. A pool that does not is the discrepancy a player
		// screenshots; the solver reconciles rounding into first place precisely so this holds.
		int32 Sum = 0;
		for (const FAFLPayoutRung& R : L.Rungs) { Sum += R.Amount; }
		TestEqual(FString::Printf(TEXT("%d positions: payouts sum to the pool"), Positions), Sum, L.Pool);

		// Monotonically decreasing: 1st always beats 2nd. A solver that inverted the ladder would still
		// sum correctly, so summing alone does not catch it.
		for (int32 i = 1; i < L.Rungs.Num(); ++i)
		{
			TestTrue(FString::Printf(TEXT("%d positions: place %d pays less than place %d"), Positions, i + 1, i),
				L.Rungs[i].Multiple < L.Rungs[i - 1].Multiple);
		}

		// MIN CASH IS AN INPUT, FIXED AT 1.40x -- and it is the last paid place, never an outcome of the
		// solve. Only meaningful where more than one place is paid; at p == 1 first place IS min cash.
		const FAFLPayoutRung& Last = L.Rungs.Last();
		TestTrue(FString::Printf(TEXT("%d positions: last paid place is marked min cash"), Positions), Last.bIsMinCash);
		if (L.PaidPlaces > 1)
		{
			TestTrue(FString::Printf(TEXT("%d positions: min cash is 1.40x (got %.4f)"), Positions, Last.Multiple),
				FMath::IsNearlyEqual(Last.Multiple, FAFLPayoutSolver::MinCash, 0.01f));
		}

		// The small-field clause, both sides of the hand-off.
		TestEqual(FString::Printf(TEXT("%d positions: paid places"), Positions),
			L.PaidPlaces, Positions < 10 ? 1 : FMath::CeilToInt(0.15f * Positions));
		TestEqual(FString::Printf(TEXT("%d positions: winner-takes-all iff one paid place"), Positions),
			L.bWinnerTakesAll, L.PaidPlaces == 1);
	}

	// A malformed field yields NO ladder rather than a one-rung one -- "fewer than two positions" is a
	// broken queue, not a winner-take-all queue, and the panel must be able to tell.
	TestFalse(TEXT("1 position is not a contest"), FAFLPayoutSolver::Solve(1, 1000).IsValid());
	TestFalse(TEXT("a zero entry has no ladder"), FAFLPayoutSolver::Solve(18, 0).IsValid());
	return true;
}
