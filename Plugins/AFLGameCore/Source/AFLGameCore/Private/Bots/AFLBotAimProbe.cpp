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
				int32 NoAcquireYet = 0;
				auto Check = [&Ar, &Fails](bool bOk, const TCHAR* Label, const FString& Detail)
				{
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

					Check(P.ReactionSeconds >= Tiers->MinReactionSeconds - KINDA_SMALL_NUMBER, TEXT("REACTION"),
						FString::Printf(TEXT("resolved %.3fs vs floor %.3fs"), P.ReactionSeconds, Tiers->MinReactionSeconds));

					Check(P.MaxTrackRateDegPerSec <= Tiers->MaxTrackRateCeilingDegPerSec + KINDA_SMALL_NUMBER, TEXT("RATECAP"),
						FString::Printf(TEXT("resolved %.0f vs ceiling %.0f"), P.MaxTrackRateDegPerSec, Tiers->MaxTrackRateCeilingDegPerSec));

					Check(P.SteadyErrorDeg >= Tiers->MinSteadyErrorDeg - KINDA_SMALL_NUMBER, TEXT("WOBBLE"),
						FString::Printf(TEXT("resolved %.2fdeg vs floor %.2fdeg (must never be 0)"), P.SteadyErrorDeg, Tiers->MinSteadyErrorDeg));

					Check(P.TrackDampingRatio < 1.0f, TEXT("DAMPING"),
						FString::Printf(TEXT("%.2f -- must stay <1 so aim overshoots and corrects"), P.TrackDampingRatio));

					Check(B->GetAimTier() >= 0.0f && B->GetAimTier() <= 1.0f, TEXT("TIER"),
						FString::Printf(TEXT("%.2f in [0,1]"), B->GetAimTier()));

					// Live acquisition metrics only exist once the bot has actually engaged something.
					if (B->GetLastReactionDelay() <= 0.0f)
					{
						++NoAcquireYet;
						Ar.Logf(TEXT("  [ .. ] ACQUIRED  no acquisition yet -- overshoot/peak-rate unproven for this bot"));
						continue;
					}
					Check(B->GetLastReactionDelay() >= Tiers->MinReactionSeconds - KINDA_SMALL_NUMBER, TEXT("REACTED"),
						FString::Printf(TEXT("last delay %.3fs vs floor %.3fs"), B->GetLastReactionDelay(), Tiers->MinReactionSeconds));
					Check(B->GetPeakTrackRate() <= P.MaxTrackRateDegPerSec + 1.0f, TEXT("PEAKRATE"),
						FString::Printf(TEXT("observed %.0fd/s vs cap %.0fd/s"), B->GetPeakTrackRate(), P.MaxTrackRateDegPerSec));
					Check(B->GetErrorSignCrossings() >= 1, TEXT("OVERSHOOT"),
						FString::Printf(TEXT("%d error sign-crossing(s) -- 0 means asymptotic, which reads robotic"), B->GetErrorSignCrossings()));
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

				Ar.Logf(TEXT("afl.Bot.AimProbe -- %s (%d failure(s)%s)"),
					Fails == 0 ? TEXT("PASS") : TEXT("FAIL"), Fails,
					NoAcquireYet > 0 ? *FString::Printf(TEXT(", %d bot(s) had not engaged yet"), NoAcquireYet) : TEXT(""));
				if (NoAcquireYet > 0)
				{
					Ar.Log(TEXT("  NOTE: overshoot and peak-rate are only asserted for bots that have acquired a target. Re-run mid-firefight."));
				}
			}));
}
