// Copyright C12 AI Gaming. All Rights Reserved.
//
// afl.Match.Result.Test -- acceptance for FAFLMatchResult::Validate, the seam that stops an illegal match
// result reaching settlement or rating.
//
// Every case below is one of the rulings closed in the R85-R96 pass. The point of the suite is NOT that
// Validate rejects garbage -- it is that Validate rejects the specific SHAPES the tier table forbids, because
// those are the ones a producer bug would actually generate: a Haywire match that thinks it is staked, a
// bot-filled match that thinks it is rated, a squad whose finishing positions have a gap in them. Each of
// those pays the wrong money or moves the wrong ladder, silently.
//
// Console:  afl.Match.Result.Test    Read the log for "AFL_RESULTTEST: ... ALL GREEN".
// Dev-only (compiled out of shipping), same as the matchmaker provider test beside it.

#include "Match/AFLMatchResultTypes.h"

#if !UE_BUILD_SHIPPING

#include "AFLGameCore.h"   // LogAFLGameCore
#include "HAL/IConsoleManager.h"

namespace
{
	/** A human participant. */
	FAFLMatchParticipant Human(const TCHAR* Id, int32 Team, int32 Place)
	{
		FAFLMatchParticipant P;
		P.ReconcileId = Id;
		P.TeamId = Team;
		P.FinishingPosition = Place;
		P.bIsBot = false;
		return P;
	}

	/** A bot -- deliberately carries NO reconcile id, which Validate also enforces. */
	FAFLMatchParticipant Bot(int32 Team, int32 Place)
	{
		FAFLMatchParticipant P;
		P.TeamId = Team;
		P.FinishingPosition = Place;
		P.bIsBot = true;
		return P;
	}

	/** A valid 1v1 MATCH PLAY result in the given tier, as the baseline every negative case mutates. */
	FAFLMatchResult MakeValidMatchPlay(EAFLPlayTier Tier)
	{
		const bool bStaked = (Tier != EAFLPlayTier::LeaguePlay);

		FAFLMatchResult R;
		R.MatchId = FGuid::NewGuid();
		R.Ruleset = EAFLRuleset::MatchPlay;
		R.League = bStaked ? EAFLLeague::ProMod : EAFLLeague::Haywire;
		R.Tier = Tier;
		R.bStaked = bStaked;
		R.bRanked = bStaked;              // LEAGUE PLAY is unrated (R87); staked tiers are rated (R85)
		R.WinningTeamId = 0;
		R.Participants = { Human(TEXT("F1A2077AAAA0001"), 0, 1), Human(TEXT("F1A2077AAAA0002"), 1, 2) };
		return R;
	}

