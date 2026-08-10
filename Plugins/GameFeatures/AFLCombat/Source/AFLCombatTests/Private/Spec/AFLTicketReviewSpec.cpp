// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.S4 — the guardrails, as rules rather than as a screen.
//
// S4 exists because of R23: showing potential winnings beside a stake drives engagement, "which is exactly
// why a stake cap relative to balance and a session loss limit belong in the design from the start". R22
// then makes the screen unskippable. Between them, the property worth testing is not "does the widget
// draw" — it is that the CLIENT'S ANSWER MATCHES THE SERVER'S, because ui-frontend.md §7's whole claim is
// that the cap a player is shown and the cap that binds are the same object:
//
//     "A rejection teaches the player the number they wanted; a visible cap frames the range they have."
//
// If these two drift, the player is shown one limit and refused against another, which is the failure the
// section is written to prevent. The server's evaluator is covered by the backend's play-limits.test.ts;
// this is the mirror, asserted against the same ordering and the same boundaries.
//
// ⚠ NO NUMBER FROM config/play-limits.json IS ASSERTED HERE. Every figure there is a placeholder pending an
// operator ruling, and pinning one would turn "undecided" into "changing this breaks the build". The
// fixtures below carry their own limits.

#include "Misc/AutomationTest.h"
#include "UI/Lobby/AFLW_TicketReview.h"

