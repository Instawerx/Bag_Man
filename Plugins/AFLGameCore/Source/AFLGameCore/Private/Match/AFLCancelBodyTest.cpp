// Copyright C12 AI Gaming. All Rights Reserved.
//
// afl.Match.Cancel.Test -- acceptance for FAFLMatchReporter::BuildCancelBody, the refund path for a match that
// did not happen.
//
// THE SUITE EXISTS BECAUSE THE ONLY OTHER WAY TO TEST THIS PATH IS TO ABANDON A REAL STAKED MATCH. The cancel
// body is built at the exact moment nobody is watching -- an empty server, an hour after the players left --
// and its failure mode is silent: a body the backend rejects looks identical, from inside the game, to a body
// it accepted. The refund simply does not arrive.
//
// So the assertions here are the backend's OWN preconditions, restated against the body we actually emit
// (`settle-match/index.ts` -> validateRequest, which runs BEFORE terminalState is even consulted):
//   - positions DENSE from 1                 -- a gap is rejected outright
//   - one UNIFORM stake total per position   -- the curve is denominated in stake units
//   - entryAmount EXACTLY as escrowed        -- verifyAgainstEscrow refuses any disagreement
//   - terminalState spelled 'cancelled-refund'
//
// Console:  afl.Match.Cancel.Test    Read the log for "AFL_CANCELTEST: ... ALL GREEN".
// Dev-only (compiled out of shipping), same as the result test beside it.

#include "Match/AFLMatchReporter.h"

#if !UE_BUILD_SHIPPING

