// Copyright C12 AI Gaming. All Rights Reserved.

#include "Bots/AFLBotProbe.h"

#include "AFLGameCore.h"
#include "BehaviorTree/BlackboardComponent.h"
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

	/** AI-3 sprint window, two-sided.
	 *
	 *  LOW EDGE 0.03. Below 3% the ability is effectively inert -- the GameplayEvent never reached it, the
	 *  threshold is unreachable, or the lease is expiring before the speed swap lands. Zero is the specific
	 *  reading for "a bot cannot trigger this at all", which is the state AI-3 exists to leave behind.
	 *
	 *  HIGH EDGE 0.70. Above 70% the bot has one gear. A bot permanently at 1.4x is no more human than a bot
	 *  permanently at 1.0x -- the tell is the CHANGE of pace, and a sprint that never ends also means the lease
	 *  is not expiring, which is the AI-0 latch wearing a new hat. This edge is the one that catches it. */
	constexpr float SprintFractionMin  = 0.03f;
	constexpr float SprintFractionMax  = 0.70f;

	/** Mean held range must be within PreferredRangeCm +/- RangeBandCm * this.
	 *  WAS 2.0, WHICH COULD NOT FAIL. Every bot ran the same fixed 400-1100 donut, so every mean landed
	 *  inside a doubled band by construction and RANGE passed while the per-bot mechanism was entirely
	 *  disconnected. 1.0 means the bot must actually hold ITS OWN donut. */
	constexpr float RangeTolerance     = 1.0f;

	// (An ObservedSpreadMinRatio threshold was written here and deleted: replayed against a run with the
	//  mechanism provably disconnected it scored 1.32 and would have passed. See the OBSERVED block.)

	bool MoveProfilesEqual(const FAFLBotMoveProfile& A, const FAFLBotMoveProfile& B)
	{
		auto Near = [](float X, float Y) { return FMath::IsNearlyEqual(X, Y, 1e-2f); };
		return Near(A.PreferredRangeCm, B.PreferredRangeCm)
			&& Near(A.RangeBandCm, B.RangeBandCm)
			&& Near(A.RepositionIntervalSec, B.RepositionIntervalSec)
			&& Near(A.LateralBias, B.LateralBias);
	}

}   // end anonymous namespace

