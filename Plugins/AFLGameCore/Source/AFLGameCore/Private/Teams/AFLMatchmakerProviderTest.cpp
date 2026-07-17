// Copyright C12 AI Gaming. All Rights Reserved.
//
// afl.Teams.Matchmaker.Test -- the T2 Increment-2 AIK-only ACCEPTANCE for UAFLMatchmakerDataProvider.
//
// Proves the PURE reconcile core (ResolveAssignments) against the LOCKED backend contract (Bag_Man_Backend
// match-allocator GameSessionData), with a 2-TEAM roster + N-team + order-independence assertions. NO live
// server, NO GameLift: it feeds the provider the fixture JSON + ordered reconcile ids and asserts
// right-roster->right-teams. Run it from the editor/PIE console:  afl.Teams.Matchmaker.Test
// Read the log for  "AFL_MMTEST: ... ALL GREEN".  The live client->GameLift->right-team prove is S12.
//
// Dev-only (compiled out of shipping); delete alongside the S12 live hook.

#include "Teams/AFLMatchmakerDataProvider.h"

#if !UE_BUILD_SHIPPING

#include "AFLGameCore.h"                    // LogAFLGameCore
#include "Teams/AFLTeamAssignmentTypes.h"
#include "GenericTeamAgentInterface.h"     // FGenericTeamId::NoTeam
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"               // GetTransientPackage

namespace
{
	// The LOCKED contract shape (mirrors Bag_Man_Backend test/fixtures/match-found.json -> the allocator's
	// GameSessionData), extended to a 2-TEAM split -- the shipped fixture is single-team "0"; a real match is
	// "0"/"1". Interleaved teams so a naive index-parallel (non-reconciling) impl would FAIL test #2.
	static const TCHAR* GMatchmakerFixture2Team =
		TEXT("{")
		TEXT("\"matchId\":\"ironics-match-7f3a9c20-0001\",")
		TEXT("\"members\":[")
		TEXT("{\"id\":\"F1A2077AAAA0001\",\"type\":\"title_player_account\",\"team\":\"0\"},")
		TEXT("{\"id\":\"F1A2077AAAA0002\",\"type\":\"title_player_account\",\"team\":\"1\"},")
		TEXT("{\"id\":\"F1A2077AAAA0003\",\"type\":\"title_player_account\",\"team\":\"0\"},")
		TEXT("{\"id\":\"F1A2077AAAA0004\",\"type\":\"title_player_account\",\"team\":\"1\"}")
		TEXT("]}");

	void RunMatchmakerProviderTest()
	{
		int32 Pass = 0;
		int32 Fail = 0;
		auto Check = [&Pass, &Fail](bool bCond, const FString& What)
		{
			if (bCond) { ++Pass; UE_LOG(LogAFLGameCore, Log,   TEXT("AFL_MMTEST: PASS -- %s"), *What); }
			else       { ++Fail; UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MMTEST: FAIL -- %s"), *What); }
		};

		// IsAuthoritative() -> true (so UAFLBotFillComponent's converge goes inert on the ranked path).
		{
			const UAFLMatchmakerDataProvider* Provider = NewObject<UAFLMatchmakerDataProvider>(GetTransientPackage());
			Check(Provider && Provider->IsAuthoritative(), TEXT("IsAuthoritative() == true"));
		}

		// 1) Roster order -> each member lands on its own AFL team (roster "0"/"1" -> AFL teams 1/2).
		{
			const TArray<FString> Ids = { TEXT("F1A2077AAAA0001"), TEXT("F1A2077AAAA0002"),
										  TEXT("F1A2077AAAA0003"), TEXT("F1A2077AAAA0004") };
			const TArray<FAFLTeamAssignment> A = UAFLMatchmakerDataProvider::ResolveAssignments(GMatchmakerFixture2Team, Ids);
			Check(A.Num() == 4, FString::Printf(TEXT("4 assignments (got %d)"), A.Num()));
			if (A.Num() == 4)
			{
				Check(A[0].TeamId.GetId() == 1, TEXT("0001 (roster 0) -> AFL team 1"));
				Check(A[1].TeamId.GetId() == 2, TEXT("0002 (roster 1) -> AFL team 2"));
				Check(A[2].TeamId.GetId() == 1, TEXT("0003 (roster 0) -> AFL team 1"));
				Check(A[3].TeamId.GetId() == 2, TEXT("0004 (roster 1) -> AFL team 2"));
				Check(A[0].PlayerId == Ids[0], TEXT("assignment carries the reconcile id (PlayerId==0001)"));
			}
		}

		// 2) SHUFFLED connect order -> reconciled BY ID (not by index): each id keeps its roster team.
		{
			const TArray<FString> Ids = { TEXT("F1A2077AAAA0004"), TEXT("F1A2077AAAA0001"),
										  TEXT("F1A2077AAAA0002"), TEXT("F1A2077AAAA0003") };
			const TArray<FAFLTeamAssignment> A = UAFLMatchmakerDataProvider::ResolveAssignments(GMatchmakerFixture2Team, Ids);
			const bool bOk = A.Num() == 4
				&& A[0].TeamId.GetId() == 2    // 0004 (roster 1) -> AFL 2
				&& A[1].TeamId.GetId() == 1    // 0001 (roster 0) -> AFL 1
				&& A[2].TeamId.GetId() == 2    // 0002 (roster 1) -> AFL 2
				&& A[3].TeamId.GetId() == 1;   // 0003 (roster 0) -> AFL 1
			Check(bOk, TEXT("shuffled connect order reconciles by id (0004->AFL2,0001->AFL1,0002->AFL2,0003->AFL1)"));
		}

		// 3) An id NOT in the roster -> NoTeam (surfaced, no crash).
		{
			const TArray<FString> Ids = { TEXT("F1A2077AAAA0001"), TEXT("STRANGER-9999") };
			const TArray<FAFLTeamAssignment> A = UAFLMatchmakerDataProvider::ResolveAssignments(GMatchmakerFixture2Team, Ids);
			const bool bOk = A.Num() == 2 && A[0].TeamId.GetId() == 1 && A[1].TeamId == FGenericTeamId::NoTeam;
			Check(bOk, TEXT("unknown reconcile id -> NoTeam (0001 -> AFL team 1)"));
		}

		// 4) Malformed JSON -> empty roster, every id NoTeam (no crash).
		{
			const TArray<FString> Ids = { TEXT("F1A2077AAAA0001") };
			const TArray<FAFLTeamAssignment> A = UAFLMatchmakerDataProvider::ResolveAssignments(TEXT("{not json"), Ids);
			Check(A.Num() == 1 && A[0].TeamId == FGenericTeamId::NoTeam, TEXT("malformed JSON -> NoTeam (no crash)"));
		}

		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MMTEST: DONE -- %d passed, %d failed. %s"),
			Pass, Fail, (Fail == 0) ? TEXT("ALL GREEN") : TEXT("*** FAILURES ***"));
	}

	FAutoConsoleCommand GAFLMatchmakerTestCmd(
		TEXT("afl.Teams.Matchmaker.Test"),
		TEXT("T2 Increment 2 acceptance: prove UAFLMatchmakerDataProvider reconciles the locked GameSessionData contract to the right teams (N-team, id-based, order-independent). No live server."),
		FConsoleCommandDelegate::CreateStatic(&RunMatchmakerProviderTest));
}

#endif // !UE_BUILD_SHIPPING
