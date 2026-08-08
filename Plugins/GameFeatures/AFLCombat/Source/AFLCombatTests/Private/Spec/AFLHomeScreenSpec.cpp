// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.Home — the R98 split, proved as a RULE rather than a wiring.
//
// R98 is "NOT NEGOTIABLE (operator, 2026-08-07)" and its sharpest clause is the one a screen can break by
// accident: A LEAGUE PLAY PLAYER NEVER PICKS A STAKE AMOUNT. League play is where Watts are EARNED, staked
// play is where they are RISKED, and most players are on the free side — the entire purpose of the split is
// that the majority are never routed through a wagering surface.
//
// That clause is why UAFLW_HomeScreen exists in C++ at all. A widget graph can be rewired by anyone; a
// static predicate can be held by this file. These tests need no world, no widget tree, and no PIE, which
// means the rule is checked in CI before an editor is ever opened.
//
// Simple-automation macros rather than BEGIN_DEFINE_SPEC — matching AFLZonePlanSpec and
// AFLDamageExecCalcSpec, whose header records that the Spec variant does not auto-register in this module.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UI/AFLW_HomeScreen.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLHome_LeagueRefusesAnyStake, "AFL.Home.R98_LeagueRouteCarriesNoStake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLHome_LeagueRefusesAnyStake::RunTest(const FString&)
{
	// Zero is the ONLY legal stake on the league route, because league has no buy-in to pick.
	TestTrue(TEXT("league accepts a zero stake"),
		UAFLW_HomeScreen::IsStakeLegalForDoor(EAFLHomeDoor::League, 0));

	// Every non-zero value must be refused. Sweep rather than spot-check: a bug that admits exactly one
	// magnitude (an off-by-one bound, a truncating cast) passes a single-value assertion and ships.
	const int64 Illegal[] = { 1, 2, 5, 10, 25, 100, 1000, 100000, TNumericLimits<int64>::Max() };
	for (const int64 Amount : Illegal)
	{
		TestFalse(FString::Printf(TEXT("league refuses a stake of %lld"), Amount),
			UAFLW_HomeScreen::IsStakeLegalForDoor(EAFLHomeDoor::League, Amount));
	}

	// Negative is refused on BOTH doors — it is not a "free" league entry, it is a malformed one, and a
	// route that treats -1 as "no stake" would let a sign error open the free door with a wagering payload.
	TestFalse(TEXT("league refuses a negative stake"),
		UAFLW_HomeScreen::IsStakeLegalForDoor(EAFLHomeDoor::League, -1));
	TestFalse(TEXT("staked refuses a negative stake"),
		UAFLW_HomeScreen::IsStakeLegalForDoor(EAFLHomeDoor::Staked, -1));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLHome_StakedAcceptsAStake, "AFL.Home.R98_StakedRouteAcceptsAStake",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLHome_StakedAcceptsAStake::RunTest(const FString&)
{
	// The other half of the rule, and the one a too-strict predicate silently breaks: if EVERY door refused
	// every stake, the league assertion above would also pass — and staked play would be unenterable.
	const int64 Legal[] = { 0, 1, 50, 500, 25000 };
	for (const int64 Amount : Legal)
	{
		TestTrue(FString::Printf(TEXT("staked accepts a stake of %lld"), Amount),
			UAFLW_HomeScreen::IsStakeLegalForDoor(EAFLHomeDoor::Staked, Amount));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLHome_LeagueIsAlwaysOpen, "AFL.Home.R98_LeagueDoorIsNeverGated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLHome_LeagueIsAlwaysOpen::RunTest(const FString&)
{
	// UAFLW_HomeScreen is UCLASS(Abstract) -- it is a chassis a WBP reparents to, so NewObject on it trips
	// UObjectGlobals' abstract-class ensure. The CDO is the right handle and is sufficient: IsDoorAvailable
	// reads only bStakedPlayAvailable and touches no widget tree.
	UAFLW_HomeScreen* Home = GetMutableDefault<UAFLW_HomeScreen>();
	if (!TestNotNull(TEXT("home screen CDO resolves"), Home))
	{
		return false;
	}

	// Restore afterwards -- this is the shared CDO, and a later test reading a flag this one flipped would
	// be a genuinely nasty order-dependent failure.
	const bool bOriginal = Home->bStakedPlayAvailable;
	ON_SCOPE_EXIT { Home->bStakedPlayAvailable = bOriginal; };

	// The shipping default is staked-closed, and league must still be open in exactly that state — that is
	// the configuration players are in today.
	Home->bStakedPlayAvailable = false;
	TestTrue(TEXT("league open while staked is closed"), Home->IsDoorAvailable(EAFLHomeDoor::League));
	TestFalse(TEXT("staked closed when unavailable"), Home->IsDoorAvailable(EAFLHomeDoor::Staked));

	Home->bStakedPlayAvailable = true;
	TestTrue(TEXT("league still open when staked opens"), Home->IsDoorAvailable(EAFLHomeDoor::League));
	TestTrue(TEXT("staked open when available"), Home->IsDoorAvailable(EAFLHomeDoor::Staked));

	return true;
}
