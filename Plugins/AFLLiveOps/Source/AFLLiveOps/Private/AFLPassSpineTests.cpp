// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLPassSpineTests.h" // UHT rule: a TU's own header first (was last -- flagged every build)

#include "AFLLiveOps.h"
#include "AFLPassProgressComponent.h"
#include "AFLPassRewardSink.h"
#include "AFLPassSeasonAsset.h"
#include "AFLStoreRotationAsset.h"
#include "GameFramework/Actor.h"

#if !UE_BUILD_SHIPPING

namespace
{
	int32 GRan = 0;
	int32 GPassed = 0;

	void Arm(const TCHAR* Name, const bool bOk, const FString& Detail)
	{
		++GRan;
		if (bOk) { ++GPassed; }
		UE_LOG(LogAFLLiveOps, Display, TEXT("AFL_TEST[PASS] %-52s %s  %s"),
			Name, bOk ? TEXT("PASS") : TEXT("FAIL"), *Detail);
	}

	/**
	 * A season that SHOULD validate clean. This is the POSITIVE CONTROL and it is the most important
	 * arm in the file: every negative arm below asserts that ValidateSeason returns a failure, and a
	 * validator that rejected everything would satisfy all of them. Without this, "7 mutation arms
	 * pass" would be indistinguishable from "the validator is broken".
	 */
	UAFLPassSeasonAsset* MakeGoodSeason()
	{
		UAFLPassSeasonAsset* S = NewObject<UAFLPassSeasonAsset>(GetTransientPackage());
		S->SeasonId = TEXT("S_TEST");
		S->StartUtc = FDateTime(2026, 1, 1);
		S->EndUtc   = FDateTime(2026, 3, 12);   // 10 weeks -- inside the ruled 9-13
		S->Tiers.Reset();

		// 100 tiers per ECONOMY_SPEC 4. Quadratic-ish curve so later tiers cost more; the exact shape
		// is a design choice, the MONOTONICITY is the invariant the validator enforces.
		for (int32 i = 0; i < 100; ++i)
		{
			FAFLPassTier T;
			T.XpThreshold = i * 1000 + (i * i * 5);
			if (i % 10 == 0)
			{
				T.FreeReward.CosmeticId = FName(*FString::Printf(TEXT("AFL.Watts.Pool%d"), i));
				T.FreeReward.Quantity   = 500;
			}
			T.PremiumReward.CosmeticId = FName(*FString::Printf(TEXT("AFL.Finish.PassS1T%d"), i));
			T.PremiumReward.Quantity   = 1;
			S->Tiers.Add(T);
		}
		return S;
	}

	/** One mutation arm: break a good season one way, assert the validator NOTICES. */
	void MutationArm(const TCHAR* Name, TFunctionRef<void(UAFLPassSeasonAsset&)> Break)
	{
		UAFLPassSeasonAsset* S = MakeGoodSeason();
		Break(*S);
		const TArray<FString> Fails = S->ValidateSeason();
		Arm(Name, Fails.Num() > 0,
			Fails.Num() > 0
				? FString::Printf(TEXT("caught: %s"), *Fails[0].Left(78))
				: TEXT("VALIDATOR DID NOT NOTICE -- this defect would ship"));
	}

