// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.Career — the hub REPLAYS was ruled into, tested as routing rather than as a screen.
//
// The 2026-08-10 ruling denied REPLAYS a sixth footer slot and placed it "as a sub-tab view inside the
// parent Career layout widget rather than polluting the root-level navigation stack". Two properties
// follow from that sentence and both are worth defending:
//
//   1. Every tab in the enum has a row, so a tab cannot be added and left unwired.
//   2. `replays` is a CAREER TAB and NOT a footer destination. The same string must parse here and be
//      refused there -- if it ever parses in both, the ruling has been quietly undone by someone helpfully
//      "restoring" the missing nav item.
//
// The tab CONTENT is a WBP wiring question and is verified in a live session instead; these are the parts
// a unit test can hold with no world and no widget tree.

#include "Misc/AutomationTest.h"
#include "UI/AFLW_CareerHub.h"
#include "UI/AFLW_HomeScreen.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLCareer_EveryTabIsRouted, "AFL.Career.EveryTabIsRouted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLCareer_EveryTabIsRouted::RunTest(const FString&)
{
	// Drives the ENUM via reflection rather than a hand-written list, so a fourth tab added without a row
	// fails here rather than at a player's click. Same shape as AFL.Home.Nav_EveryTargetIsRouted, and for
	// the same reason: the footer shipped inert once already.
	const UEnum* TabEnum = StaticEnum<EAFLCareerTab>();
	if (!TestNotNull(TEXT("EAFLCareerTab is a reflected enum"), TabEnum))
	{
		return false;
	}

	const int32 Count = TabEnum->NumEnums() - 1;   // -1 for UHT's implicit _MAX
	TestEqual(TEXT("three tabs: the two career axes R10 keeps apart, plus replays"), Count, 3);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const EAFLCareerTab Tab = static_cast<EAFLCareerTab>(TabEnum->GetValueByIndex(Index));
		const FString Name = TabEnum->GetNameStringByIndex(Index);

		const FName Id = UAFLW_CareerHub::GetTabId(Tab);
		TestNotEqual(FString::Printf(TEXT("%s has a console id"), *Name), Id, FName(NAME_None));

		EAFLCareerTab RoundTripped = EAFLCareerTab::Volume;
		if (TestTrue(FString::Printf(TEXT("'%s' parses"), *Id.ToString()),
				UAFLW_CareerHub::TryParseTab(Id, RoundTripped)))
		{
			TestEqual(FString::Printf(TEXT("'%s' parses back to %s"), *Id.ToString(), *Name),
				(int32)RoundTripped, (int32)Tab);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLCareer_ReplaysIsATabNotAFooterItem, "AFL.Career.ReplaysIsATabNotAFooterItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLCareer_ReplaysIsATabNotAFooterItem::RunTest(const FString&)
{
	// THE RULING, AS AN ASSERTION ABOUT TWO ROUTERS AT ONCE. AFL.Home already checks that "replays" is not
	// a footer destination; on its own that check would still pass if REPLAYS had simply been dropped.
	// Pairing it with the positive here is what pins the actual decision: it exists, and it lives HERE.
	EAFLCareerTab Tab = EAFLCareerTab::Volume;
	TestTrue(TEXT("'replays' IS a Career tab"),
		UAFLW_CareerHub::TryParseTab(TEXT("replays"), Tab));
	TestEqual(TEXT("and it resolves to the Replays tab"), (int32)Tab, (int32)EAFLCareerTab::Replays);

	EAFLNavTarget NavTarget = EAFLNavTarget::Store;
	TestFalse(TEXT("'replays' is still NOT a footer destination"),
		UAFLW_HomeScreen::TryParseNavTarget(TEXT("replays"), NavTarget));

	// 'career' goes the other way: a footer item, never a tab inside itself.
	TestTrue(TEXT("'career' IS a footer destination"),
		UAFLW_HomeScreen::TryParseNavTarget(TEXT("career"), NavTarget));
	TestFalse(TEXT("'career' is not a tab within Career"),
		UAFLW_CareerHub::TryParseTab(TEXT("career"), Tab));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLCareer_UnknownTabIsRefused, "AFL.Career.UnknownTabIsRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLCareer_UnknownTabIsRefused::RunTest(const FString&)
{
	EAFLCareerTab Tab = EAFLCareerTab::Volume;
	// No guessing and no nearest match -- an unknown id is a caller bug, not a routing decision. 'stats'
	// and 'progress' are the two most likely well-meaning aliases for the Volume tab, and neither should
	// silently work: R10 keeps volume and rating apart, and a vague alias is how they get conflated again.
	for (const TCHAR* Id : { TEXT("stats"), TEXT("progress"), TEXT("volumes"), TEXT("") })
	{
		TestFalse(FString::Printf(TEXT("'%s' is refused"), Id),
			UAFLW_CareerHub::TryParseTab(FName(Id), Tab));
	}
	return true;
}
