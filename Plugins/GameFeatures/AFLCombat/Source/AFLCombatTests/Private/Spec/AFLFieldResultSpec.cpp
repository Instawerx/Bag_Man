// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.Match.FieldResult — the battle royale economy terminal, proved as rules rather than as a match.
//
// ══ WHAT THIS FILE IS FOR ═══════════════════════════════════════════════════════════════════════════════
//
// Battle royale played and paid nothing: AFLBattleRoyaleComponent computed placements 1..N and never called
// the reporter, no BuildFieldResult existed, and nothing escrowed at match start either. The payout curve had
// only ever settled a 2-position result.
//
// ⚠ THE REFUSALS ARE THE POINT, AND THEY CANNOT BE WATCHED ANY OTHER WAY. "A staked field containing bots is
// refused escrow" is not something a live run can demonstrate: R85 exists precisely to stop that match being
// created, so the only way to see the refusal fire is to ask the rule directly. Same for a result carrying a
// winner, and for a hole in the position sequence -- each is a state the producers are built never to emit,
// which is exactly why the guard against it must be exercised somewhere it CAN be emitted.
//
// Simple-automation macros rather than BEGIN_DEFINE_SPEC — matching AFLZonePlanSpec and AFLStartGateSpec,
// whose headers record that the Spec variant does not auto-register in this DeveloperTool module.

#include "Misc/AutomationTest.h"
#include "Match/AFLMatchReporter.h"
#include "Match/AFLMatchResultTypes.h"

namespace
{
	/** A settleable solo field: N participants, positions 1..N, no teams, no bots. */
	FAFLMatchResult SoloField(int32 N, EAFLPlayTier Tier = EAFLPlayTier::VoltsPlay)
	{
		FAFLMatchResult R;
		R.MatchId = FGuid(0xA1B2C3D4, 0x11223344, 0x55667788, 0x99AABBCC);
		R.Ruleset = EAFLRuleset::BattleRoyale;
		R.League  = EAFLLeague::ProMod;
		R.Tier    = Tier;
		R.bStaked = (Tier != EAFLPlayTier::LeaguePlay);
		R.bRanked = (Tier != EAFLPlayTier::LeaguePlay);
		R.WinningTeamId = INDEX_NONE;
		for (int32 i = 1; i <= N; ++i)
		{
			FAFLMatchParticipant P;
			P.ReconcileId = FString::Printf(TEXT("PLAYER%04d"), i);
			P.TeamId = INDEX_NONE;          // the ruling: a free-for-all has no teams economically
			P.FinishingPosition = i;
			R.Participants.Add(P);
		}
		return R;
	}
}


