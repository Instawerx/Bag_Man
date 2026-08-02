// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLGameCore.h"
#include "Bots/AFLBotController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

/**
 * afl.Bot.MoveProbe  (AI-2 proof)
 *
 * Sibling of afl.Bot.AimProbe, and it inherits that probe's hard-won rules:
 *   - assert on LIFETIME accumulators, never on instantaneous state. The aim probe sampled per-acquisition
 *     counters at one instant and reported four false failures against a correct model.
 *   - THREE outcomes. A bot with no combat time reports NO DATA, never PASS. "0 d/s vs a 69 d/s cap" is a
 *     test a dead bot passes, and two of those shipped in the first aim probe.
 *   - every assertion must have a describable failure. An assertion nobody can state a failure for is not
 *     an assertion -- that is how the original overshoot test survived being unfalsifiable.
 *
 * AND ONE OF ITS OWN: STATIONARY and REVERSALS are TWO-SIDED. A single-sided "more movement is better"
 * test is passed perfectly by a bot vibrating in place at 10 Hz, which is exactly what the fast-cadence
 * failure mode produces -- and which reads worse to a player than a bot that simply stands still.
 */
namespace
{
	// Two-sided windows. Outside either edge is a real, nameable failure.
	constexpr float StationaryMax      = 0.25f;   // above: the standing-still bug AI-2 exists to fix
	constexpr float StationaryMin      = 0.02f;   // below: never pauses, reads as a machine on rails
	constexpr float ReversalsMax       = 2.0f;    // above: thrashing between two goals -- cadence too fast
	constexpr float ReversalsMin       = 0.05f;   // below: never changes lateral direction -> not strafing
	constexpr float LateralRatioMin    = 0.25f;   // below: velocity parallel to aim -> the stock symptom
	constexpr int32 DistinctCellsMin   = 3;       // below: the reposition cycle is still terminating
	constexpr float MinCombatSeconds   = 5.0f;    // below this, nothing is asserted -- sample too thin
	constexpr float RangeTolerance     = 2.0f;    // mean range must be within band * this