#include "AFLGameCore.h"   // LogAFLGameCore
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FAFLEscrowedEntry Entry(const TCHAR* Id, int32 Team, int32 Amount, bool bConfirmed = true)
	{
		FAFLEscrowedEntry E;
		E.ReconcileId = Id;
		E.TeamId = Team;
		E.Amount = Amount;
		E.bConfirmed = bConfirmed;
		return E;
	}

	/** A 1v1 staked ledger -- the baseline every negative case mutates. Team ids 1/2, as the ShooterCore
	 *  two-team stack actually produces (NOT 0/1 -- a test that used 0/1 would not exercise the real mapping). */
	FAFLEscrowLedger MakeLedger1v1()
	{
		FAFLEscrowLedger L;
		L.MatchId = FGuid::NewGuid();
		L.CurrencyCode = TEXT("VO");
		L.StakePerPosition = 10;
		L.Entries = { Entry(TEXT("F1A2077AAAA0001"), 1, 10), Entry(TEXT("F1A2077AAAA0002"), 2, 10) };
		return L;
	}

	/** A 2v2 ledger: each TEAM stakes one unit of 10, split 5/5 across its members (R92). */
	FAFLEscrowLedger MakeLedger2v2()
	{
		FAFLEscrowLedger L;
		L.MatchId = FGuid::NewGuid();
		L.CurrencyCode = TEXT("WA");
		L.StakePerPosition = 10;
		L.Entries = {
			Entry(TEXT("F1A2077AAAA0001"), 1, 5), Entry(TEXT("F1A2077AAAA0002"), 1, 5),
			Entry(TEXT("F1A2077AAAA0003"), 2, 5), Entry(TEXT("F1A2077AAAA0004"), 2, 5),
		};
		return L;
	}

	TSharedPtr<FJsonObject> Parse(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
		return Root;
	}

	void RunCancelBodyTest()
	{
		int32 Pass = 0;
		int32 Fail = 0;

		auto Check = [&Pass, &Fail](bool bCond, const FString& What)
		{
			if (bCond) { ++Pass; UE_LOG(LogAFLGameCore, Log,   TEXT("AFL_CANCELTEST: PASS -- %s"), *What); }
			else       { ++Fail; UE_LOG(LogAFLGameCore, Error, TEXT("AFL_CANCELTEST: FAIL -- %s"), *What); }
		};

		auto CheckRejects = [&Check](const FAFLEscrowLedger& L, const TCHAR* Fragment, const FString& What)
		{
			FString Json, Err;
			const bool bBuilt = FAFLMatchReporter::BuildCancelBody(L, Json, Err);
			Check(!bBuilt && Err.Contains(Fragment), FString::Printf(TEXT("%s  [reason: %s]"), *What,
				bBuilt ? TEXT("*** ACCEPTED, expected rejection ***") : *Err));
		};

		// ---- 1. the shape the backend requires, on a plain 1v1 ----
		{
			const FAFLEscrowLedger L = MakeLedger1v1();
			FString Json, Err;
			const bool bBuilt = FAFLMatchReporter::BuildCancelBody(L, Json, Err);
			Check(bBuilt, FString::Printf(TEXT("a funded 1v1 ledger builds a cancel body%s"),
				bBuilt ? TEXT("") : *FString::Printf(TEXT("  [rejected: %s]"), *Err)));

			const TSharedPtr<FJsonObject> Root = bBuilt ? Parse(Json) : nullptr;
			Check(Root.IsValid(), TEXT("the cancel body is valid JSON"));
			if (Root.IsValid())
			{
				// THE TERMINAL STATE IS THE WHOLE POINT. 'settled' here would pay the payout curve against a
				// match nobody won, and take a rake on it.
				Check(Root->GetStringField(TEXT("terminalState")) == TEXT("cancelled-refund"),
					FString::Printf(TEXT("terminalState is 'cancelled-refund', got '%s'"), *Root->GetStringField(TEXT("terminalState"))));
				Check(Root->GetStringField(TEXT("currencyCode")) == TEXT("VO"), TEXT("currencyCode rides from the ledger"));
				Check(Root->GetStringField(TEXT("matchId")) == L.MatchId.ToString(EGuidFormats::DigitsWithHyphens),
					TEXT("matchId uses the ONE wire spelling escrow and settlement join on"));

				// NO rake, NO stake, NO winner field -- the body is entries and nothing else.
				Check(!Root->HasField(TEXT("winningTeamId")) && !Root->HasField(TEXT("stake")),
					TEXT("the cancel body carries no winner and no stake field"));

				const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
				const bool bHasEntries = Root->TryGetArrayField(TEXT("entries"), Entries) && Entries;
				Check(bHasEntries && Entries->Num() == 2, TEXT("both escrowed players appear as entries"));

				if (bHasEntries)
				{
					// DENSE FROM 1, one position per team. validateRequest rejects a gap outright.
					TSet<int32> Positions;
					int32 Total = 0;
					for (const TSharedPtr<FJsonValue>& V : *Entries)
					{
						const TSharedPtr<FJsonObject> E = V->AsObject();
						Positions.Add(static_cast<int32>(E->GetNumberField(TEXT("finishingPosition"))));
						Total += static_cast<int32>(E->GetNumberField(TEXT("entryAmount")));
					}
					Check(Positions.Num() == 2 && Positions.Contains(1) && Positions.Contains(2),
						TEXT("finishing positions are dense from 1 (one per team)"));
					// EXACTLY the pot that was escrowed -- a refund that mints or shorts is the failure this
					// endpoint's second invariant exists to prevent.
					Check(Total == 20, FString::Printf(TEXT("the refund totals exactly what was escrowed (20), got %d"), Total));
				}
			}
		}

		// ---- 2. R92: a squad shares one position, and its members split that position's one unit ----
		{
			const FAFLEscrowLedger L = MakeLedger2v2();
			FString Json, Err;
			const bool bBuilt = FAFLMatchReporter::BuildCancelBody(L, Json, Err);
			Check(bBuilt, FString::Printf(TEXT("a 2v2 ledger builds a cancel body%s"),
				bBuilt ? TEXT("") : *FString::Printf(TEXT("  [rejected: %s]"), *Err)));

			const TSharedPtr<FJsonObject> Root = bBuilt ? Parse(Json) : nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
			if (Root.IsValid() && Root->TryGetArrayField(TEXT("entries"), Entries) && Entries)
			{
				// UNIFORM STAKE PER POSITION -- 4 players, 2 positions, 10 each. A per-player split against the
				// curve would make a duo stake two units against the other duo's two and pay the wrong shape.
				TMap<int32, int32> TotalByPosition;
				for (const TSharedPtr<FJsonValue>& V : *Entries)
				{
					const TSharedPtr<FJsonObject> E = V->AsObject();
					TotalByPosition.FindOrAdd(static_cast<int32>(E->GetNumberField(TEXT("finishingPosition"))))
						+= static_cast<int32>(E->GetNumberField(TEXT("entryAmount")));
				}
				Check(TotalByPosition.Num() == 2, FString::Printf(TEXT("4 players in 2 squads -> 2 positions, got %d"), TotalByPosition.Num()));
				bool bUniform = true;
				for (const TPair<int32, int32>& P : TotalByPosition)
				{
					if (P.Value != 10) { bUniform = false; }
				}
				Check(bUniform, TEXT("R92: each position holds exactly one 10-unit stake, split across its roster"));
			}
		}

		// ---- 3. the ledger shapes that must NOT reach the wire ----
		{
			FAFLEscrowLedger L = MakeLedger1v1();
			L.Entries.Empty();
			CheckRejects(L, TEXT("nothing was escrowed"), TEXT("an empty ledger is rejected (there is no pot to refund)"));
		}
		{
			FAFLEscrowLedger L = MakeLedger1v1();
			L.MatchId.Invalidate();
			CheckRejects(L, TEXT("no MatchId"), TEXT("a ledger with no MatchId is rejected"));
		}
		{
			FAFLEscrowLedger L = MakeLedger1v1();
			L.CurrencyCode = TEXT("USD");
			CheckRejects(L, TEXT("VO or WA"), TEXT("a non-VO/WA currency is rejected (the pools are SEALED, and NEVER USD)"));
		}
		{
			FAFLEscrowLedger L = MakeLedger1v1();
			L.Entries[1].ReconcileId.Reset();
			CheckRejects(L, TEXT("no reconcile id"), TEXT("an entry with no reconcile id is rejected (it could be refunded to nobody)"));
		}
		{
			FAFLEscrowLedger L = MakeLedger1v1();
			L.Entries[1].Amount = 0;
			CheckRejects(L, TEXT("positive integers only"), TEXT("a zero-amount entry is rejected (it never funded the pot)"));
		}
		{
			FAFLEscrowLedger L = MakeLedger1v1();
			L.Entries[1].TeamId = INDEX_NONE;
			CheckRejects(L, TEXT("no team"), TEXT("an entry with no team is rejected (there is no position to place it in)"));
		}
		{
			// The one the backend would otherwise reject with a message pointing at the payout curve rather
			// than at the ledger that is actually wrong.
			FAFLEscrowLedger L = MakeLedger1v1();
			L.Entries[1].Amount = 7;
			CheckRejects(L, TEXT("unequal totals"), TEXT("teams staking unequal totals are rejected HERE, not left to the backend"));
		}

		// ---- 4. an UNCONFIRMED escrow still builds. The full plan is sent and the ledger arbitrates --
		//         dropping the entry would either short the refund or hide a debit that did land. ----
		{
			FAFLEscrowLedger L = MakeLedger1v1();
			L.Entries[1].bConfirmed = false;
			FString Json, Err;
			const bool bBuilt = FAFLMatchReporter::BuildCancelBody(L, Json, Err);
			Check(bBuilt, FString::Printf(TEXT("an unconfirmed escrow entry still builds the full plan%s"),
				bBuilt ? TEXT("") : *FString::Printf(TEXT("  [rejected: %s]"), *Err)));
			Check(L.ConfirmedCount() == 1, FString::Printf(TEXT("ConfirmedCount reports 1 of 2, got %d"), L.ConfirmedCount()));
		}

		// ---- 5. IsStaked: the test the cancel path uses to decide there is anything to refund at all ----
		{
			FAFLEscrowLedger L = MakeLedger1v1();
			Check(L.IsStaked(), TEXT("a funded ledger reads as staked"));
			L.StakePerPosition = 0;
			Check(!L.IsStaked(), TEXT("a ledger with no stake reads as unstaked (LEAGUE PLAY refunds nothing)"));
		}

		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_CANCELTEST: DONE -- %d passed, %d failed. %s"),
			Pass, Fail, (Fail == 0) ? TEXT("ALL GREEN") : TEXT("*** FAILURES ***"));
	}

	FAutoConsoleCommand GAFLCancelBodyTestCmd(
		TEXT("afl.Match.Cancel.Test"),
		TEXT("Acceptance for FAFLMatchReporter::BuildCancelBody -- proves an abandoned or stalemated match emits a 'cancelled-refund' settlement whose positions are dense, whose per-position stake is uniform, and whose entry amounts are exactly what was escrowed."),
		FConsoleCommandDelegate::CreateStatic(&RunCancelBodyTest));
}

#endif // !UE_BUILD_SHIPPING
