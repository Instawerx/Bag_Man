// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.Venues — S8, tested as the rule it exists to enforce rather than as a screen.
//
// ui-frontend.md §8 is written mostly in the negative, and the negative is the specification:
//
//   "A venue browser attached to a queue becomes a venue picker, no matter how it is labelled... either
//    the matchmaker honours that (fragmenting the pool) or the surface disappoints."
//
// It permits exactly one exit — a deep link to the lobby — with exactly one condition: "only to the lobby
// as it is, never pre-filtered by venue, because a pre-filter is a venue choice wearing a different name."
//
// That condition is the whole test surface. Everything else about a showcase (art, layout, tiles) is
// presentation and belongs in a live session; this is the part that can be violated by a well-meaning
// edit six months from now and would look like a feature while doing it.

#include "Misc/AutomationTest.h"
#include "UI/AFLW_VenueShowcase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLVenues_ExitCarriesNothing, "AFL.Venues.TheExitCarriesNoVenue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLVenues_ExitCarriesNothing::RunTest(const FString&)
{
	// THE §8 RULE. A route out of the showcase carries no venue and no stake.
	TestTrue(TEXT("an empty exit is legal"),
		UAFLW_VenueShowcase::IsExitLegal(NAME_None, 0));

	// Any venue at all is a pre-filter, whatever it is called. Swept rather than spot-checked: a check
	// that only rejected one sentinel would pass while admitting every real map name.
	for (const TCHAR* Venue : { TEXT("L_Arena_04"), TEXT("ARCANEON"), TEXT("L_Expanse"), TEXT("Arena") })
	{
		TestFalse(FString::Printf(TEXT("carrying venue '%s' is refused"), Venue),
			UAFLW_VenueShowcase::IsExitLegal(FName(Venue), 0));
	}

	// And no stake. This surface sits behind a footer item EVERY player can reach, including the majority
	// R98 deliberately never routes through a wagering surface — a buy-in leaving here would put one on
	// the free half of the economy.
	for (const int64 Stake : { (int64)1, (int64)100, (int64)10000, TNumericLimits<int64>::Max() })
	{
		TestFalse(FString::Printf(TEXT("carrying a stake of %lld is refused"), Stake),
			UAFLW_VenueShowcase::IsExitLegal(NAME_None, Stake));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLVenues_DeepLinkCannotLeakAVenue, "AFL.Venues.DeepLinkCannotLeakAVenue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLVenues_DeepLinkCannotLeakAVenue::RunTest(const FString&)
{
	// ⚠ THE POINT IS THE SIGNATURE, NOT THE RETURN VALUE. BuildLobbyDeepLink takes no arguments, so there
	// is no selected venue in scope for it to leak. A function that COULD return a venue and merely chose
	// not to would pass this assertion today and fail the design one refactor later; this one cannot be
	// made to leak without changing its signature, which breaks this file.
	TestTrue(TEXT("the deep link carries nothing"), UAFLW_VenueShowcase::BuildLobbyDeepLink().IsNone());

	// And what it produces must satisfy the rule above — the two halves are wired to each other, so a
	// future edit cannot relax one and leave the other looking green.
	TestTrue(TEXT("and what it produces is a legal exit"),
		UAFLW_VenueShowcase::IsExitLegal(UAFLW_VenueShowcase::BuildLobbyDeepLink(), 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLVenues_ClassLabelsAreShared, "AFL.Venues.R97ClassLabelsComeFromOnePlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLVenues_ClassLabelsAreShared::RunTest(const FString&)
{
	// R97's two classes, worded once. The list and the detail panel both call this, so a venue cannot read
	// ARENA in one and something else in the other — the registry is explicit that this field going wrong
	// means "players getting the style they did not choose", and a surface that words it two ways is how
	// a reader stops trusting either.
	const FText Arena = UAFLW_VenueTile::FormatVenueClass(EAFLVenueClass::Arena);
	const FText Map   = UAFLW_VenueTile::FormatVenueClass(EAFLVenueClass::Map);

	TestEqual(TEXT("ARENA"), Arena.ToString(), FString(TEXT("ARENA")));
	TestEqual(TEXT("MAP"),   Map.ToString(),   FString(TEXT("MAP")));
	TestFalse(TEXT("the two classes are distinguishable"), Arena.EqualTo(Map));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLVenues_IncompleteEntryIsNotValid, "AFL.Venues.AHalfAuthoredVenueIsRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLVenues_IncompleteEntryIsNotValid::RunTest(const FString&)
{
	// A blank tile on a surface whose whole job is presentation reads as a broken MAP rather than a bad
	// data row, so an incomplete entry is skipped and logged instead of rendered.
	FAFLVenueEntry Empty;
	TestFalse(TEXT("an empty entry is not valid"), Empty.IsValid());

	FAFLVenueEntry NameOnly;
	NameOnly.DisplayName = FText::FromString(TEXT("ARCANEON"));
	TestFalse(TEXT("a name with no map is not valid"), NameOnly.IsValid());

	FAFLVenueEntry MapOnly;
	MapOnly.MapId = TEXT("L_Arena_04");
	TestFalse(TEXT("a map with no name is not valid"), MapOnly.IsValid());

	FAFLVenueEntry Complete;
	Complete.DisplayName = FText::FromString(TEXT("ARCANEON"));
	Complete.MapId = TEXT("L_Arena_04");
	TestTrue(TEXT("name + map is enough -- art and blurb are optional"), Complete.IsValid());
	return true;
}