	void RunPassSpine(UWorld* World)
	{
		GRan = 0; GPassed = 0;
		UE_LOG(LogAFLLiveOps, Display, TEXT("AFL_TEST[PASS] BEGIN -- live-ops spine"));

		// ── CONTROL ─────────────────────────────────────────────────────────────────────────────
		UAFLPassSeasonAsset* Good = MakeGoodSeason();
		{
			const TArray<FString> Fails = Good->ValidateSeason();
			Arm(TEXT("CONTROL: a valid season validates clean"), Fails.Num() == 0,
				Fails.Num() == 0
					? TEXT("0 failures")
					: FString::Printf(TEXT("%d failure(s), first: %s"), Fails.Num(), *Fails[0]));
		}

		// ── MUTATION ARMS ───────────────────────────────────────────────────────────────────────
		MutationArm(TEXT("mutation: empty SeasonId"),        [](UAFLPassSeasonAsset& S){ S.SeasonId = NAME_None; });
		MutationArm(TEXT("mutation: empty ladder"),          [](UAFLPassSeasonAsset& S){ S.Tiers.Reset(); });
		MutationArm(TEXT("mutation: window reversed"),       [](UAFLPassSeasonAsset& S){ Swap(S.StartUtc, S.EndUtc); });
		MutationArm(TEXT("mutation: 40-week season"),        [](UAFLPassSeasonAsset& S){ S.EndUtc = S.StartUtc + FTimespan::FromDays(280); });
		MutationArm(TEXT("mutation: tier 0 not at zero XP"), [](UAFLPassSeasonAsset& S){ S.Tiers[0].XpThreshold = 50; });
		MutationArm(TEXT("mutation: non-monotonic curve"),   [](UAFLPassSeasonAsset& S){ S.Tiers[40].XpThreshold = S.Tiers[39].XpThreshold; });
		MutationArm(TEXT("mutation: zero-quantity reward"),  [](UAFLPassSeasonAsset& S){ S.Tiers[10].FreeReward.Quantity = 0; });
		MutationArm(TEXT("mutation: free track all empty"),  [](UAFLPassSeasonAsset& S){ for (FAFLPassTier& T : S.Tiers) { T.FreeReward.CosmeticId = NAME_None; } });

		// ── CURVE LOOKUP ────────────────────────────────────────────────────────────────────────
		// Boundaries, not midpoints. An off-by-one in TierForXp is invisible in the middle of a tier
		// and obvious exactly ON a threshold.
		{
			const int32 T5 = Good->XpForTier(5);
			Arm(TEXT("curve: XP exactly on a threshold enters that tier"),
				Good->TierForXp(T5) == 5,
				FString::Printf(TEXT("xp=%d -> tier %d (want 5)"), T5, Good->TierForXp(T5)));

			Arm(TEXT("curve: one XP below a threshold stays in the lower tier"),
				Good->TierForXp(T5 - 1) == 4,
				FString::Printf(TEXT("xp=%d -> tier %d (want 4)"), T5 - 1, Good->TierForXp(T5 - 1)));

			Arm(TEXT("curve: zero XP is tier 0"),
				Good->TierForXp(0) == 0,
				FString::Printf(TEXT("-> tier %d"), Good->TierForXp(0)));

			Arm(TEXT("curve: XP past the ladder clamps to the last tier"),
				Good->TierForXp(MAX_int32) == Good->GetTierCount() - 1,
				FString::Printf(TEXT("-> tier %d (want %d)"),
					Good->TierForXp(MAX_int32), Good->GetTierCount() - 1));
		}

		// ── COMPONENT, SERVER-SIDE ──────────────────────────────────────────────────────────────
		AActor* Host = World ? World->SpawnActor<AActor>() : nullptr;
		if (!Host)
		{
			Arm(TEXT("component arms"), false, TEXT("could not spawn a host actor -- NOTHING TESTED"));
		}
		else
		{
			UAFLPassProgressComponent* C = NewObject<UAFLPassProgressComponent>(Host);
			C->RegisterComponent();
			C->ServerSetSeason(Good);

			Arm(TEXT("component: fresh progress starts at tier 0"),
				C->GetCurrentTier() == 0, FString::Printf(TEXT("tier=%d"), C->GetCurrentTier()));

			const int32 AfterGrant = C->ServerGrantXp(Good->XpForTier(3));
			Arm(TEXT("component: granting XP advances the tier"),
				AfterGrant == 3, FString::Printf(TEXT("tier=%d (want 3)"), AfterGrant));

			const int32 BeforeBad = C->GetCurrentTier();
			C->ServerGrantXp(0);
			C->ServerGrantXp(-500000);
			Arm(TEXT("component: non-positive XP is REFUSED, not applied"),
				C->GetCurrentTier() == BeforeBad,
				FString::Printf(TEXT("tier %d -> %d (must not move, and must not go backwards)"),
					BeforeBad, C->GetCurrentTier()));

			// SATURATION, not wrap. An overflow would wrap negative and read as a tier reset -- a
			// player at the top of a paid ladder dropping to tier 0 is the worst possible failure.
			C->ServerGrantXp(MAX_int32);
			C->ServerGrantXp(MAX_int32);
			Arm(TEXT("component: XP saturates instead of wrapping negative"),
				C->GetCurrentTier() == Good->GetTierCount() - 1,
				FString::Printf(TEXT("tier=%d (want %d)"), C->GetCurrentTier(), Good->GetTierCount() - 1));

			// STALE SEASON. Progress from another season must not read as progress in this one.
			UAFLPassSeasonAsset* Next = MakeGoodSeason();
			Next->SeasonId = TEXT("S_TEST_NEXT");
			C->ServerSetSeason(Next);
			Arm(TEXT("component: a new season resets progress to tier 0"),
				C->GetCurrentTier() == 0 && C->GetProgressForCurrentSeason().Xp == 0,
				FString::Printf(TEXT("tier=%d xp=%d"),
					C->GetCurrentTier(), C->GetProgressForCurrentSeason().Xp));

			// RE-SETTING THE SAME SEASON MUST NOT WIPE. This fires on every respawn and reconnect.
			C->ServerGrantXp(Next->XpForTier(7));
			C->ServerSetSeason(Next);
			Arm(TEXT("component: re-setting the SAME season preserves progress"),
				C->GetCurrentTier() == 7, FString::Printf(TEXT("tier=%d (want 7)"), C->GetCurrentTier()));

			// ── CLAIM (slice 2) ─────────────────────────────────────────────────────────────────
			//
			// Fresh component and a fresh season so claim state is not inherited from the XP arms.
			UAFLPassSeasonAsset* CS = MakeGoodSeason();
			CS->SeasonId = TEXT("S_CLAIM");
			UAFLPassProgressComponent* CC = NewObject<UAFLPassProgressComponent>(Host);
			CC->RegisterComponent();
			CC->ServerSetSeason(CS);

			UAFLPassSpineSinkHolder* Sink = NewObject<UAFLPassSpineSinkHolder>(Host);
			CC->SetRewardSink(TScriptInterface<IAFLPassRewardSink>(Sink));

			// Tier 10 carries BOTH a free reward (every 10th) and a premium reward.
			CC->ServerGrantXp(CS->XpForTier(10));

			// CONTROL: an earned tier with a working sink actually hands something over. Every arm
			// below asserts a REFUSAL, and a claim path that granted nothing would satisfy them all.
			Sink->Granted.Reset();
			const int32 G1 = CC->ServerClaimTier(10);
			Arm(TEXT("CONTROL claim: earned tier grants"),
				G1 > 0 && Sink->Granted.Num() == G1,
				FString::Printf(TEXT("granted=%d recorded=%d"), G1, Sink->Granted.Num()));

			// IDEMPOTENT. A retried packet or a double-tapped button must not pay twice.
			Sink->Granted.Reset();
			const int32 G2 = CC->ServerClaimTier(10);
			Arm(TEXT("claim: re-claiming the same tier grants nothing"),
				G2 == 0 && Sink->Granted.Num() == 0,
				FString::Printf(TEXT("granted=%d recorded=%d"), G2, Sink->Granted.Num()));

			// UNEARNED. The server checks against its own XP; this is what stops tier 99 on match one.
			Sink->Granted.Reset();
			const int32 G3 = CC->ServerClaimTier(80);
			Arm(TEXT("claim: unearned tier is REFUSED and grants nothing"),
				G3 == 0 && Sink->Granted.Num() == 0,
				FString::Printf(TEXT("granted=%d recorded=%d"), G3, Sink->Granted.Num()));

			// OUT OF RANGE.
			Sink->Granted.Reset();
			Arm(TEXT("claim: out-of-range tier is REFUSED"),
				CC->ServerClaimTier(9999) == 0 && CC->ServerClaimTier(-1) == 0
					&& Sink->Granted.Num() == 0,
				FString::Printf(TEXT("recorded=%d"), Sink->Granted.Num()));

			// ── ENTITLEMENT MOVES HERE -- the mutation arms that matter ─────────────────────────
			//
			// Tier 20 unclaimed, premium NOT held. The free half must settle and the premium half
			// must NOT -- and crucially must NOT be marked, or subscribing later finds it consumed.
			CC->ServerGrantXp(CS->XpForTier(20) - CS->XpForTier(10));
			Sink->Granted.Reset();
			const int32 G4 = CC->ServerClaimTier(20);
			const bool bOnlyFree = (G4 == 1) && Sink->Granted.Num() == 1
				&& Sink->Granted[0].Key == CS->Tiers[20].FreeReward.CosmeticId;
			Arm(TEXT("entitlement: premium refused without the subscription"),
				bOnlyFree,
				FString::Printf(TEXT("granted=%d first=%s"), G4,
					Sink->Granted.Num() ? *Sink->Granted[0].Key.ToString() : TEXT("<none>")));

			// THE ARM THIS WHOLE DESIGN EXISTS FOR: subscribe afterwards and the premium half is
			// still there. If the refusal above had set the bit, this grants nothing and a paying
			// player has silently lost the reward.
			CC->ServerSetPremiumHeld(true);
			Sink->Granted.Reset();
			const int32 G5 = CC->ServerClaimTier(20);
			Arm(TEXT("entitlement: premium claimable AFTER subscribing (bit was not consumed)"),
				G5 == 1 && Sink->Granted.Num() == 1
					&& Sink->Granted[0].Key == CS->Tiers[20].PremiumReward.CosmeticId,
				FString::Printf(TEXT("granted=%d first=%s"), G5,
					Sink->Granted.Num() ? *Sink->Granted[0].Key.ToString() : TEXT("<none>")));

			// A FAILED GRANT MUST NOT CONSUME THE TIER. Otherwise a transient failure costs the
			// player the reward permanently.
			CC->ServerGrantXp(CS->XpForTier(30) - CS->XpForTier(20));
			Sink->bRefuse = true;
			Sink->Granted.Reset();
			const int32 G6 = CC->ServerClaimTier(30);
			Sink->bRefuse = false;
			const int32 G7 = CC->ServerClaimTier(30);
			Arm(TEXT("failure: refused grant leaves the tier claimable (retry succeeds)"),
				G6 == 0 && G7 > 0,
				FString::Printf(TEXT("refused-claim=%d retry=%d"), G6, G7));

			// NO SINK. Must grant nothing and say so, rather than appearing to work.
			UAFLPassProgressComponent* NC = NewObject<UAFLPassProgressComponent>(Host);
			NC->RegisterComponent();
			NC->ServerSetSeason(CS);
			NC->ServerGrantXp(CS->XpForTier(10));
			Arm(TEXT("failure: no reward sink grants nothing"),
				NC->ServerClaimTier(10) == 0, TEXT("see the Error line naming SetRewardSink"));

			// CLAIM-ALL settles every earned, unclaimed tier.
			Sink->Granted.Reset();
			const int32 GAll = CC->ServerClaimAllEarned();
			Arm(TEXT("claim-all: settles remaining earned tiers"),
				GAll > 0 && Sink->Granted.Num() == GAll,
				FString::Printf(TEXT("granted=%d recorded=%d"), GAll, Sink->Granted.Num()));

			Sink->Granted.Reset();
			Arm(TEXT("claim-all: is idempotent once everything is settled"),
				CC->ServerClaimAllEarned() == 0 && Sink->Granted.Num() == 0,
				FString::Printf(TEXT("recorded=%d"), Sink->Granted.Num()));

			// -- SLICE 3: TIER VIEWER / UPSELL -------------------------------------------------
			//
			// Fresh component: the claim arms above deliberately left tiers settled.
			UAFLPassSeasonAsset* VS = MakeGoodSeason();
			VS->SeasonId = TEXT("S_VIEW");
			UAFLPassProgressComponent* VC = NewObject<UAFLPassProgressComponent>(Host);
			VC->RegisterComponent();
			VC->ServerSetSeason(VS);
			UAFLPassSpineSinkHolder* VSink = NewObject<UAFLPassSpineSinkHolder>(Host);
			VC->SetRewardSink(TScriptInterface<IAFLPassRewardSink>(VSink));
			VC->ServerGrantXp(VS->XpForTier(20));

			Arm(TEXT("view: earned tier reads earned, later tier does not"),
				VC->IsTierEarned(20) && !VC->IsTierEarned(21),
				FString::Printf(TEXT("t20=%d t21=%d"), VC->IsTierEarned(20), VC->IsTierEarned(21)));

			// KEEPS THE BUTTON HONEST: claimable must agree with what the claim would do. Premium is
			// unheld here, so it must NOT read claimable -- a viewer lighting it offers a button that
			// refuses, which is the silent no-op the states table forbids.
			Arm(TEXT("view: premium not claimable while unsubscribed"),
				VC->IsTrackClaimable(20, EAFLPassTrack::Free)
					&& !VC->IsTrackClaimable(20, EAFLPassTrack::Premium),
				FString::Printf(TEXT("free=%d premium=%d"),
					VC->IsTrackClaimable(20, EAFLPassTrack::Free) ? 1 : 0,
					VC->IsTrackClaimable(20, EAFLPassTrack::Premium) ? 1 : 0));

			// CLAIMABLE MUST PREDICT THE CLAIM. If these disagree, UI and server have drifted.
			const bool bPredictedFree = VC->IsTrackClaimable(20, EAFLPassTrack::Free);
			const int32 ActuallyGranted = VC->ServerClaimTier(20);
			Arm(TEXT("view: claimable predicted what the claim granted"),
				(bPredictedFree ? 1 : 0) == ActuallyGranted,
				FString::Printf(TEXT("predicted=%d granted=%d"), bPredictedFree ? 1 : 0, ActuallyGranted));

			Arm(TEXT("view: a claimed track stops reading claimable"),
				!VC->IsTrackClaimable(20, EAFLPassTrack::Free)
					&& VC->IsTrackClaimed(20, EAFLPassTrack::Free),
				TEXT("free settled"));

			// UPSELL IS A COUNT, not a flag: earned 0..20, premium on every tier, none claimed -> 21.
			const int32 Owed = VC->GetUnclaimablePremiumCount();
			Arm(TEXT("upsell: counts earned-but-unclaimable premium rewards"),
				Owed == 21, FString::Printf(TEXT("owed=%d (want 21)"), Owed));

			VC->ServerSetPremiumHeld(true);
			Arm(TEXT("upsell: nothing to sell once premium is held"),
				VC->GetUnclaimablePremiumCount() == 0,
				FString::Printf(TEXT("owed=%d"), VC->GetUnclaimablePremiumCount()));

			// -- SLICE 3: STORE ROTATION -------------------------------------------------------
			UAFLStoreRotationAsset* Rot = NewObject<UAFLStoreRotationAsset>(GetTransientPackage());
			Rot->EpochUtc = FDateTime(2026, 1, 1);
			Rot->PeriodDays = 7;
			Rot->SlotCount = 6;
			Rot->FeaturedCount = 2;
			for (int32 i = 0; i < 40; ++i)
			{
				Rot->Pool.Add(FName(*FString::Printf(TEXT("AFL.Finish.Rot%02d"), i)));
			}

			Arm(TEXT("CONTROL rotation: a valid rotation validates clean"),
				Rot->ValidateRotation().Num() == 0,
				FString::Printf(TEXT("%d failure(s)"), Rot->ValidateRotation().Num()));

			// DETERMINISM is the design: reopening the store mid-window must not reshuffle.
			Arm(TEXT("rotation: same window twice is identical"),
				Rot->GetOfferForWindow(3) == Rot->GetOfferForWindow(3), TEXT("stable"));

			Arm(TEXT("rotation: offer respects SlotCount"),
				Rot->GetOfferForWindow(3).Num() == 6,
				FString::Printf(TEXT("n=%d"), Rot->GetOfferForWindow(3).Num()));

			{
				const TArray<FName> W = Rot->GetOfferForWindow(3);
				const TSet<FName> Uniq(W);
				Arm(TEXT("rotation: no duplicate rows within a window"),
					Uniq.Num() == W.Num(),
					FString::Printf(TEXT("%d unique of %d"), Uniq.Num(), W.Num()));
			}

			{
				// Identical consecutive offers would be a rotation that does not rotate -- and it
				// would look like a working system.
				int32 Differing = 0;
				for (int32 w = 0; w < 12; ++w)
				{
					if (Rot->GetOfferForWindow(w) != Rot->GetOfferForWindow(w + 1)) { ++Differing; }
				}
				Arm(TEXT("rotation: consecutive windows differ"),
					Differing >= 11, FString::Printf(TEXT("%d/12 boundaries changed"), Differing));
			}

			{
				const TArray<FName> Off = Rot->GetOfferForWindow(5);
				const TArray<FName> Feat = Rot->GetFeaturedForWindow(5);
				bool bSubset = (Feat.Num() == 2);
				for (const FName& F : Feat) { bSubset = bSubset && Off.Contains(F); }
				Arm(TEXT("rotation: featured is a subset of the same offer"),
					bSubset, FString::Printf(TEXT("featured=%d offer=%d"), Feat.Num(), Off.Num()));
			}

			{
				// Truncating instead of flooring maps pre-epoch time onto window 0, so the first two
				// windows would share an offer.
				const int32 W0 = Rot->WindowIndexAt(Rot->EpochUtc);
				const int32 W1 = Rot->WindowIndexAt(Rot->EpochUtc + FTimespan::FromDays(7));
				const int32 WNeg = Rot->WindowIndexAt(Rot->EpochUtc - FTimespan::FromDays(1));
				Arm(TEXT("rotation: window maths floors, including before the epoch"),
					W0 == 0 && W1 == 1 && WNeg == -1,
					FString::Printf(TEXT("epoch=%d plus7d=%d minus1d=%d"), W0, W1, WNeg));
			}

			{
				UAFLStoreRotationAsset* Small = NewObject<UAFLStoreRotationAsset>(GetTransientPackage());
				Small->EpochUtc = FDateTime(2026, 1, 1);
				Small->PeriodDays = 7; Small->SlotCount = 6; Small->FeaturedCount = 2;
				Small->Pool.Add(TEXT("A")); Small->Pool.Add(TEXT("B")); Small->Pool.Add(TEXT("C"));
				const TArray<FName> W = Small->GetOfferForWindow(1);
				const TSet<FName> U(W);
				Arm(TEXT("rotation: pool under SlotCount offers the pool, no duplicates"),
					W.Num() == 3 && U.Num() == 3,
					FString::Printf(TEXT("n=%d unique=%d"), W.Num(), U.Num()));
				Arm(TEXT("rotation: undersized pool is REPORTED, not silently short"),
					Small->ValidateRotation().Num() > 0, TEXT("validator flags it"));
			}

			{
				// Editing the pool must change the draw, or a removed row stays on sale.
				const TArray<FName> Before = Rot->GetOfferForWindow(4);
				Rot->Pool.Add(TEXT("AFL.Finish.RotNEW"));
				Arm(TEXT("rotation: editing the pool changes the offer"),
					Rot->GetOfferForWindow(4) != Before, TEXT("pool is part of the seed"));
				Rot->Pool.Pop();
			}

			{
				auto RotMut = [&](const TCHAR* Name, TFunctionRef<void(UAFLStoreRotationAsset&)> Break)
				{
					UAFLStoreRotationAsset* R = NewObject<UAFLStoreRotationAsset>(GetTransientPackage());
					R->EpochUtc = FDateTime(2026, 1, 1);
					R->PeriodDays = 7; R->SlotCount = 6; R->FeaturedCount = 2;
					for (int32 i = 0; i < 40; ++i)
					{
						R->Pool.Add(FName(*FString::Printf(TEXT("R%02d"), i)));
					}
					Break(*R);
					const TArray<FString> F = R->ValidateRotation();
					Arm(Name, F.Num() > 0, F.Num() ? F[0].Left(70) : FString(TEXT("NOT CAUGHT")));
				};
				RotMut(TEXT("rot mutation: empty pool"),         [](UAFLStoreRotationAsset& R){ R.Pool.Reset(); });
				RotMut(TEXT("rot mutation: zero period"),        [](UAFLStoreRotationAsset& R){ R.PeriodDays = 0; });
				RotMut(TEXT("rot mutation: featured beyond slots"), [](UAFLStoreRotationAsset& R){ R.FeaturedCount = 99; });
				RotMut(TEXT("rot mutation: duplicate pool row"), [](UAFLStoreRotationAsset& R)
				{
					// COPY FIRST. R.Pool.Add(R.Pool[0]) passes a REFERENCE INTO the array being
					// grown; Add reallocates and the reference dangles. UE asserts on exactly this
					// (Array.h: Addr < GetData() || Addr >= GetData()+ArrayMax) and it took the
					// editor down mid-suite. Same aliasing family as the socket-transform capture:
					// a reference into live data is not a value.
					const FName First = R.Pool[0];
					R.Pool.Add(First);
				});
				RotMut(TEXT("rot mutation: unset epoch"),        [](UAFLStoreRotationAsset& R){ R.EpochUtc = FDateTime(0); });
			}

			Host->Destroy();
		}

		UE_LOG(LogAFLLiveOps, Display, TEXT("AFL_TEST[PASS] END -- %d/%d passed"), GPassed, GRan);
		if (GPassed != GRan)
		{
			UE_LOG(LogAFLLiveOps, Error, TEXT("AFL_TEST[PASS] %d ARM(S) FAILED"), GRan - GPassed);
		}
	}

	FAutoConsoleCommandWithWorld GAFLPassSpineCmd(
		TEXT("afl.Pass.Spine"),
		TEXT("Prove the live-ops spine: season validation (mutation-tested, with a positive control), "
		     "the XP curve at its boundaries, and the server-authoritative progress component."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&RunPassSpine));
}

#endif // !UE_BUILD_SHIPPING