namespace AFLBotProbe
{
	void RunMove(UWorld* World, FOutputDevice& Ar)
	{
		// Body identical whether a human typed the command or the world tore down. Two call sites, one
		// implementation -- an auto-run that drifted from the console version would be worse than none.
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

					// DETECTOR SELF-TEST. BOUND below infers "the key name is right" from the blackboard
					// reading back a non-zero value -- which is only evidence if a WRONG name really does
					// read zero. So read a key that cannot exist and prove the detector discriminates before
					// believing anything it says. UBlackboardComponent::GetValue returns InvalidValue silently
					// for an unknown key, so a non-zero control means BOUND is measuring nothing at all.
					for (const AAFLBotController* B : Bots)
					{
						if (const UBlackboardComponent* BB = B->GetBlackboardComponent())
						{
							const float Control = BB->GetValueAsFloat(TEXT("AFL_NoSuchKey_DetectorSelfTest"));
							CheckData(true, FMath::IsNearlyZero(Control), TEXT("SELFTEST"),
								FString::Printf(TEXT("a key that cannot exist reads %.2f -- BOUND %s discriminate a key-name drift"),
									Control, FMath::IsNearlyZero(Control) ? TEXT("CAN") : TEXT("CANNOT")));
							break;
						}
					}

				for (const AAFLBotController* B : Bots)
				{
					const FAFLBotMoveProfile& M = B->GetMoveProfile();
					const APlayerState* PS = B->GetPlayerState<APlayerState>();
					const float T = B->GetCombatSampleSeconds();

					Ar.Logf(TEXT("--- %s (%s) tier=%.2f  combat=%.1fs ---"),
						*B->GetName(), PS ? *PS->GetPlayerName() : TEXT("?"), B->GetAimTier(), T);
					Ar.Logf(TEXT("    range=%.0fcm band=%.0fcm interval=%.2fs lateral=%.2f  pushed=%s  sprintAt>%.0fcm"),
						M.PreferredRangeCm, M.RangeBandCm, M.RepositionIntervalSec, M.LateralBias,
						B->AreMoveParamsPushed() ? TEXT("yes") : TEXT("NO"),
						FMath::Max(300.0f, M.PreferredRangeCm));

					// If the params never reached the blackboard the bot is running the query's authored
					// defaults, so every metric below describes the wrong thing. Fail loudly, not silently.
					CheckData(true, B->AreMoveParamsPushed(), TEXT("PARAMS"),
						B->AreMoveParamsPushed()
							? FString(TEXT("movement params reached the blackboard"))
							: FString(TEXT("params NOT pushed -- bot is on the query's DEFAULTS, not its profile")));

					// BOUND -- the C++ <-> blackboard half of the EQS binding, read back from the LIVE
					// blackboard rather than trusted. This is the one check that can catch a key-name drift:
					// SetValueAsFloat on a key the asset does not have is a silent no-op, so a renamed key
					// leaves the value at 0 here while PARAMS above still reports "pushed". Compared against
					// the controller's own pushed radii, never re-derived -- a probe that recomputes the
					// expectation from the same inputs only ever agrees with itself.
					const UBlackboardComponent* BB = B->GetBlackboardComponent();
					const float BBInner   = BB ? BB->GetValueAsFloat(AAFLBotController::BBKey_DonutInner)  : 0.0f;
					const float BBOuter   = BB ? BB->GetValueAsFloat(AAFLBotController::BBKey_DonutOuter)  : 0.0f;
					const float BBLateral = BB ? BB->GetValueAsFloat(AAFLBotController::BBKey_LateralBias) : 0.0f;
					const bool bBound = BB
						&& FMath::IsNearlyEqual(BBInner,   B->GetDonutInnerCm(), 1.0f)
						&& FMath::IsNearlyEqual(BBOuter,   B->GetDonutOuterCm(), 1.0f)
						&& FMath::IsNearlyEqual(BBLateral, M.LateralBias,        KINDA_SMALL_NUMBER);
					CheckData(B->AreMoveParamsPushed(), bBound, TEXT("BOUND"),
						FString::Printf(TEXT("blackboard reads donut=[%.0f..%.0f] lateral=%.2f vs pushed [%.0f..%.0f] %.2f (all-zero = key-name drift; EQS would silently run its authored defaults)"),
							BBInner, BBOuter, BBLateral,
							B->GetDonutInnerCm(), B->GetDonutOuterCm(), M.LateralBias));

					// ================= PER-ROUND ASSERTIONS =================
					// Everything below is evaluated over EVERY round with a usable sample and fails on the
					// WORST one, naming it. A lifetime mean cannot see an intermittent extreme -- it read
					// STATIONARY as a mild 2-of-5 tuning miss while bots sat wedged at 97%, and SPRINT as a
					// comfortable 62% while three bots ran 94-95% for the only real combat round. Both were
					// found from the per-round snapshot log, not from this command. Now the command sees it.
					TArray<FAFLBotRoundSummary> Rounds = B->GetRoundHistory();
					const FAFLBotRoundSummary Cur = B->GetCurrentRoundSummary();
					if (Cur.CombatSeconds >= MinCombatSeconds)
					{
						Rounds.Add(Cur);   // the round in progress counts once it has a real sample
					}
					Rounds.RemoveAll([](const FAFLBotRoundSummary& R) { return R.CombatSeconds < MinCombatSeconds; });
					const bool bRounds = Rounds.Num() > 0;

					/** Fail on the worst round for a metric; report which round and the value. */
					auto WorstOf = [&Rounds](TFunctionRef<float(const FAFLBotRoundSummary&)> Get,
					                         TFunctionRef<bool(float)> bBad,
					                         float& OutVal, int32& OutRound) -> bool
					{
						bool bAny = false;
						for (const FAFLBotRoundSummary& R : Rounds)
						{
							const float V = Get(R);
							if (bBad(V) && (!bAny || FMath::Abs(V) > FMath::Abs(OutVal)))
							{
								OutVal = V; OutRound = R.Round; bAny = true;
							}
						}
						return bAny;   // true => at least one round is outside the window
					};

					float BadV = 0.0f; int32 BadR = INDEX_NONE;

					BadV = 0.f; BadR = INDEX_NONE;
					CheckData(bRounds, !WorstOf([](const FAFLBotRoundSummary& R){ return R.Stationary; },
							[](float V){ return V > StationaryMax || V < StationaryMin; }, BadV, BadR), TEXT("STATIONARY"),
						BadR == INDEX_NONE
							? FString::Printf(TEXT("every one of %d round(s) inside %.0f-%.0f%%"), Rounds.Num(), StationaryMin*100.f, StationaryMax*100.f)
							: FString::Printf(TEXT("round %d hit %.0f%% (window %.0f-%.0f%%, %d rounds checked; high = the standing-still bug, near-zero = never pauses)"),
								BadR, BadV*100.f, StationaryMin*100.f, StationaryMax*100.f, Rounds.Num()));

					BadV = 0.f; BadR = INDEX_NONE;
					CheckData(bRounds, !WorstOf([](const FAFLBotRoundSummary& R){ return (float)R.Cells; },
							[](float V){ return V < (float)DistinctCellsMin; }, BadV, BadR), TEXT("POSITIONS"),
						BadR == INDEX_NONE
							? FString::Printf(TEXT("every one of %d round(s) >= %d cells"), Rounds.Num(), DistinctCellsMin)
							: FString::Printf(TEXT("round %d covered only %.0f cells (min %d; the reposition cycle terminated that round)"), BadR, BadV, DistinctCellsMin));

					BadV = 0.f; BadR = INDEX_NONE;
					CheckData(bRounds, !WorstOf([](const FAFLBotRoundSummary& R){ return R.Lateral; },
							[](float V){ return V < LateralRatioMin; }, BadV, BadR), TEXT("LATERAL"),
						BadR == INDEX_NONE
							? FString::Printf(TEXT("every one of %d round(s) >= %.2f lateral/forward"), Rounds.Num(), LateralRatioMin)
							: FString::Printf(TEXT("round %d fell to %.2f lateral/forward (min %.2f; velocity parallel to aim = not strafing)"), BadR, BadV, LateralRatioMin));

					BadV = 0.f; BadR = INDEX_NONE;
					CheckData(bRounds, !WorstOf([](const FAFLBotRoundSummary& R){ return R.Reversals; },
							[](float V){ return V > ReversalsMax || V < ReversalsMin; }, BadV, BadR), TEXT("REVERSALS"),
						BadR == INDEX_NONE
							? FString::Printf(TEXT("every one of %d round(s) inside %.2f-%.1f/s"), Rounds.Num(), ReversalsMin, ReversalsMax)
							: FString::Printf(TEXT("round %d hit %.2f/s (window %.2f-%.1f; high = thrashing, zero = one straight line)"), BadR, BadV, ReversalsMin, ReversalsMax));

					BadV = 0.f; BadR = INDEX_NONE;
					CheckData(bRounds, !WorstOf([](const FAFLBotRoundSummary& R){ return R.Sprint; },
							[](float V){ return V > SprintFractionMax || V < SprintFractionMin; }, BadV, BadR), TEXT("SPRINT"),
						BadR == INDEX_NONE
							? FString::Printf(TEXT("every one of %d round(s) inside %.0f-%.0f%%"), Rounds.Num(), SprintFractionMin*100.f, SprintFractionMax*100.f)
							: FString::Printf(TEXT("round %d hit %.0f%% sprinting (window %.0f-%.0f%%, %d rounds checked; ~0 = event never reached the ability, high = one gear / lease not expiring)"),
								BadR, BadV*100.f, SprintFractionMin*100.f, SprintFractionMax*100.f, Rounds.Num()));

					// RANGE compares each round against the profile HELD THAT ROUND. Preferred range moves with
					// the tier, so a whole-match mean measured against the current profile is the wrong yardstick.
					BadV = 0.f; BadR = INDEX_NONE;
					CheckData(bRounds, !WorstOf(
							[](const FAFLBotRoundSummary& R){ return (R.RangeBandCm > KINDA_SMALL_NUMBER && R.MeanRangeCm > 0.f)
								? (R.MeanRangeCm - R.PreferredRangeCm) / R.RangeBandCm : 0.0f; },
							[](float V){ return FMath::Abs(V) > RangeTolerance; }, BadV, BadR), TEXT("RANGE"),
						BadR == INDEX_NONE
							? FString::Printf(TEXT("every one of %d round(s) held within +/-%.0fx its own band"), Rounds.Num(), RangeTolerance)
							: FString::Printf(TEXT("round %d held %+.2f bands off its own preferred range (limit %.0fx)"), BadR, BadV, RangeTolerance));

					Ar.Logf(TEXT("        (lifetime, diagnostic only: stationary=%.0f%% sprint=%.0f%% cells=%d range=%.0fcm)"),
						B->GetStationaryFraction()*100.f, B->GetSprintFraction()*100.f,
						B->GetDistinctCellsVisited(), B->GetMeanRangeCm());
				}

				// -- VARIANCE, in two halves. The first is cheap and was all the original had; the second
				//    is the one that can actually fail when the profiles drive nothing. --
				bool bDup = false;
				for (int32 i = 0; i < Bots.Num() && !bDup; ++i)
				{
					for (int32 j = i + 1; j < Bots.Num(); ++j)
					{
						if (MoveProfilesEqual(Bots[i]->GetMoveProfile(), Bots[j]->GetMoveProfile()))
						{
							Ar.Logf(TEXT("  [FAIL] ROLLED     %s and %s resolved IDENTICAL movement profiles"),
								*Bots[i]->GetName(), *Bots[j]->GetName());
							bDup = true; ++Fails; break;
						}
					}
				}
				if (!bDup && Bots.Num() > 1)
				{
					Ar.Logf(TEXT("  [PASS] ROLLED     all %d rolled profiles distinct (structs only -- says nothing about behaviour)"), Bots.Num());
				}

				// OBSERVED variance. Five distinct profiles feeding one shared donut produce five nearly
				// identical mean ranges -- which is exactly the state this probe previously called a PASS.
				// Requiring the MEASURED spread to track the INTENDED spread is what makes a disconnected
				// mechanism fail instead of hide.
				float PrefMin = TNumericLimits<float>::Max(), PrefMax = -PrefMin;
				float ObsMin  = TNumericLimits<float>::Max(), ObsMax  = -ObsMin;
				int32 Sampled = 0;
				for (const AAFLBotController* B : Bots)
				{
					if (B->GetCombatSampleSeconds() < MinCombatSeconds || B->GetMeanRangeCm() <= 0.0f) { continue; }
					PrefMin = FMath::Min(PrefMin, B->GetMoveProfile().PreferredRangeCm);
					PrefMax = FMath::Max(PrefMax, B->GetMoveProfile().PreferredRangeCm);
					ObsMin  = FMath::Min(ObsMin,  B->GetMeanRangeCm());
					ObsMax  = FMath::Max(ObsMax,  B->GetMeanRangeCm());
					++Sampled;
				}
				if (Sampled < 2)
				{
					Ar.Logf(TEXT("  [ -- ] OBSERVED   NO DATA -- need 2+ bots with %.0fs of combat to compare spreads"), MinCombatSeconds);
					++NoData;
				}
				else
				{
					const float PrefSpread = PrefMax - PrefMin;
					const float ObsSpread  = ObsMax - ObsMin;
					const float Ratio = (PrefSpread > KINDA_SMALL_NUMBER) ? (ObsSpread / PrefSpread) : 0.0f;

					// DIAGNOSTIC, NOT AN ASSERTION -- and that is deliberate.
					//
					// This was written as a pass/fail on Ratio >= 0.30, then replayed against a run where
					// the per-bot mechanism was PROVABLY disconnected (BT_AFL_Bot referenced the four
					// blackboard keys zero times, every bot on one fixed 400-1100 donut). It scored 1.32
					// and would have PASSED. It measures combat noise -- bots fight at whatever distance
					// the fight happens -- not whether a profile drives geometry.
					//
					// The root cause is the tier values, not the test: PreferredRange spans ~290cm across
					// bots while each band is 237-368cm wide, so every bot's [pref-band, pref+band] window
					// overlaps every other's almost completely. No behavioural test can separate five bots
					// whose intended ranges are indistinguishable. Widen the spread (or narrow the bands)
					// and this becomes assertable; until then a pass here would mean nothing.
					Ar.Logf(TEXT("  [ ~~ ] OBSERVED   held-range spread %.0fcm vs preferred spread %.0fcm = %.2f -- DIAGNOSTIC ONLY (bands overlap; cannot discriminate a disconnected mechanism)"),
						ObsSpread, PrefSpread, Ratio);
				}

				const TCHAR* Verdict = (Fails > 0) ? TEXT("FAIL") : (NoData > 0 ? TEXT("INCONCLUSIVE") : TEXT("PASS"));
				Ar.Logf(TEXT("afl.Bot.MoveProbe -- %s (%d failure(s), %d unproven)"), Verdict, Fails, NoData);
				if (NoData > 0)
				{
					Ar.Logf(TEXT("  NO DATA means under %.0fs of COMBAT time (focus target held), not that it passed. Re-run mid-firefight."), MinCombatSeconds);
				}
		}
	}
}   // namespace AFLBotProbe

namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLBotMoveProbeCmd(
		TEXT("afl.Bot.MoveProbe"),
		TEXT("AI-2/AI-3 proof: assert combat movement PER ROUND against every live AAFLBotController -- stationary, distinct positions, lateral ratio, reversals, sprint fraction, range holding, per-bot variance. HOST only. Also auto-runs at end of play, so console timing is optional."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				AFLBotProbe::RunMove(World, Ar);
			}));
}
