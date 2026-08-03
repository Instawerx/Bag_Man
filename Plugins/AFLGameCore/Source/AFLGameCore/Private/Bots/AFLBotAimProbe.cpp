// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLGameCore.h"
#include "AFLMatchTierSource.h"
#include "Bots/AFLBotController.h"
#include "Engine/World.h"
#include "EngineUtils.h"             // TActorIterator
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

/**
 * afl.Bot.AimProbe  (AI-1 proof)
 *
 * "Bot aim feels human" is not a criterion. This turns the four floors into ASSERTIONS a grep can settle,
 * in the shape AFLChaosTestRunner / afl.Match.Test.Run already establish: a HOST-only console command that
 * walks live state and prints PASS/FAIL per claim.
 *
 * It is a READER, not a spawner. It inspects whatever bots the current match already has -- so it reports
 * against the real tier, the real rolls and the real acquisitions rather than a synthetic rig that could
 * pass while the shipping path fails. Run it mid-round, after bots have engaged at least once.
 *
 * WHAT EACH LINE PROVES
 *   REACTION  reactionDelay >= MinReactionSeconds        -- the perception floor holds
 *   RATE      peakRate      <= MaxTrackRateDegPerSec     -- the motor cap was never exceeded
 *   OVERSHOOT crossings     >= 1                         -- it passes the target and corrects. Never
 *                                                          overshooting is the FAIL: that is a servo.
 *   WOBBLE    SteadyErrorDeg > MinSteadyErrorDeg         -- residual error can never resolve to zero
 *   VARIANCE  no two bots share a resolved profile       -- five names, five hands
 *   TIER      tier within [0,1], profile within clamps   -- the envelope held after lerp AND roll
 */
