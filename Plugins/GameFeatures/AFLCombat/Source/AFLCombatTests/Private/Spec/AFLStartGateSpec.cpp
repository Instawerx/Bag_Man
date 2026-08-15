// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.Match.StartGate — the arrival gate, proved as a decision table.
//
// ══ THE DEFECT THIS FILE EXISTS FOR, MEASURED 2026-08-14 ════════════════════════════════════════════════
//
// The gate was `HasPayload() && CountHumanParticipants() > 0`. The payload was read as a BOOL and the count
// came from PlayerArray, and nothing compared the two — so the FIRST rostered human to arrive released it.
//
// In the BR_9 gate run the two clients' LoadMap calls were 1.9 seconds apart (02:50:00.659, 02:50:02.578).
// Playing began at 02:50:01.811, between them. The second player joined at 02:50:03.057, was seated on AFL
// team 1 by a roster mapping that was entirely correct, and then: never spawned (AAFLGameMode::
// ControllerCanRestart denies a pawn once the BR match has started), never bound a death delegate, never
// entered TotalParticipants, and finished the match as neither a survivor nor a placement. participants=35
// for a 36-player field, and nothing in the run complained.
//
// ⚠ THE GATE IS PROVED HERE AND NOT IN PIE ON PURPOSE. The failure was a 1.24-second race. Reproducing it
// live means landing two clients inside a window narrower than a frame budget, and PIE cannot produce it at
// all — PIE has no payload, so the gate never even arms. As a pure decision table every branch is reachable
// in microseconds, including the ones a live run would take a very bad day to hit.
//
// Simple-automation macros rather than BEGIN_DEFINE_SPEC — matching AFLZonePlanSpec and AFLDamageExecCalcSpec,
// whose headers record that the Spec variant does not auto-register in this DeveloperTool module.

#include "Misc/AutomationTest.h"
#include "Phases/AFLMatchPhaseComponent.h"

namespace
{
	constexpr float GRACE   = 30.f;    // ArrivalGraceSeconds default
	constexpr float NO_SHOW = 600.f;   // NoShowDeadlineSeconds default (the ready-row TTL)

	/** No usable roster, the value CountRosterMembers returns for absent-or-unparseable. Never 0. */
	constexpr int32 NO_ROSTER = INDEX_NONE;

	/** Nobody has arrived yet, so the grace has not started. */
	constexpr float NOT_ARRIVED = -1.f;

	/**
	 * THE RULE THAT SHIPPED, kept executable so the known-bad is watched by the suite rather than remembered
	 * from a commit message. Every test below that pairs it with the real gate is asserting the difference.
	 */
	bool OldRule(bool bHasPayload, int32 PresentHumanCount)
	{
		return bHasPayload && PresentHumanCount > 0;
	}
}


