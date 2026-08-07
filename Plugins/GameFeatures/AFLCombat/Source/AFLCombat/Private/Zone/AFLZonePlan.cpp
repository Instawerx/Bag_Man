// Copyright C12 AI Gaming. All Rights Reserved.

#include "Zone/AFLZonePlan.h"

#include "Math/RandomStream.h"

float FAFLZonePlan::TotalSeconds() const
{
	float Total = 0.f;
	for (const FAFLZonePhasePlan& P : Phases)
	{
		Total += P.HoldSeconds + P.ShrinkSeconds;
	}
	return Total;
}

int32 FAFLZonePlan::SeedFromGuid(const FGuid& MatchId)
{
	// Fold all four words rather than taking A alone: FGuid::NewGuid() does not guarantee which words carry
	// entropy, and a seed built from one of them would collide across matches that differ only elsewhere.
	//
	// ⚠ ORDER-DEPENDENT (FNV-1a), NOT XOR. A plain XOR fold is COMMUTATIVE, so any permutation of the four
	// words lands on the same seed -- {1,2,3,4} and {4,3,2,1} produce an identical zone. That is a fairness
	// bug before it is a replay bug: distinct staked matches would silently share a circle sequence, and the
	// sequence would be that much more memorisable. Caught by AFL.Zone.Plan.Z2_DifferentSeedDiverges, which
	// is why that test compares a permutation rather than two unrelated ids.
	uint32 Hash = 2166136261u;
	const uint32 Words[4] = {
		static_cast<uint32>(MatchId.A), static_cast<uint32>(MatchId.B),
		static_cast<uint32>(MatchId.C), static_cast<uint32>(MatchId.D),
	};
	for (const uint32 Word : Words)
	{
		// Mix a word a byte at a time -- FNV's avalanche is weak when fed 32 bits in one step, and two ids
		// differing in a single high byte should not produce adjacent seeds.
		for (int32 Shift = 0; Shift < 32; Shift += 8)
		{
			Hash ^= (Word >> Shift) & 0xFFu;
			Hash *= 16777619u;
		}
	}
	const uint32 Folded = Hash;

	// FRandomStream takes a signed seed and treats 0 as "not seeded" in some call paths; map it to a fixed
	// non-zero so a (vanishingly rare) all-zero fold still produces a deterministic plan rather than a
	// differently-deterministic one.
	const int32 Seed = static_cast<int32>(Folded);
	return (Seed == 0) ? 0x5BD1E995 : Seed;
}

FAFLZonePlan FAFLZonePlan::Build(int32 Seed, const FAFLZoneRules& Rules)
{
	FAFLZonePlan Plan;
	Plan.Seed = Seed;

	const int32 ShrinkCount = FMath::Clamp(Rules.ShrinkCount, 1, 16);
	const float R0 = FMath::Max(Rules.PlayableRadius, 1.f);
	// The final radius can never exceed the opening one; a config that says otherwise is a typo, and
	// clamping it is better than emitting a sequence that grows.
	const float RN = FMath::Clamp(Rules.FinalRadius, 1.f, R0);

	FRandomStream Stream(Seed);

	// -- phase 0: the opening circle, centred on the playable area. No hold, no shrink; it simply IS. --
	FAFLZonePhasePlan Opening;
	Opening.Index = 0;
	Opening.Centre = Rules.PlayableCentre;
	Opening.Radius = R0;
	Opening.HoldSeconds = 0.f;
	Opening.ShrinkSeconds = 0.f;
	Opening.DamagePerSecond = 0.f;         // nowhere is outside the opening circle
	Plan.Phases.Add(Opening);

	for (int32 i = 1; i <= ShrinkCount; ++i)
	{
		// GEOMETRIC radius schedule. A linear one takes far too much AREA out of the early circles -- area
		// goes as r^2, so halving the radius quarters the map in one step. A constant RATIO per phase means
		// each shrink removes a comparable proportion of what is left, which is what makes the arc read as
		// steady pressure rather than one brutal step followed by five polite ones.
		const float T = static_cast<float>(i) / static_cast<float>(ShrinkCount);
		const float Radius = R0 * FMath::Pow(RN / R0, T);

		const FAFLZonePhasePlan& Parent = Plan.Phases.Last();

		// CONTAINMENT BY CONSTRUCTION. The child centre lives inside a disc of radius (parentR - childR)
		// around the parent centre, so every point of the child circle is inside the parent. A player
		// standing safely in the current zone can therefore always reach the next one without having been
		// outside in between -- the fairness property that makes this defensible in a staked match.
		const float MaxOffset = FMath::Max(0.f, Parent.Radius - Radius)
			* FMath::Clamp(Rules.CentreDriftFraction, 0.f, 1.f);

		// UNIFORM-IN-DISC: sqrt on the radial roll. Without it the draw concentrates near the parent centre
		// -- which is not merely a distribution nicety here, because a predictable centre is a competitive
		// edge for whoever notices, and this mode settles wagers.
		const float Angle = Stream.FRand() * 2.f * PI;
		const float Dist = MaxOffset * FMath::Sqrt(Stream.FRand());

		FAFLZonePhasePlan Phase;
		Phase.Index = i;
		Phase.Centre = Parent.Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Dist;
		Phase.Radius = Radius;

		// The first hold is longer: a match opens with people scattered across the whole map, and the
		// opening rotation is a real journey rather than a repositioning.
		Phase.HoldSeconds = (i == 1) ? Rules.FirstHoldSeconds : Rules.HoldSeconds;

		// Shrink duration and damage both ramp linearly across the sequence: later circles close faster and
		// hurt more, so being caught out late is a decision rather than an inconvenience.
		const float Alpha = (ShrinkCount == 1) ? 1.f : static_cast<float>(i - 1) / static_cast<float>(ShrinkCount - 1);
		Phase.ShrinkSeconds = FMath::Lerp(Rules.FirstShrinkSeconds, Rules.FinalShrinkSeconds, Alpha);
		Phase.DamagePerSecond = FMath::Lerp(Rules.FirstDamagePerSecond, Rules.FinalDamagePerSecond, Alpha);

		Plan.Phases.Add(Phase);
	}

	return Plan;
}

FString FAFLZonePlan::ToLogString() const
{
	FString Out = FString::Printf(TEXT("seed=%d phases=%d total=%.1fs"), Seed, Phases.Num(), TotalSeconds());
	for (const FAFLZonePhasePlan& P : Phases)
	{
		Out += FString::Printf(
			TEXT("\n  [%d] centre=(%.0f,%.0f) r=%.0f hold=%.1fs shrink=%.1fs dps=%.1f"),
			P.Index, P.Centre.X, P.Centre.Y, P.Radius, P.HoldSeconds, P.ShrinkSeconds, P.DamagePerSecond);
	}
	return Out;
}