	bool MoveProfilesEqual(const FAFLBotMoveProfile& A, const FAFLBotMoveProfile& B)
	{
		auto Near = [](float X, float Y) { return FMath::IsNearlyEqual(X, Y, 1e-2f); };
		return Near(A.PreferredRangeCm, B.PreferredRangeCm)
			&& Near(A.RangeBandCm, B.RangeBandCm)
			&& Near(A.RepositionIntervalSec, B.RepositionIntervalSec)
			&& Near(A.LateralBias, B.LateralBias);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLBotMoveProbeCmd(
		TEXT("afl.Bot.MoveProbe"),
		TEXT("AI-2 proof: assert combat movement against every live AAFLBotController -- stationary fraction (two-sided), distinct positions, lateral/forward ratio, direction reversals (two-sided), range holding, per-bot variance. HOST only; run mid-firefight."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				if (!World || World->GetNetMode() == NM_Client)
				{
					Ar.Log(TEXT("afl.Bot.MoveProbe -- HOST window only (bot controllers are server-side)."));
					return;
				}

				TArray<AAFLBotController*> Bots;
				for (TActorIterator<AAFLBotController> It(World); It; ++It) { Bots.Add(*It); }
				if (Bots.Num() == 0)
				{
					Ar.Log(TEXT("afl.Bot.MoveProbe -- FAIL: no AAFLBotController in the world. Is B_AFLBotFill's BotControllerClass pointed at the AFL controller?"));
					return;
				}

				int32 Fails = 0, NoData = 0;
				auto CheckData = [&Ar, &Fails, &NoData](bool bHasData, bool bOk, const TCHAR* Label, const FString& Detail)
				{
					if (!bHasData) { Ar.Logf(TEXT("  [ -- ] %-10s NO DATA -- %s"), Label, *Detail); ++NoData; return; }
					Ar.Logf(TEXT("  [%s] %-10s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), Label, *Detail);
					if (!bOk) { ++Fails; }
				};

				Ar.Logf(TEXT("afl.Bot.MoveProbe -- %d bot(s); windows: stationary %.0f-%.0f%%, reversals %.2f-%.1f/s, lateral>=%.2f, cells>=%d"),
					Bots.Num(), StationaryMin * 100.f, StationaryMax * 100.f, ReversalsMin, ReversalsMax, LateralRatioMin, DistinctCellsMin);

				for (const AAFLBotController* B : Bots)
				{
					const FAFLBotMoveProfile& M = B->GetMoveProfile();
					const APlayerState* PS = B->GetPlayerState<APlayerState>();
					const float T = B->GetCombatSampleSeconds();
					const bool bEnough = T >= MinCombatSeconds;

					Ar.Logf(TEXT("--- %s (%s) tier=%.2f  combat=%.1fs ---"),
						*B->GetName(), PS ? *PS->GetPlayerName() : TEXT("?"), B->GetAimTier(), T);
					Ar.Logf(TEXT("    range=%.0fcm band=%.0fcm interval=%.2fs lateral=%.2f  pushed=%s"),
						M.PreferredRangeCm, M.RangeBandCm, M.RepositionIntervalSec, M.LateralBias,
						B->AreMoveParamsPushed() ? TEXT("yes") : TEXT("NO"));

					// If the params never reached the blackboard the bot is running the query's authored
					// defaults, so every metric below describes the wrong thing. Fail loudly, not silently.
					CheckData(true, B->AreMoveParamsPushed(), TEXT("PARAMS"),
						B->AreMoveParamsPushed()
							? FString(TEXT("movement params reached the blackboard"))
							: FString(TEXT("params NOT pushed -- bot is on the query's DEFAULTS, not its profile")));

					const float Stat = B->GetStationaryFraction();
					CheckData(bEnough, Stat <= StationaryMax && Stat >= StationaryMin, TEXT("STATIONARY"),
						FString::Printf(TEXT("%.0f%% of combat below %.0f uu/s (window %.0f-%.0f%%; high = the standing-still bug, near-zero = never pauses)"),
							Stat * 100.f, 40.0f, StationaryMin * 100.f, StationaryMax * 100.f));

					CheckData(bEnough, B->GetDistinctCellsVisited() >= DistinctCellsMin, TEXT("POSITIONS"),
						FString::Printf(TEXT("%d distinct 2m cells (min %d; 1-2 means the reposition cycle is terminating)"),
							B->GetDistinctCellsVisited(), DistinctCellsMin));

					CheckData(bEnough, B->GetLateralRatio() >= LateralRatioMin, TEXT("LATERAL"),
						FString::Printf(TEXT("%.2f lateral/forward (min %.2f; near-zero = velocity parallel to aim, i.e. not strafing)"),
							B->GetLateralRatio(), LateralRatioMin));

					const float Rev = B->GetReversalsPerSecond();
					CheckData(bEnough, Rev <= ReversalsMax && Rev >= ReversalsMin, TEXT("REVERSALS"),
						FString::Printf(TEXT("%.2f/s (window %.2f-%.1f; high = thrashing between goals, zero = one straight line)"),
							Rev, ReversalsMin, ReversalsMax));

					const float MeanR = B->GetMeanRangeCm();
					const bool bRangeOk = FMath::Abs(MeanR - M.PreferredRangeCm) <= M.RangeBandCm * RangeTolerance;
					CheckData(bEnough && MeanR > 0.0f, bRangeOk, TEXT("RANGE"),
						FString::Printf(TEXT("held %.0fcm vs preferred %.0f +/- %.0f (x%.0f tolerance)"),
							MeanR, M.PreferredRangeCm, M.RangeBandCm, RangeTolerance));
				}

				bool bDup = false;
				for (int32 i = 0; i < Bots.Num() && !bDup; ++i)
				{
					for (int32 j = i + 1; j < Bots.Num(); ++j)
					{
						if (MoveProfilesEqual(Bots[i]->GetMoveProfile(), Bots[j]->GetMoveProfile()))
						{
							Ar.Logf(TEXT("  [FAIL] VARIANCE   %s and %s resolved IDENTICAL movement profiles"),
								*Bots[i]->GetName(), *Bots[j]->GetName());
							bDup = true; ++Fails; break;
						}
					}
				}
				if (!bDup && Bots.Num() > 1)
				{
					Ar.Logf(TEXT("  [PASS] VARIANCE   all %d movement profiles distinct"), Bots.Num());
				}

				const TCHAR* Verdict = (Fails > 0) ? TEXT("FAIL") : (NoData > 0 ? TEXT("INCONCLUSIVE") : TEXT("PASS"));
				Ar.Logf(TEXT("afl.Bot.MoveProbe -- %s (%d failure(s), %d unproven)"), Verdict, Fails, NoData);
				if (NoData > 0)
				{
					Ar.Logf(TEXT("  NO DATA means under %.0fs of COMBAT time (focus target held), not that it passed. Re-run mid-firefight."), MinCombatSeconds);
				}
			}));
}