// ── THE GATE RUN ITSELF ─────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLStartGate_OneHereOneTravelling,
	"AFL.Match.StartGate.OneRosteredStillTravelling_Holds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLStartGate_OneHereOneTravelling::RunTest(const FString&)
{
	// The exact shape of 02:50:01.811: payload landed, roster of 2, one standing here, one still loading,
	// 11 seconds into the hold, the present player having arrived a moment ago.
	const EAFLStartGateDecision Decision = UAFLMatchPhaseComponent::EvaluateStartGate(
		/*bHasPayload=*/true, /*RosterHumanCount=*/2, /*PresentHumanCount=*/1, /*AbsentRosteredCount=*/1,
		/*HeldSeconds=*/11.f, /*SecondsSinceFirstArrival=*/0.3f, GRACE, NO_SHOW);

	// ⚠ THE KNOWN-BAD, ASSERTED. The rule that shipped opens here — that is the whole defect, and this line
	// is what stops it coming back as a "simplification".
	TestTrue(TEXT("KNOWN-BAD: the old rule OPENS with a rostered player still travelling"),
		OldRule(/*bHasPayload=*/true, /*PresentHumanCount=*/1));

	// And the gate holds.
	TestEqual(TEXT("the arrival gate HOLDS for the player who has not arrived"),
		Decision, EAFLStartGateDecision::Hold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLStartGate_BothArrived,
	"AFL.Match.StartGate.AllRosteredPresent_Opens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLStartGate_BothArrived::RunTest(const FString&)
{
	// 1.9s later, in the real run. Nothing absent -> open immediately, no grace waited.
	TestEqual(TEXT("opens the moment the last rostered player lands"),
		UAFLMatchPhaseComponent::EvaluateStartGate(true, 2, 2, 0, 12.9f, 2.2f, GRACE, NO_SHOW),
		EAFLStartGateDecision::OpenAllPresent);

	// A full nine-player BR field, the bracket this was found in.
	TestEqual(TEXT("nine of nine opens"),
		UAFLMatchPhaseComponent::EvaluateStartGate(true, 9, 9, 0, 40.f, 8.f, GRACE, NO_SHOW),
		EAFLStartGateDecision::OpenAllPresent);
	return true;
}


// ── THE GRACE ───────────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLStartGate_GraceExpiry,
	"AFL.Match.StartGate.GraceExpiry_StartsShortHanded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLStartGate_GraceExpiry::RunTest(const FString&)
{
	// One short of nine, grace exhausted. The match starts; the absentee is simply not a participant.
	TestEqual(TEXT("grace expired with one missing -> start short-handed"),
		UAFLMatchPhaseComponent::EvaluateStartGate(true, 9, 8, 1, 45.f, GRACE, GRACE, NO_SHOW),
		EAFLStartGateDecision::OpenGraceExpired);

	// One tick before the boundary it is still holding — the gate does not round in the player's disfavour.
	TestEqual(TEXT("one second before the grace, still holding"),
		UAFLMatchPhaseComponent::EvaluateStartGate(true, 9, 8, 1, 44.f, GRACE - 1.f, GRACE, NO_SHOW),
		EAFLStartGateDecision::Hold);

	// ⚠ THE GRACE RUNS FROM THE FIRST ARRIVAL, NOT FROM THE HOLD. A server that has been holding for ten
	// minutes waiting for ANYONE must still give the first arrival their full grace — otherwise the second
	// player is punished for the first player's slow travel, which is the original bug with extra steps.
	TestEqual(TEXT("held for 400s but the first human landed 2s ago -> HOLD, the grace is theirs"),
		UAFLMatchPhaseComponent::EvaluateStartGate(true, 2, 1, 1, 400.f, 2.f, GRACE, NO_SHOW),
		EAFLStartGateDecision::Hold);
	return true;
}


// ── DEGRADATION: NO ROSTER TO CHECK AGAINST ─────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLStartGate_NoRoster,
	"AFL.Match.StartGate.NoRoster_DegradesToOldRule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLStartGate_NoRoster::RunTest(const FString&)
{
	// REQUIRED, NOT A COURTESY. PIE, offline, and an unparseable payload all land here. A match with no
	// roster has no arrival to wait for, so holding would hang it until the no-show bound killed it — the gate
	// would have turned every local session into a ten-minute stall.
	TestEqual(TEXT("no roster + a human present -> open, exactly the old rule"),
		UAFLMatchPhaseComponent::EvaluateStartGate(true, NO_ROSTER, 1, 0, 5.f, 1.f, GRACE, NO_SHOW),
		EAFLStartGateDecision::OpenNoRoster);

	// The old rule agrees here, and that agreement IS the contract: nothing about a rosterless match changed.
	TestTrue(TEXT("and the old rule agrees -- rosterless behaviour is byte-for-byte unchanged"),
		OldRule(/*bHasPayload=*/true, /*PresentHumanCount=*/1));

	// INDEX_NONE is "no roster", never "a roster of nobody" -- with no payload it must still hold.
	TestEqual(TEXT("no roster and nobody here -> still holds"),
		UAFLMatchPhaseComponent::EvaluateStartGate(true, NO_ROSTER, 0, 0, 5.f, NOT_ARRIVED, GRACE, NO_SHOW),
		EAFLStartGateDecision::Hold);
	return true;
}


// ── THE NO-SHOW BOUND, AND ITS SECOND ENDING ────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLStartGate_NoShowBound,
	"AFL.Match.StartGate.NoShowBound_BothEndings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLStartGate_NoShowBound::RunTest(const FString&)
{
	// THE ORIGINAL ENDING, UNTOUCHED. Nobody ever came; the process must stop holding a session.
	TestEqual(TEXT("at the bound with ZERO humans -> NO SHOW cancel"),
		UAFLMatchPhaseComponent::EvaluateStartGate(true, 9, 0, 9, NO_SHOW, NOT_ARRIVED, GRACE, NO_SHOW),
		EAFLStartGateDecision::CancelNoShow);

	// No payload at the bound is the same terminal answer -- and before the bound it holds rather than
	// releasing, because under GameLift a server with no session refuses every connection at PreLogin.
	TestEqual(TEXT("no payload at the bound -> NO SHOW cancel"),
		UAFLMatchPhaseComponent::EvaluateStartGate(false, NO_ROSTER, 0, 0, NO_SHOW, NOT_ARRIVED, GRACE, NO_SHOW),
		EAFLStartGateDecision::CancelNoShow);
	TestEqual(TEXT("no payload before the bound -> hold, never open"),
		UAFLMatchPhaseComponent::EvaluateStartGate(false, NO_ROSTER, 0, 0, 120.f, NOT_ARRIVED, GRACE, NO_SHOW),
		EAFLStartGateDecision::Hold);

	// THE SECOND ENDING. Somebody arrived so late that the bound beats their own grace (an arrival past 570s
	// with a 30s grace). One human present is a match worth starting -- cancelling a match somebody is sitting
	// in is strictly worse than starting it short-handed.
	TestEqual(TEXT("at the bound with a human present -> START, not cancel"),
		UAFLMatchPhaseComponent::EvaluateStartGate(true, 9, 1, 8, NO_SHOW, 5.f, GRACE, NO_SHOW),
		EAFLStartGateDecision::OpenGraceExpired);
	return true;
}
