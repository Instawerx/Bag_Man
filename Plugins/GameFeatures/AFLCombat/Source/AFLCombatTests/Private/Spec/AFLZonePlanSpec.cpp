// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.Zone.Plan — the BR shrinking-zone plan, proved as MATH.
//
// IRONICS_BR_ZONE_SYSTEM.md §8 names determinism under net as the HIGHEST RISK in the whole system, because
// the primary users are staked battle royales and tournaments: "same MatchId => same zone", and a contested
// match must be replayable exactly. §5's Z2 is the assertion that carries it.
//
// ⚠ THIS FILE NEEDS NO WORLD, NO ACTORS, NO NET, AND NO PIE, and that is the point of building the plan as a
// pure function rather than drawing each circle as it arrives. A determinism property proved against a live
// match is proved against one run of one match on one machine; proved here it is proved for every seed the
// suite cares to try, in milliseconds, in CI, before anyone loads the editor.
//
// Simple-automation macros rather than BEGIN_DEFINE_SPEC — matching AFLDamageExecCalcSpec, whose header
// records that the Spec variant does not auto-register in this DeveloperTool module.

#include "Misc/AutomationTest.h"
#include "Zone/AFLZonePlan.h"

namespace
{
	/** Rules with a non-origin centre, so a bug that silently resets the centre cannot pass by coincidence. */
	FAFLZoneRules TestRules()
	{
		FAFLZoneRules R;
		R.PlayableCentre = FVector2D(12000.f, -8000.f);
		R.PlayableRadius = 17500.f;
		R.ShrinkCount = 6;
		R.FinalRadius = 3000.f;
		R.FirstHoldSeconds = 60.f;
		R.HoldSeconds = 35.f;
		R.FirstShrinkSeconds = 45.f;
		R.FinalShrinkSeconds = 20.f;
		R.FirstDamagePerSecond = 2.f;
		R.FinalDamagePerSecond = 30.f;
		R.CentreDriftFraction = 1.f;
		return R;
	}

	FGuid GuidOf(uint32 A, uint32 B, uint32 C, uint32 D) { return FGuid(A, B, C, D); }
}