	void RunMatchResultTest()
	{
		int32 Pass = 0;
		int32 Fail = 0;

		auto Check = [&Pass, &Fail](bool bCond, const FString& What)
		{
			if (bCond) { ++Pass; UE_LOG(LogAFLGameCore, Log,   TEXT("AFL_RESULTTEST: PASS -- %s"), *What); }
			else       { ++Fail; UE_LOG(LogAFLGameCore, Error, TEXT("AFL_RESULTTEST: FAIL -- %s"), *What); }
		};

		/** Assert the result is REJECTED, and that the reason mentions Fragment -- so a test cannot pass by
		 *  tripping some unrelated rule that happens to also reject. */
		auto CheckRejects = [&Check](const FAFLMatchResult& R, const TCHAR* Fragment, const FString& What)
		{
			FString Err;
			const bool bValid = R.Validate(Err);
			const bool bRightReason = !bValid && Err.Contains(Fragment);
			Check(bRightReason, FString::Printf(TEXT("%s  [reason: %s]"), *What,
				bValid ? TEXT("*** ACCEPTED, expected rejection ***") : *Err));
		};

		auto CheckAccepts = [&Check](const FAFLMatchResult& R, const FString& What)
		{
			FString Err;
			const bool bValid = R.Validate(Err);
			Check(bValid, FString::Printf(TEXT("%s%s"), *What, bValid ? TEXT("") : *FString::Printf(TEXT("  [rejected: %s]"), *Err)));
		};

		// ---- 1. the three tiers, each in its legal shape ----
		CheckAccepts(MakeValidMatchPlay(EAFLPlayTier::LeaguePlay), TEXT("LEAGUE PLAY 1v1 (unstaked, unrated, Haywire) is valid"));
		CheckAccepts(MakeValidMatchPlay(EAFLPlayTier::WattsPlay),  TEXT("WATTS PLAY 1v1 (staked, rated, Pro Mod) is valid"));
		CheckAccepts(MakeValidMatchPlay(EAFLPlayTier::VoltsPlay),  TEXT("VOLTS PLAY 1v1 (staked, rated, Pro Mod) is valid"));

		// ---- 2. R86: staked play is PRO MOD ONLY ----
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::WattsPlay);
			R.League = EAFLLeague::Haywire;
			CheckRejects(R, TEXT("PRO MOD ONLY"), TEXT("R86: a STAKED Haywire match is rejected"));
		}

		// ---- 3. R85: the tier fixes staked/rated; a result may not contradict its own tier ----
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::LeaguePlay);
			R.bStaked = true;
			CheckRejects(R, TEXT("tier/stake mismatch"), TEXT("R85: LEAGUE PLAY claiming to be staked is rejected"));
		}
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::WattsPlay);
			R.bStaked = false;
			CheckRejects(R, TEXT("tier/stake mismatch"), TEXT("R85: WATTS PLAY claiming to be unstaked is rejected"));
		}
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::LeaguePlay);
			R.bRanked = true;
			CheckRejects(R, TEXT("UNRATED"), TEXT("R87: LEAGUE PLAY claiming to be rated is rejected"));
		}
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::VoltsPlay);
			R.bRanked = false;
			CheckRejects(R, TEXT("must be rated"), TEXT("R85: a staked tier claiming to be unrated is rejected"));
		}

		// ---- 4. bots: permitted in LEAGUE PLAY, barred wherever the outcome moves a balance or a rating ----
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::LeaguePlay);
			R.Participants = { Human(TEXT("F1A2077AAAA0001"), 0, 1), Bot(1, 2) };
			CheckAccepts(R, TEXT("R87: a BOT-FILLED LEAGUE PLAY match is valid"));
		}
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::WattsPlay);
			R.Participants = { Human(TEXT("F1A2077AAAA0001"), 0, 1), Bot(1, 2) };
			CheckRejects(R, TEXT("bot participant"), TEXT("R85: a bot in a STAKED match is rejected"));
		}

		// ---- 5. identity: a human must be payable; a bot must not resolve to an account ----
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::WattsPlay);
			R.Participants[1].ReconcileId.Reset();
			CheckRejects(R, TEXT("empty ReconcileId"), TEXT("a human with no reconcile id is rejected (could not be paid)"));
		}
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::LeaguePlay);
			FAFLMatchParticipant B = Bot(1, 2);
			B.ReconcileId = TEXT("F1A2077AAAA0002");         // a bot wearing a real account key
			R.Participants = { Human(TEXT("F1A2077AAAA0001"), 0, 1), B };
			CheckRejects(R, TEXT("bot participant carries a ReconcileId"), TEXT("a bot carrying a reconcile id is rejected"));
		}

		// ---- 6. finishing positions: 1-based and DENSE (a gap misprices every position below it) ----
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::WattsPlay);
			R.Participants[1].FinishingPosition = 0;
			CheckRejects(R, TEXT("not 1-based"), TEXT("a 0-based finishing position is rejected"));
		}
		{
			FAFLMatchResult R;
			R.MatchId = FGuid::NewGuid();
			R.Ruleset = EAFLRuleset::BattleRoyale;
			R.League = EAFLLeague::ProMod;
			R.Tier = EAFLPlayTier::VoltsPlay;
			R.bStaked = true; R.bRanked = true;
			// positions 1, 2, 4 -- 3 is missing
			R.Participants = {
				Human(TEXT("F1A2077AAAA0001"), INDEX_NONE, 1),
				Human(TEXT("F1A2077AAAA0002"), INDEX_NONE, 2),
				Human(TEXT("F1A2077AAAA0003"), INDEX_NONE, 4),
			};
			CheckRejects(R, TEXT("not dense"), TEXT("a GAP in finishing positions is rejected"));
		}

		// ---- 7. R92: a SQUAD shares one finishing position, and that must stay legal ----
		{
			FAFLMatchResult R;
			R.MatchId = FGuid::NewGuid();
			R.Ruleset = EAFLRuleset::BattleRoyale;
			R.League = EAFLLeague::ProMod;
			R.Tier = EAFLPlayTier::WattsPlay;
			R.bStaked = true; R.bRanked = true;
			// two duos: team 0 finishes 1st, team 1 finishes 2nd -- positions REPEAT by design
			R.Participants = {
				Human(TEXT("F1A2077AAAA0001"), 0, 1), Human(TEXT("F1A2077AAAA0002"), 0, 1),
				Human(TEXT("F1A2077AAAA0003"), 1, 2), Human(TEXT("F1A2077AAAA0004"), 1, 2),
			};
			CheckAccepts(R, TEXT("R92: a squad BR result sharing one position per squad is valid"));
			Check(R.GetPaidPositionCount() == 2,
				FString::Printf(TEXT("R92: 4 participants in 2 squads -> N=2 paid positions, got %d"), R.GetPaidPositionCount()));
		}

		// ---- 8. per-ruleset shape ----
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::WattsPlay);
			R.WinningTeamId = 1;   // team 1 finished 2nd
			CheckRejects(R, TEXT("holds no participant in finishing position 1"), TEXT("MATCH PLAY winner disagreeing with position 1 is rejected"));
		}
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::WattsPlay);
			R.WinningTeamId = INDEX_NONE;
			CheckRejects(R, TEXT("no WinningTeamId"), TEXT("MATCH PLAY without a winning team is rejected"));
		}
		{
			FAFLMatchResult R;
			R.MatchId = FGuid::NewGuid();
			R.Ruleset = EAFLRuleset::BattleRoyale;
			R.League = EAFLLeague::ProMod;
			R.Tier = EAFLPlayTier::VoltsPlay;
			R.bStaked = true; R.bRanked = true;
			R.WinningTeamId = 0;   // meaningless under BR
			R.Participants = { Human(TEXT("F1A2077AAAA0001"), 0, 1), Human(TEXT("F1A2077AAAA0002"), 1, 2) };
			CheckRejects(R, TEXT("placement is the result"), TEXT("BATTLE ROYALE carrying a WinningTeamId is rejected"));
		}

		// ---- 9. the basics ----
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::WattsPlay);
			R.MatchId.Invalidate();
			CheckRejects(R, TEXT("MatchId is unset"), TEXT("an unset MatchId is rejected"));
		}
		{
			FAFLMatchResult R = MakeValidMatchPlay(EAFLPlayTier::LeaguePlay);
			R.Participants.Empty();
			CheckRejects(R, TEXT("no participants"), TEXT("an empty participant set is rejected"));
		}

		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_RESULTTEST: DONE -- %d passed, %d failed. %s"),
			Pass, Fail, (Fail == 0) ? TEXT("ALL GREEN") : TEXT("*** FAILURES ***"));
	}

	FAutoConsoleCommand GAFLMatchResultTestCmd(
		TEXT("afl.Match.Result.Test"),
		TEXT("Acceptance for FAFLMatchResult::Validate -- proves the tier table (R85/R86/R87), the bot bar, squad-shared finishing positions (R92) and the per-ruleset shape are enforced before a result reaches settlement or rating."),
		FConsoleCommandDelegate::CreateStatic(&RunMatchResultTest));
}

#endif // !UE_BUILD_SHIPPING