// ── THE RESULT SHAPE ────────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLFieldResult_NineWayValidates,
	"AFL.Match.FieldResult.NineWayFieldValidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLFieldResult_NineWayValidates::RunTest(const FString&)
{
	FString Error;
	const FAFLMatchResult R = SoloField(9);
	TestTrue(TEXT("a nine-way solo field validates: ") + Error, R.Validate(Error));
	TestEqual(TEXT("nine DISTINCT finishing positions -- the N the payout curve solves over"), R.GetPaidPositionCount(), 9);

	// The full ladder the published brackets need.
	for (const int32 N : { 9, 20, 36 })
	{
		FString E;
		TestTrue(FString::Printf(TEXT("BR_%d validates"), N), SoloField(N).Validate(E));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLFieldResult_WinnerIsRefused,
	"AFL.Match.FieldResult.WinningTeamIdIsRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLFieldResult_WinnerIsRefused::RunTest(const FString&)
{
	// A winner field on a battle royale is a SECOND source of truth that can disagree with position 1.
	// BuildFieldResult writes INDEX_NONE; this proves the validator would catch a producer that did not.
	FAFLMatchResult R = SoloField(9);
	R.WinningTeamId = 3;

	FString Error;
	TestFalse(TEXT("a field result carrying a WinningTeamId must NOT validate"), R.Validate(Error));
	TestTrue(TEXT("and the reason names the second source of truth"), Error.Contains(TEXT("second")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLFieldResult_NonDenseIsRefused,
	"AFL.Match.FieldResult.NonDensePositionsAreRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLFieldResult_NonDenseIsRefused::RunTest(const FString&)
{
	// A GAP SHIFTS EVERY POSITION BELOW IT ONTO THE WRONG RUNG OF THE CURVE -- a real money error. This is
	// also the exact shape of the arrival-gate defect: a player seated in the match and never placed leaves a
	// hole. BuildFieldResult refuses that at source; this proves the validator is the second net under it.
	FAFLMatchResult R = SoloField(9);
	R.Participants[4].FinishingPosition = 9;   // now 5 is missing and 9 appears twice

	FString Error;
	TestFalse(TEXT("non-dense finishing positions must NOT validate"), R.Validate(Error));
	TestTrue(TEXT("and the reason names density"), Error.Contains(TEXT("dense")));

	// A zero or negative position is refused before density is even considered.
	FAFLMatchResult Z = SoloField(9);
	Z.Participants[0].FinishingPosition = 0;
	FString ZError;
	TestFalse(TEXT("a 0 finishing position must NOT validate"), Z.Validate(ZError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLFieldResult_BotsInStakedRefused,
	"AFL.Match.FieldResult.BotsInAStakedFieldAreRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLFieldResult_BotsInStakedRefused::RunTest(const FString&)
{
	// The RESULT half of R85. A bot in a staked field means the pot was short by a whole share.
	FAFLMatchResult R = SoloField(9);
	R.Participants[3].bIsBot = true;
	R.Participants[3].ReconcileId.Reset();   // a bot must carry no id, or a different rule fires first

	FString Error;
	TestFalse(TEXT("a bot in a STAKED field must NOT validate"), R.Validate(Error));
	TestTrue(TEXT("and the reason names bots"), Error.Contains(TEXT("bot")));

	// ...and the same field is perfectly legal unstaked, which is what every published BR cell is today.
	FAFLMatchResult L = SoloField(9, EAFLPlayTier::LeaguePlay);
	L.Participants[3].bIsBot = true;
	L.Participants[3].ReconcileId.Reset();
	FString LError;
	TestTrue(TEXT("a bot in a LEAGUE PLAY field is fine: ") + LError, L.Validate(LError));
	return true;
}


// ── THE ESCROW REFUSALS ─────────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLFieldResult_EscrowRefusals,
	"AFL.Match.FieldResult.FreeForAllEscrowRefusals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLFieldResult_EscrowRefusals::RunTest(const FString&)
{
	FString Error;

	// ⚠ THE ONE THE OPERATOR ASKED TO SEE FAIL. One bot and nobody is debited -- not "the bot is skipped",
	// the WHOLE match is refused, because a short pot cannot settle and half-charging players is worse than
	// not starting.
	TestFalse(TEXT("a staked free-for-all containing a bot is REFUSED"),
		FAFLMatchReporter::ValidateFreeForAllEscrow(/*Humans=*/8, /*Bots=*/1, /*Stake=*/100, Error));
	TestTrue(TEXT("and the refusal names bots and R85"), Error.Contains(TEXT("bot")) && Error.Contains(TEXT("R85")));

	// A contest needs two.
	TestFalse(TEXT("a single-human staked field is REFUSED"),
		FAFLMatchReporter::ValidateFreeForAllEscrow(1, 0, 100, Error));

	// Currency is positive integers only (E1).
	TestFalse(TEXT("a zero stake is REFUSED"),
		FAFLMatchReporter::ValidateFreeForAllEscrow(9, 0, 0, Error));

	// THE CONTROL, and it is what makes the three refusals above mean anything: a clean nine-way field passes.
	TestTrue(TEXT("a clean nine-human staked field is ACCEPTED"),
		FAFLMatchReporter::ValidateFreeForAllEscrow(9, 0, 100, Error));
	TestTrue(TEXT("and it carries no error text"), Error.IsEmpty());

	// NO DIVISIBILITY RULE HERE, and its absence is deliberate -- a solo player funds a whole unit alone, so
	// a stake that would not divide across a team is irrelevant. Squad BR (R92) would bring it back.
	TestTrue(TEXT("a stake that does not divide by the player count is still fine in a SOLO field"),
		FAFLMatchReporter::ValidateFreeForAllEscrow(7, 0, 100, Error));
	return true;
}