// ── Z2: THE STAKING PROPERTY ────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLZonePlan_SameSeedSamePlan, "AFL.Zone.Plan.Z2_SameSeedIsIdentical",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLZonePlan_SameSeedSamePlan::RunTest(const FString&)
{
	const FAFLZoneRules Rules = TestRules();
	const int32 Seed = FAFLZonePlan::SeedFromGuid(GuidOf(0xA1B2C3D4, 0x11223344, 0x55667788, 0x99AABBCC));

	const FAFLZonePlan A = FAFLZonePlan::Build(Seed, Rules);
	const FAFLZonePlan B = FAFLZonePlan::Build(Seed, Rules);

	TestEqual(TEXT("phase count"), A.Phases.Num(), B.Phases.Num());
	for (int32 i = 0; i < A.Phases.Num(); ++i)
	{
		// Bit-exact, not nearly-equal. A replay that disagrees in the last decimal is a replay that disagrees.
		TestTrue(FString::Printf(TEXT("phase %d centre identical"), i), A.Phases[i].Centre == B.Phases[i].Centre);
		TestTrue(FString::Printf(TEXT("phase %d radius identical"), i), A.Phases[i].Radius == B.Phases[i].Radius);
		TestTrue(FString::Printf(TEXT("phase %d dps identical"), i),
			A.Phases[i].DamagePerSecond == B.Phases[i].DamagePerSecond);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLZonePlan_DifferentSeedDiffers, "AFL.Zone.Plan.Z2_DifferentSeedDiverges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLZonePlan_DifferentSeedDiffers::RunTest(const FString&)
{
	// The other half of determinism, and the one a broken seed silently passes: if every match produced the
	// SAME zone, "same MatchId => same zone" would also hold, and the mode would be trivially memorisable.
	//
	// The pair below is a PERMUTATION on purpose. It caught the original implementation, which folded the
	// four GUID words with XOR -- a commutative operation, so {1,2,3,4} and {4,3,2,1} seeded identically and
	// two distinct staked matches shared a circle sequence. Unrelated ids would have passed happily.
	const FAFLZoneRules Rules = TestRules();
	auto CentresDiffer = [&Rules](const FGuid& X, const FGuid& Y)
	{
		const FAFLZonePlan A = FAFLZonePlan::Build(FAFLZonePlan::SeedFromGuid(X), Rules);
		const FAFLZonePlan B = FAFLZonePlan::Build(FAFLZonePlan::SeedFromGuid(Y), Rules);
		for (int32 i = 0; i < FMath::Min(A.Phases.Num(), B.Phases.Num()); ++i)
		{
			if (A.Phases[i].Centre != B.Phases[i].Centre) { return true; }
		}
		return false;
	};

	TestTrue(TEXT("a REVERSED id is a different zone"), CentresDiffer(GuidOf(1, 2, 3, 4), GuidOf(4, 3, 2, 1)));
	TestTrue(TEXT("a SWAPPED PAIR is a different zone"), CentresDiffer(GuidOf(1, 2, 3, 4), GuidOf(2, 1, 3, 4)));
	TestTrue(TEXT("unrelated ids are different zones"), CentresDiffer(GuidOf(0xDEADBEEF, 1, 2, 3), GuidOf(0xFEEDFACE, 9, 8, 7)));

	// Seeds must also not collide en masse: 512 sequential ids, 512 distinct seeds.
	TSet<int32> Seeds;
	for (uint32 i = 1; i <= 512; ++i) { Seeds.Add(FAFLZonePlan::SeedFromGuid(GuidOf(i, i * 3, i * 7, i * 11))); }
	TestEqual(TEXT("512 distinct match ids give 512 distinct seeds"), Seeds.Num(), 512);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLZonePlan_SeedFoldsWholeGuid, "AFL.Zone.Plan.Z2_SeedUsesTheWholeGuid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLZonePlan_SeedFoldsWholeGuid::RunTest(const FString&)
{
	// A seed taken from one word would collide across matches differing only elsewhere — two different staked
	// matches with the same zone, which is a fairness problem before it is a replay problem.
	TestNotEqual(TEXT("B varies the seed"), FAFLZonePlan::SeedFromGuid(GuidOf(1, 0, 0, 0)), FAFLZonePlan::SeedFromGuid(GuidOf(1, 9, 0, 0)));
	TestNotEqual(TEXT("C varies the seed"), FAFLZonePlan::SeedFromGuid(GuidOf(1, 0, 0, 0)), FAFLZonePlan::SeedFromGuid(GuidOf(1, 0, 9, 0)));
	TestNotEqual(TEXT("D varies the seed"), FAFLZonePlan::SeedFromGuid(GuidOf(1, 0, 0, 0)), FAFLZonePlan::SeedFromGuid(GuidOf(1, 0, 0, 9)));
	TestNotEqual(TEXT("an all-zero fold still seeds"), FAFLZonePlan::SeedFromGuid(FGuid()), 0);
	return true;
}


// ── FAIRNESS: containment is the property that makes a shrink defensible ────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLZonePlan_CirclesAreContained, "AFL.Zone.Plan.EveryCircleIsInsideItsParent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLZonePlan_CirclesAreContained::RunTest(const FString&)
{
	// THE fairness invariant. If a child circle could poke outside its parent, a player standing safely in
	// the current zone could find themselves outside the next one having done nothing and having had no way
	// to prevent it. In a match that settles a wager that is indefensible, so it is asserted across many
	// seeds rather than one — this is exactly the kind of property that holds for the seed you happened to
	// test and fails for the seed a player happened to get.
	const FAFLZoneRules Rules = TestRules();
	for (int32 S = 1; S <= 400; ++S)
	{
		const FAFLZonePlan Plan = FAFLZonePlan::Build(S * 7919, Rules);
		for (int32 i = 1; i < Plan.Phases.Num(); ++i)
		{
			const FAFLZonePhasePlan& Parent = Plan.Phases[i - 1];
			const FAFLZonePhasePlan& Child = Plan.Phases[i];
			const float CentreDist = FVector2D::Distance(Parent.Centre, Child.Centre);
			// Fully contained <=> distance between centres + child radius <= parent radius.
			if (CentreDist + Child.Radius > Parent.Radius + 0.01f)
			{
				AddError(FString::Printf(
					TEXT("seed %d phase %d escapes its parent: dist=%.2f + r=%.2f > parentR=%.2f"),
					S * 7919, i, CentreDist, Child.Radius, Parent.Radius));
				return false;
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLZonePlan_RadiiShrinkMonotonically, "AFL.Zone.Plan.RadiiOnlyEverShrink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLZonePlan_RadiiShrinkMonotonically::RunTest(const FString&)
{
	const FAFLZonePlan Plan = FAFLZonePlan::Build(12345, TestRules());
	for (int32 i = 1; i < Plan.Phases.Num(); ++i)
	{
		TestTrue(FString::Printf(TEXT("phase %d is smaller than %d"), i, i - 1),
			Plan.Phases[i].Radius < Plan.Phases[i - 1].Radius);
	}
	TestEqual(TEXT("opens at the playable radius"), Plan.Phases[0].Radius, TestRules().PlayableRadius);
	TestTrue(TEXT("ends at the final radius"),
		FMath::IsNearlyEqual(Plan.Phases.Last().Radius, TestRules().FinalRadius, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLZonePlan_FinalCircleIsNotAPoint, "AFL.Zone.Plan.FinalCircleIsFightable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLZonePlan_FinalCircleIsNotAPoint::RunTest(const FString&)
{
	// §7 decision 3. Shrinking to a point makes the ending a coin flip no skill survives — the one outcome a
	// WAGERED match cannot ship. The final circle must remain an arena you can actually fight in.
	const FAFLZonePlan Plan = FAFLZonePlan::Build(999, TestRules());
	TestTrue(TEXT("final radius is a fightable footprint"), Plan.Phases.Last().Radius > 100.f);
	return true;
}


// ── PACING: the numbers a player feels ──────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLZonePlan_PressureRamps, "AFL.Zone.Plan.PressureRampsAcrossThePhases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLZonePlan_PressureRamps::RunTest(const FString&)
{
	const FAFLZonePlan Plan = FAFLZonePlan::Build(4242, TestRules());
	for (int32 i = 2; i < Plan.Phases.Num(); ++i)
	{
		TestTrue(FString::Printf(TEXT("phase %d hurts at least as much as %d"), i, i - 1),
			Plan.Phases[i].DamagePerSecond >= Plan.Phases[i - 1].DamagePerSecond);
		TestTrue(FString::Printf(TEXT("phase %d closes no slower than %d"), i, i - 1),
			Plan.Phases[i].ShrinkSeconds <= Plan.Phases[i - 1].ShrinkSeconds + KINDA_SMALL_NUMBER);
	}
	// The opening circle has no outside, so nothing may be damaged during it.
	TestEqual(TEXT("opening circle deals no damage"), Plan.Phases[0].DamagePerSecond, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLZonePlan_OpeningIsGenerous, "AFL.Zone.Plan.FirstHoldIsTheLongest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLZonePlan_OpeningIsGenerous::RunTest(const FString&)
{
	// A match opens with people scattered across the whole map; the first rotation is a journey, not a
	// reposition. If this ever inverts, the opening circle is punishing arrival rather than shaping it.
	const FAFLZonePlan Plan = FAFLZonePlan::Build(77, TestRules());
	TestTrue(TEXT("first hold is the longest"), Plan.Phases[1].HoldSeconds >= Plan.Phases[2].HoldSeconds);
	TestTrue(TEXT("a full match is a sensible length"),
		Plan.TotalSeconds() > 120.f && Plan.TotalSeconds() < 1800.f);
	return true;
}


// ── CONFIG ABUSE: a bad asset must degrade, never explode ──────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLZonePlan_SurvivesHostileConfig, "AFL.Zone.Plan.HostileConfigDegradesSafely",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLZonePlan_SurvivesHostileConfig::RunTest(const FString&)
{
	// The config is designer-authored data, so it WILL at some point contain a typo. Every one of these
	// should produce a dull, playable plan rather than a crash, a NaN, or a zone that grows.
	{
		FAFLZoneRules R = TestRules();
		R.FinalRadius = 99999.f;                       // larger than the map
		const FAFLZonePlan P = FAFLZonePlan::Build(5, R);
		TestTrue(TEXT("final radius clamped to the playable radius"), P.Phases.Last().Radius <= R.PlayableRadius + 1.f);
		for (int32 i = 1; i < P.Phases.Num(); ++i)
		{
			TestTrue(TEXT("never grows"), P.Phases[i].Radius <= P.Phases[i - 1].Radius + 0.01f);
		}
	}
	{
		FAFLZoneRules R = TestRules();
		R.ShrinkCount = 0;                             // no shrinks authored
		const FAFLZonePlan P = FAFLZonePlan::Build(5, R);
		TestTrue(TEXT("clamped to at least one shrink, so the plan is still valid"), P.IsValid());
	}
	{
		FAFLZoneRules R = TestRules();
		R.CentreDriftFraction = 0.f;                   // concentric
		const FAFLZonePlan P = FAFLZonePlan::Build(5, R);
		for (const FAFLZonePhasePlan& Phase : P.Phases)
		{
			TestTrue(TEXT("zero drift keeps every circle concentric"),
				FVector2D::Distance(Phase.Centre, R.PlayableCentre) < 0.01f);
		}
	}
	return true;
}