namespace
{
	/** A player with room to spare, unless a test says otherwise. */
	FAFLPlayLimit Limit(int64 Balance, int64 EntryCap, int64 Staked, int64 Ceiling)
	{
		FAFLPlayLimit L;
		L.bKnown = true;
		L.Balance = Balance;
		L.EntryCap = EntryCap;
		L.WindowStaked = Staked;
		L.WindowCeiling = Ceiling;
		L.WindowRemaining = FMath::Max<int64>(0, Ceiling - Staked);
		L.WindowLoss = 0;
		return L;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLS4_UnknownLimitsNeverPermit, "AFL.S4.UnknownLimitsAreNotNoLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLS4_UnknownLimitsNeverPermit::RunTest(const FString&)
{
	// THE FAILURE MODE THIS GUARDS. A limits fetch that failed leaves every field at its zero value, and a
	// zeroed FAFLPlayLimit is indistinguishable from "no restrictions" unless bKnown is consulted. Reading
	// it as permission would present a commit with no guardrails on screen — the skip R22 forbids, and
	// invisible to the player, who would see a normal-looking confirm.
	FAFLPlayLimit Unknown;   // bKnown defaults false
	FText Refusal;
	TestFalse(TEXT("an unfetched limit never permits an entry"),
		UAFLW_TicketReview::EvaluateEntry(Unknown, 100, Refusal));
	TestFalse(TEXT("and it says something rather than refusing silently"), Refusal.IsEmpty());

	// Not even a zero stake, which is the case that would slip through a naive `Stake > Cap` check.
	TestFalse(TEXT("not even for a zero stake"),
		UAFLW_TicketReview::EvaluateEntry(Unknown, 0, Refusal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLS4_EntryCapBoundary, "AFL.S4.EntryCapBindsAtTheBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLS4_EntryCapBoundary::RunTest(const FString&)
{
	const FAFLPlayLimit L = Limit(/*Balance*/ 10000, /*Cap*/ 1000, /*Staked*/ 0, /*Ceiling*/ 40000);
	FText Refusal;

	// Inclusive, and it must match the server's `stake > entryCap`. An off-by-one here means the client
	// disables a control the server would have accepted, or offers one it will refuse.
	TestTrue (TEXT("under the cap"),   UAFLW_TicketReview::EvaluateEntry(L, 999,  Refusal));
	TestTrue (TEXT("exactly the cap"), UAFLW_TicketReview::EvaluateEntry(L, 1000, Refusal));
	TestFalse(TEXT("one over the cap"),UAFLW_TicketReview::EvaluateEntry(L, 1001, Refusal));

	// The refusal names the number. §7: a cap the player cannot see is the thing being avoided, so a
	// refusal that does not state it is the same defect one step later.
	//
	// GROUPED, and asserted that way on purpose rather than loosened to match either spelling. Every figure
	// on this screen is money, and `12480` is a different number at a glance than `12,480` -- misreading a
	// stake by an order of magnitude is precisely the mistake a guardrail screen exists to prevent. If the
	// grouping is ever dropped this should fail.
	TestTrue(TEXT("the refusal states the cap, grouped"), Refusal.ToString().Contains(TEXT("1,000")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLS4_WindowCeilingBoundary, "AFL.S4.WindowCeilingCountsTheEntryItself",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLS4_WindowCeilingBoundary::RunTest(const FString&)
{
	// Already 39,500 into a 40,000 ceiling.
	const FAFLPlayLimit L = Limit(/*Balance*/ 1000000, /*Cap*/ 100000, /*Staked*/ 39500, /*Ceiling*/ 40000);
	FText Refusal;

	// The PENDING entry counts. A meter that only reflected history would let a player cross the ceiling
	// with the very entry they are looking at, which is the opposite of "legible before it binds".
	TestTrue (TEXT("500 lands exactly on the ceiling"), UAFLW_TicketReview::EvaluateEntry(L, 500, Refusal));
	TestFalse(TEXT("501 crosses it"),                   UAFLW_TicketReview::EvaluateEntry(L, 501, Refusal));
	TestTrue(TEXT("the refusal states what is left"), Refusal.ToString().Contains(TEXT("500")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLS4_RefusalOrdering, "AFL.S4.RefusalNamesTheRecoverableLimitFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLS4_RefusalOrdering::RunTest(const FString&)
{
	// Breaches BOTH: 900 is over a 100 cap, and would also cross the ceiling.
	const FAFLPlayLimit L = Limit(/*Balance*/ 1000, /*Cap*/ 100, /*Staked*/ 39900, /*Ceiling*/ 40000);
	FText Refusal;
	TestFalse(TEXT("refused"), UAFLW_TicketReview::EvaluateEntry(L, 900, Refusal));

	// Same ordering as the server's evaluator. The cap is the one a player can act on RIGHT NOW by staking
	// less; the ceiling means come back later. Naming the recoverable one is the more useful sentence, and
	// the two sides must agree on which they name or the screen and the 409 will tell different stories.
	TestTrue(TEXT("names the entry cap, not the period limit"),
		Refusal.ToString().Contains(TEXT("single entry")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLS4_BalanceIsCheckedToo, "AFL.S4.AnUnaffordableEntryIsRefusedBeforeTheCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLS4_BalanceIsCheckedToo::RunTest(const FString&)
{
	// A cap can exceed a balance when the balance has moved since the cap was computed. Affordability is
	// the more basic fact and the clearer sentence, so it is reported first.
	FAFLPlayLimit L = Limit(/*Balance*/ 50, /*Cap*/ 1000, /*Staked*/ 0, /*Ceiling*/ 40000);
	FText Refusal;
	TestFalse(TEXT("cannot stake more than is held"), UAFLW_TicketReview::EvaluateEntry(L, 100, Refusal));
	TestTrue(TEXT("and says so in balance terms"), Refusal.ToString().Contains(TEXT("balance")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLS4_LowBalanceLocksOutStakedPlay, "AFL.S4.ACapBelowTheLowestPresetIsAnHonestNo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLS4_LowBalanceLocksOutStakedPlay::RunTest(const FString&)
{
	// A 500 VO balance at a 10% cap gives 50, below the 100 bottom rung — so staked play is closed to this
	// player entirely. That is the INTENDED outcome (league play is the free half and where Watts are
	// earned, R98), and it is pinned here so nobody "fixes" it with a floor that would quietly let someone
	// stake a fifth of everything they own.
	const FAFLPlayLimit L = Limit(/*Balance*/ 500, /*Cap*/ 50, /*Staked*/ 0, /*Ceiling*/ 40000);
	FText Refusal;
	TestFalse(TEXT("the lowest preset is refused"), UAFLW_TicketReview::EvaluateEntry(L, 100, Refusal));
	TestFalse(TEXT("IsEntryPossible agrees"), L.IsEntryPossible(100));
	TestTrue (TEXT("but a player with room is fine"), Limit(2000, 200, 0, 40000).IsEntryPossible(100));
	return true;
}
