// Copyright C12 AI Gaming. All Rights Reserved.

#include "Online/AFLPayoutPreview.h"

#include "Algo/Reverse.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLPayoutPreview)

int32 FAFLPayoutSolver::PaidPlaces(int32 Positions)
{
	if (Positions < 10)
	{
		// The SMALL-FIELD CLAUSE. Under ten positions there is one paid place, full stop -- which is why
		// BR_9 is winner-take-all and why R37's 1.40x guarantee has nothing to attach to below N = 10.
		return 1;
	}
	return FMath::CeilToInt(0.15f * static_cast<float>(Positions));
}

float FAFLPayoutSolver::SolveRatio(int32 PaidPlaces, float BudgetUnits)
{
	checkf(PaidPlaces >= 2, TEXT("SolveRatio needs p >= 2; p == 1 degenerates to M for any r."));

	// Monotone increasing in r, so 200 halvings of (1, 64] resolve it far past float precision.
	const auto SumAt = [PaidPlaces](float R)
	{
		return MinCash * (FMath::Pow(R, static_cast<float>(PaidPlaces)) - 1.f) / (R - 1.f);
	};

	double Lo = 1.0 + 1e-9;
	double Hi = 64.0;
	for (int32 Iteration = 0; Iteration < 200; ++Iteration)
	{
		const double Mid = (Lo + Hi) * 0.5;
		if (SumAt(static_cast<float>(Mid)) < BudgetUnits) { Lo = Mid; }
		else                                              { Hi = Mid; }
	}
	return static_cast<float>((Lo + Hi) * 0.5);
}

FAFLPayoutLadder FAFLPayoutSolver::Solve(int32 Positions, int32 EntryPerPosition)
{
	FAFLPayoutLadder Ladder;
	if (Positions < 2 || EntryPerPosition <= 0)
	{
		// A contest needs at least two finishing positions; anything less is a malformed queue, not a
		// winner-take-all one. Returned invalid so the panel can say nothing rather than show a ladder.
		return Ladder;
	}

	Ladder.Positions = Positions;
	Ladder.PaidPlaces = PaidPlaces(Positions);
	Ladder.bWinnerTakesAll = (Ladder.PaidPlaces == 1);

	// Budget in STAKE UNITS -- what the field pays in, less rake.
	const float BudgetUnits = static_cast<float>(Positions) * (1.f - Rake);
	Ladder.Pool = FMath::RoundToInt(static_cast<float>(Positions) * static_cast<float>(EntryPerPosition) * (1.f - Rake));

	TArray<float> Multiples;
	if (Ladder.bWinnerTakesAll)
	{
		// No ratio to solve: the whole budget goes to first.
		Multiples.Add(BudgetUnits);
	}
	else
	{
		const float R = SolveRatio(Ladder.PaidPlaces, BudgetUnits);
		for (int32 Index = 0; Index < Ladder.PaidPlaces; ++Index)
		{
			Multiples.Add(MinCash * FMath::Pow(R, static_cast<float>(Index)));
		}
		// Solved ASCENDING from min cash; the ladder reads top-down.
		Algo::Reverse(Multiples);
	}

	int32 Sum = 0;
	for (int32 Index = 0; Index < Multiples.Num(); ++Index)
	{
		FAFLPayoutRung Rung;
		Rung.Place = Index + 1;
		Rung.Multiple = Multiples[Index];
		Rung.Amount = FMath::RoundToInt(Multiples[Index] * static_cast<float>(EntryPerPosition));
		Rung.bIsMinCash = (Index == Multiples.Num() - 1);
		Sum += Rung.Amount;
		Ladder.Rungs.Add(Rung);
	}

	// ⚠ THE ROUNDING REMAINDER GOES TO FIRST, and it has to go SOMEWHERE. Rounding each rung independently
	// leaves the paid amounts summing to a few units either side of the pool, and a pool that does not equal
	// the sum of its payouts is the kind of discrepancy a player screenshots. First place absorbs it because
	// it is the largest figure, so the relative distortion is smallest there.
	if (Ladder.Rungs.Num() > 0)
	{
		Ladder.Rungs[0].Amount += (Ladder.Pool - Sum);
	}

	for (FAFLPayoutRung& Rung : Ladder.Rungs)
	{
		Rung.SharePercent = (Ladder.Pool > 0)
			? (100.f * static_cast<float>(Rung.Amount) / static_cast<float>(Ladder.Pool))
			: 0.f;
	}

	return Ladder;
}