namespace
{
	bool ProfilesEffectivelyEqual(const FAFLBotAimProfile& A, const FAFLBotAimProfile& B)
	{
		auto Near = [](float X, float Y) { return FMath::IsNearlyEqual(X, Y, 1e-3f); };
		return Near(A.ReactionSeconds,       B.ReactionSeconds)
			&& Near(A.MaxTrackRateDegPerSec, B.MaxTrackRateDegPerSec)
			&& Near(A.TrackStiffness,        B.TrackStiffness)
			&& Near(A.TrackDampingRatio,     B.TrackDampingRatio)
			&& Near(A.SteadyErrorDeg,        B.SteadyErrorDeg)
			&& Near(A.ErrorFrequencyHz,      B.ErrorFrequencyHz);
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLBotAimProbeCmd(
		TEXT("afl.Bot.AimProbe"),
		TEXT("AI-1 proof: assert the bot aim model's floors against every live AAFLBotController -- reaction floor, rate cap, overshoot, wobble, per-bot variance, tier envelope. HOST only; run mid-round after bots have engaged."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				if (!World || World->GetNetMode() == NM_Client)
				{
					Ar.Log(TEXT("afl.Bot.AimProbe -- HOST window only (bot controllers are server-side)."));
					return;
				}

				TArray<AAFLBotController*> Bots;
				for (TActorIterator<AAFLBotController> It(World); It; ++It)
				{
					Bots.Add(*It);
				}
				if (Bots.Num() == 0)
				{
					Ar.Log(TEXT("afl.Bot.AimProbe -- FAIL: no AAFLBotController in the world. Is B_AFLBotFill's BotControllerClass pointed at the AFL controller?"));
					return;
				}

				// Tier envelope is shared, so read the table off the first bot that has one.
				const UAFLBotAimTiers* Tiers = nullptr;
				for (const AAFLBotController* B : Bots)
				{
					if (B->AimTiers) { Tiers = B->AimTiers; break; }
				}
				if (!Tiers)
				{
					Ar.Log(TEXT("afl.Bot.AimProbe -- FAIL: no AimTiers asset assigned. The model is INERT and bots are using stock snap-aim."));
					return;
				}

				int32 Fails = 0;
				int32 NoData = 0;

				// THREE outcomes, not two. A test that can only say PASS or FAIL will report PASS for a
				// bot that has never moved -- "peak rate 0 vs cap 69" is a test a dead bot passes, and two
				// of those shipped in the first version. NO DATA is counted separately and keeps the run
				// from being called green.
				auto Check = [&Ar, &Fails](bool bOk, const TCHAR* Label, const FString& Detail)
				{
					Ar.Logf(TEXT("  [%s] %-9s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), Label, *Detail);
					if (!bOk) { ++Fails; }
				};
				auto CheckData = [&Ar, &Fails, &NoData](bool bHasData, bool bOk, const TCHAR* Label, const FString& Detail)
				{
					if (!bHasData)
					{
						Ar.Logf(TEXT("  [ -- ] %-9s NO DATA -- %s"), Label, *Detail);
						++NoData;
						return;
					}
					Ar.Logf(TEXT("  [%s] %-9s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), Label, *Detail);
					if (!bOk) { ++Fails; }
				};

				Ar.Logf(TEXT("afl.Bot.AimProbe -- %d bot controller(s); floors: react>=%.3fs rate<=%.0fd/s err>=%.2fdeg"),
					Bots.Num(), Tiers->MinReactionSeconds, Tiers->MaxTrackRateCeilingDegPerSec, Tiers->MinSteadyErrorDeg);

				for (const AAFLBotController* B : Bots)
				{
					const FAFLBotAimProfile& P = B->GetAimProfile();
					const APlayerState* PS = B->GetPlayerState<APlayerState>();
					Ar.Logf(TEXT("--- %s (%s) tier=%.2f ---"),
						*B->GetName(), PS ? *PS->GetPlayerName() : TEXT("?"), B->GetAimTier());
					Ar.Logf(TEXT("    react=%.3fs rate=%.0fd/s stiff=%.0f damp=%.2f err=%.2fdeg freq=%.2fHz"),
						P.ReactionSeconds, P.MaxTrackRateDegPerSec, P.TrackStiffness,
						P.TrackDampingRatio, P.SteadyErrorDeg, P.ErrorFrequencyHz);

					// -- CONFIG assertions. Valid only once OnPossess has rolled and resolved. --
					const bool bResolved = B->IsProfileResolved();

					CheckData(bResolved, P.ReactionSeconds >= Tiers->MinReactionSeconds - KINDA_SMALL_NUMBER, TEXT("REACTION"),
						FString::Printf(TEXT("resolved %.3fs vs floor %.3fs"), P.ReactionSeconds, Tiers->MinReactionSeconds));

					CheckData(bResolved, P.MaxTrackRateDegPerSec <= Tiers->MaxTrackRateCeilingDegPerSec + KINDA_SMALL_NUMBER, TEXT("RATECAP"),
						FString::Printf(TEXT("resolved %.0f vs ceiling %.0f"), P.MaxTrackRateDegPerSec, Tiers->MaxTrackRateCeilingDegPerSec));

					CheckData(bResolved, P.SteadyErrorDeg >= Tiers->MinSteadyErrorDeg - KINDA_SMALL_NUMBER, TEXT("WOBBLE"),
						FString::Printf(TEXT("resolved %.2fdeg vs floor %.2fdeg (must never be 0)"), P.SteadyErrorDeg, Tiers->MinSteadyErrorDeg));

					CheckData(bResolved, P.TrackDampingRatio < 1.0f, TEXT("DAMPING"),
						FString::Printf(TEXT("%.2f -- must stay <1 so aim overshoots and corrects"), P.TrackDampingRatio));

					CheckData(bResolved, B->GetAimTier() >= 0.0f && B->GetAimTier() <= 1.0f, TEXT("TIER"),
						FString::Printf(TEXT("%.2f in [0,1]"), B->GetAimTier()));

					// -- RUNTIME assertions, all on LIFETIME maxima. Per-acquisition counters reset every
					//    1-2s, so sampling them at one instant reads a fragment: that is what produced four
					//    false OVERSHOOT failures against a model logging 147 real crossings. --
					const int32 Acqs = B->GetAcquisitionCount();
					const bool  bTracked = B->HasTrackedEver();

					CheckData(Acqs > 0,
						B->GetLifetimeMinReactionDelay() >= Tiers->MinReactionSeconds - KINDA_SMALL_NUMBER, TEXT("REACTED"),
						FString::Printf(TEXT("min over %d acquisition(s) %.3fs vs floor %.3fs"),
							Acqs, B->GetLifetimeMinReactionDelay(), Tiers->MinReactionSeconds));

					CheckData(bTracked,
						B->GetLifetimePeakRate() <= P.MaxTrackRateDegPerSec + 1.0f, TEXT("PEAKRATE"),
						FString::Printf(TEXT("lifetime peak %.0fd/s vs cap %.0fd/s"),
							B->GetLifetimePeakRate(), P.MaxTrackRateDegPerSec));

					CheckData(bTracked,
						B->GetLifetimeMaxCrossings() >= 1, TEXT("OVERSHOOT"),
						FString::Printf(TEXT("%d true-error crossing(s) in the best acquisition (%d total acqs) -- 0 means asymptotic"),
							B->GetLifetimeMaxCrossings(), Acqs));

					// DEPTH -- DIAGNOSTIC, NOT AN ASSERTION. It reported 140.53deg "past the true target",
					// which is not an overshoot; it is the bearing swinging when a close target crosses the
					// bot. The number measures target geometry, not the tracker. It also could not fail in
					// practice -- any crossing at all leaves a non-zero peak, so ">0" was satisfied by
					// construction. Same family as OBSERVED in the move probe: keep the number visible
					// because it is occasionally informative, but stop letting it stand as proof.
					// OVERSHOOT above is the real assertion -- crossings reset per acquisition, so they
					// cannot be manufactured by a target switch.
					Ar.Logf(TEXT("  [ ~~ ] %-9s peak %.2fdeg past the true target -- DIAGNOSTIC ONLY (bearing swing on a crossing target inflates this; not an overshoot measure)"),
						TEXT("DEPTH"), B->GetLifetimeMaxOvershootDeg());

					Ar.Logf(TEXT("        (diagnostic, current acquisition: crossings=%d peak=%.0fd/s lastReact=%.3fs)"),
						B->GetErrorSignCrossings(), B->GetPeakTrackRate(), B->GetLastReactionDelay());
				}

				// PER-BOT VARIANCE: two bots at the same tier must differ, or the roster is one AI in five hats.
				bool bAnyDuplicate = false;
				for (int32 i = 0; i < Bots.Num() && !bAnyDuplicate; ++i)
				{
					for (int32 j = i + 1; j < Bots.Num(); ++j)
					{
						if (ProfilesEffectivelyEqual(Bots[i]->GetAimProfile(), Bots[j]->GetAimProfile()))
						{
							Ar.Logf(TEXT("  [FAIL] VARIANCE  %s and %s resolved IDENTICAL profiles"),
								*Bots[i]->GetName(), *Bots[j]->GetName());
							bAnyDuplicate = true;
							++Fails;
							break;
						}
					}
				}
				if (!bAnyDuplicate && Bots.Num() > 1)
				{
					Ar.Logf(TEXT("  [PASS] VARIANCE  all %d profiles distinct"), Bots.Num());
				}

				// A run with unproven assertions is NOT green. Reporting PASS while checks were skipped is
				// how the first version called a dead bot healthy.
				const TCHAR* Verdict = (Fails > 0) ? TEXT("FAIL") : (NoData > 0 ? TEXT("INCONCLUSIVE") : TEXT("PASS"));
				Ar.Logf(TEXT("afl.Bot.AimProbe -- %s (%d failure(s), %d unproven)"), Verdict, Fails, NoData);
				if (NoData > 0)
				{
					Ar.Log(TEXT("  NO DATA means the bot had not tracked yet, NOT that it passed. Re-run mid-firefight, after bots have engaged."));
				}
			}));
}
