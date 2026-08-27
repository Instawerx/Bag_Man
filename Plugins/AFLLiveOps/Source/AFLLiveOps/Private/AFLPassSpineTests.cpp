// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLLiveOps.h"
#include "AFLPassProgressComponent.h"
#include "AFLPassSeasonAsset.h"
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
