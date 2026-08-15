// Copyright C12 AI Gaming. All Rights Reserved.

#include "Match/AFLMatchReporter.h"

#include "AFLGameCore.h"                 // LogAFLGameCore
#include "AFLOnlineSubsystem.h"          // the signed server transport (server-only key)
#include "Teams/AFLReconcileIdComponent.h"   // the per-player reconcile key the matchmaker roster matched on
#include "Teams/AFLMatchmakerDataProvider.h" // S12: ResolveAuthoritativeMatchmakerData -- the single payload source
#include "Match/AFLMatchOutcomeComponent.h"  // replicated carrier for payout / rating -> the results board
#include "Engine/Engine.h"                   // GEngine->GetWorldFromContextObject
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Guid.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameModeBase.h"     // OptionsString
#include "GameFramework/PlayerState.h"
#include "GenericTeamAgentInterface.h"      // IGenericTeamAgentInterface -> the assigned team
#include "Kismet/GameplayStatics.h"         // ParseOption

// Forward-declared: the escrow path records the STAKE and sits above these, while the definitions live
// beside the settle/rating publishers they belong with. Declaring rather than hoisting keeps the three
// outcome helpers together as one readable unit instead of splitting them around the file.
namespace
{
	APlayerState* ResolveByReconcileId(const UObject* WorldContext, const FString& PlayFabId);
	void PublishSettleOutcome(const UObject* WorldContext, const FString& Response);
	void PublishRatingOutcome(const UObject* WorldContext, const FString& Response);
}

// NOTE (2026-08-08): a dev cvar `afl.Match.DevOptions` briefly lived here as a stand-in for the launch-line
// options an in-process PIE cannot supply. It was REMOVED deliberately. It fabricated the stake -- the exact
// thing a staked-match test is supposed to prove -- so a green PIE run demonstrated only that the fake agreed
// with itself. Worse, it was a third source of truth in a currency-integrity path.
//
// The economy round-trip is verified on a DEDICATED SERVER fed real allocator MatchmakerData, with clients
// carrying a real ?PlayFabId=. If you are here because a PIE match logs "LEAGUE PLAY ... nothing to escrow":
// that is correct behaviour, not a bug. PIE cannot stake a match, by design.

namespace
{
	FString SerializeObject(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}
}

FString FAFLMatchReporter::MatchIdToWire(const FGuid& MatchId)
{
	// The SAME spelling UAFLRoundManagerComponent::GetMatchId() already emits. Escrow, settlement and rating
	// all join on this string; two formats would silently become two matches.
	return MatchId.ToString(EGuidFormats::DigitsWithHyphens);
}

FString FAFLMatchReporter::RulesetToWire(EAFLRuleset Ruleset)
{
	return Ruleset == EAFLRuleset::BattleRoyale ? TEXT("BattleRoyale") : TEXT("MatchPlay");
}

FAFLMatchReporter::FMatchEconomics FAFLMatchReporter::ReadEconomics(const UObject* WorldContext)
{
	FMatchEconomics Econ;

	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	const AGameModeBase* GameMode = GS ? GS->AuthorityGameMode : nullptr;
	if (!GameMode)
	{
		// No authority game mode -> a client, or a world without one. Defaults mean UNSTAKED and UNRATED,
		// which is the safe direction: a misread never invents a stake.
		return Econ;
	}
	const FString& Options = GameMode->OptionsString;

	// --- PREFERRED SOURCE: the matchmaker payload. ---
	//
	// `economics` is emitted by the match-allocator, which lifts it off the TICKET ATTRIBUTES and has already
	// verified that every member of the match agreed on it. That verification is the reason this outranks the
	// launch line: the ticket is what actually matched the players, whereas the launch line is whatever the
	// process was started with. A staked match must be worth what the players queued for, not what the
	// command line says.
	// S12: the ONE resolver -- GameLift's delivered payload if it arrived, else the launch option. Reading
	// OptionsString directly here silently produced LEAGUE PLAY under GameLift even while the roster was
	// delivered correctly, because the economics live in the same payload but were being read from a
	// different place.
	const FString MatchmakerData = UAFLMatchmakerDataProvider::ResolveAuthoritativeMatchmakerData(WorldContext);
	if (!MatchmakerData.IsEmpty())
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MatchmakerData);
		const TSharedPtr<FJsonObject>* EconObj = nullptr;
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()
			&& Root->TryGetObjectField(TEXT("economics"), EconObj) && EconObj)
		{
			FString TierStr, LeagueStr, CurrencyStr;
			double StakeNum = 0.0;
			(*EconObj)->TryGetStringField(TEXT("tier"), TierStr);
			(*EconObj)->TryGetStringField(TEXT("league"), LeagueStr);
			(*EconObj)->TryGetStringField(TEXT("currency"), CurrencyStr);
			(*EconObj)->TryGetNumberField(TEXT("stake"), StakeNum);

			if (TierStr.Equals(TEXT("WattsPlay"), ESearchCase::IgnoreCase))      { Econ.Tier = EAFLPlayTier::WattsPlay; }
			else if (TierStr.Equals(TEXT("VoltsPlay"), ESearchCase::IgnoreCase)) { Econ.Tier = EAFLPlayTier::VoltsPlay; }
			else                                                                  { Econ.Tier = EAFLPlayTier::LeaguePlay; }
			Econ.League = LeagueStr.Equals(TEXT("Haywire"), ESearchCase::IgnoreCase) ? EAFLLeague::Haywire : EAFLLeague::ProMod;
			Econ.StakePerPosition = FMath::Max(0, FMath::RoundToInt(StakeNum));
			Econ.CurrencyCode = CurrencyStr.Equals(TEXT("WA"), ESearchCase::IgnoreCase) ? TEXT("WA") : TEXT("VO");

			UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: economics from MATCHMAKER -- tier=%s league=%s stake=%d %s"),
				*TierStr, *LeagueStr, Econ.StakePerPosition, *Econ.CurrencyCode);
			return Econ;
		}

		// A roster with no economics block is an OLDER allocator, not a corrupt payload -- fall through to
		// the launch line rather than forcing the match unstaked, so a mixed-version deploy still works.
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_MATCHREPORT: MatchmakerData carries no 'economics' block (pre-stake allocator?) -- falling back to launch options."));
	}

	// --- FALLBACK: launch options. Correct for local/dedicated runs with no matchmaker in front. ---
	const FString TierOpt = UGameplayStatics::ParseOption(Options, TEXT("Tier"));
	if (TierOpt.Equals(TEXT("WattsPlay"), ESearchCase::IgnoreCase))      { Econ.Tier = EAFLPlayTier::WattsPlay; }
	else if (TierOpt.Equals(TEXT("VoltsPlay"), ESearchCase::IgnoreCase)) { Econ.Tier = EAFLPlayTier::VoltsPlay; }
	else                                                                 { Econ.Tier = EAFLPlayTier::LeaguePlay; }

	const FString LeagueOpt = UGameplayStatics::ParseOption(Options, TEXT("League"));
	Econ.League = LeagueOpt.Equals(TEXT("Haywire"), ESearchCase::IgnoreCase) ? EAFLLeague::Haywire : EAFLLeague::ProMod;

	Econ.StakePerPosition = FCString::Atoi(*UGameplayStatics::ParseOption(Options, TEXT("Stake")));

	const FString CurrencyOpt = UGameplayStatics::ParseOption(Options, TEXT("StakeCurrency"));
	Econ.CurrencyCode = CurrencyOpt.Equals(TEXT("WA"), ESearchCase::IgnoreCase) ? TEXT("WA") : TEXT("VO");

	// R86: staked play is PRO MOD only. A launch line asking for a staked Haywire match is a configuration
	// error, and the result would be rejected by Validate() anyway -- refuse the STAKE here so the match runs
	// unstaked rather than dying at settlement, and say so loudly.
	if (Econ.IsStaked() && Econ.League != EAFLLeague::ProMod)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_MATCHREPORT: ?Tier=%s with ?League=Haywire is invalid -- staked play is PRO MOD ONLY (R86). Falling back to LEAGUE PLAY (unstaked)."),
			*TierOpt);
		Econ.Tier = EAFLPlayTier::LeaguePlay;
		Econ.StakePerPosition = 0;
	}
	return Econ;
}

bool FAFLMatchReporter::BuildTeamSeriesResult(const UObject* WorldContext, const FGuid& MatchId, int32 WinningTeamId,
	const FMatchEconomics& Economics, FAFLMatchResult& OutResult, FString& OutError)
{
	OutResult = FAFLMatchResult();
	OutError.Reset();

	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		OutError = TEXT("no game state -- cannot enumerate participants");
		return false;
	}

	OutResult.MatchId = MatchId;
	OutResult.Ruleset = EAFLRuleset::MatchPlay;
	OutResult.League  = Economics.League;
	OutResult.Tier    = Economics.Tier;
	OutResult.bStaked = Economics.IsStaked();
	// R85: the staked tiers are rated and LEAGUE PLAY is not. Derived from the tier rather than configured
	// separately, so the two cannot be set inconsistently from a launch line.
	OutResult.bRanked = Economics.IsStaked();
	OutResult.WinningTeamId = WinningTeamId;

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }

		FAFLMatchParticipant P;
		P.bIsBot = PS->IsABot();

		if (const UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>())
		{
			P.ReconcileId = IdComp->GetReconcileId();
		}
		// A BOT must carry NO id and a HUMAN must carry one -- Validate() enforces both. Clearing a bot's id
		// here rather than trusting it to be empty means a stray stashed id cannot resolve to a real account
		// at settlement.
		if (P.bIsBot)
		{
			P.ReconcileId.Reset();
		}
		else if (P.ReconcileId.IsEmpty())
		{
			OutError = FString::Printf(TEXT("human player '%s' has no reconcile id -- they could be neither paid nor rated"), *PS->GetPlayerName());
			return false;
		}

		int32 TeamId = INDEX_NONE;
		if (const IGenericTeamAgentInterface* Agent = Cast<IGenericTeamAgentInterface>(PS))
		{
			const FGenericTeamId Assigned = Agent->GetGenericTeamId();
			if (Assigned != FGenericTeamId::NoTeam)
			{
				TeamId = Assigned.GetId();
			}
		}
		if (TeamId == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("player '%s' has no team -- a two-team series cannot place them"), *PS->GetPlayerName());
			return false;
		}
		P.TeamId = TeamId;

		// THE WHOLE OF "MATCH PLAY RESOLVES OVER EXACTLY 2": winner is 1, everyone else is 2. No scaling, no
		// per-player placement, and therefore winner-takes-all falls out of ceil(0.15 x 2) = 1 downstream.
		P.FinishingPosition = (TeamId == WinningTeamId) ? 1 : 2;

		OutResult.Participants.Add(MoveTemp(P));
	}

	if (OutResult.Participants.Num() == 0)
	{
		OutError = TEXT("no participants in PlayerArray at match end");
		return false;
	}
	return true;
}

bool FAFLMatchReporter::BuildFieldResult(const UObject* WorldContext, const FGuid& MatchId,
	const TMap<TWeakObjectPtr<APlayerState>, int32>& Placements,
	const FMatchEconomics& Economics, FAFLMatchResult& OutResult, FString& OutError)
{
	OutResult = FAFLMatchResult();
	OutError.Reset();

	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		OutError = TEXT("no game state -- cannot enumerate participants");
		return false;
	}

	OutResult.MatchId = MatchId;
	OutResult.Ruleset = EAFLRuleset::BattleRoyale;
	OutResult.League  = Economics.League;
	OutResult.Tier    = Economics.Tier;
	OutResult.bStaked = Economics.IsStaked();
	OutResult.bRanked = Economics.IsStaked();   // R85, same derivation as the team-series builder
	// PLACEMENT IS THE RESULT. Validate() REFUSES a battle royale carrying a winner, because a winner field
	// would be a second source of truth that can disagree with position 1.
	OutResult.WinningTeamId = INDEX_NONE;

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }

		FAFLMatchParticipant P;
		P.bIsBot = PS->IsABot();

		if (const UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>())
		{
			P.ReconcileId = IdComp->GetReconcileId();
		}
		if (P.bIsBot)
		{
			P.ReconcileId.Reset();
		}
		else if (P.ReconcileId.IsEmpty())
		{
			OutError = FString::Printf(TEXT("human player '%s' has no reconcile id -- they could be neither paid nor rated"), *PS->GetPlayerName());
			return false;
		}

		// OPERATOR RULING: INDEX_NONE, not the live 1..N id. A free-for-all has no teams ECONOMICALLY even
		// though the runtime assigns them for spawning and damage, and writing the live ids would both claim a
		// structure the mode does not have and make a future squad BR indistinguishable from solo.
		//
		// ⚠ THIS IS ALSO WHAT SELECTS THE RATING PATH. groupIntoUnits keys on `teamId >= 0 ? team : solo`, so
		// INDEX_NONE takes the SOLO branch and every player is rated as their own unit. Live ids would silently
		// take the TEAM branch -- which happens to produce the same units today only because BR seats one
		// player per team, and would stop being true the moment anything shares a team.
		P.TeamId = INDEX_NONE;

		const int32* Found = Placements.Find(PS);
		if (!Found)
		{
			// REFUSED, NOT DEFAULTED. A participant with no placement is a hole in the position sequence, and
			// this is the exact shape of the defect the arrival gate fixed -- a player seated in the match and
			// never placed. Validate() would reject the result one step later with a less useful message.
			OutError = FString::Printf(
				TEXT("participant '%s' has no finishing placement -- every player in the match must hold one, or the positions are not dense"),
				*PS->GetPlayerName());
			return false;
		}
		P.FinishingPosition = *Found;

		OutResult.Participants.Add(MoveTemp(P));
	}

	if (OutResult.Participants.Num() == 0)
	{
		OutError = TEXT("no participants in PlayerArray at match end");
		return false;
	}
	return true;
}

FString FAFLMatchReporter::BuildEscrowBody(const FGuid& MatchId, const FString& ReconcileId,
	const FString& CurrencyCode, int32 Amount)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("matchId"), MatchIdToWire(MatchId));
	Body->SetStringField(TEXT("playFabId"), ReconcileId);
	Body->SetStringField(TEXT("currencyCode"), CurrencyCode);
	Body->SetNumberField(TEXT("amount"), Amount);
	return SerializeObject(Body);
}

bool FAFLMatchReporter::BuildSettleBody(const FAFLMatchResult& Result, int32 StakeAmountPerPosition,
	const FString& CurrencyCode, const FString& TerminalState, FString& OutJson, FString& OutError)
{
	OutJson.Reset();
	OutError.Reset();

	if (!Result.bStaked)
	{
		OutError = TEXT("match is not staked -- there is nothing to settle");
		return false;
	}
	if (StakeAmountPerPosition <= 0)
	{
		OutError = FString::Printf(TEXT("stake per position must be positive, got %d"), StakeAmountPerPosition);
		return false;
	}
	if (CurrencyCode != TEXT("VO") && CurrencyCode != TEXT("WA"))
	{
		OutError = FString::Printf(TEXT("currency must be VO or WA (the pools are SEALED), got '%s'"), *CurrencyCode);
		return false;
	}

	// Per-position roster sizes, so a squad's members each carry an equal share of the position's one unit.
	// The backend requires every POSITION to have staked the same total; splitting here is what makes that
	// true for squads without the caller having to reason about it.
	TMap<int32, int32> RosterSize;
	for (const FAFLMatchParticipant& P : Result.Participants)
	{
		RosterSize.FindOrAdd(P.FinishingPosition)++;
	}
	for (const TPair<int32, int32>& Pair : RosterSize)
	{
		if (StakeAmountPerPosition % Pair.Value != 0)
		{
			// Refuse rather than round. An uneven split would make the position totals differ, and the
			// backend would (correctly) reject the whole settlement with a less obvious message.
			OutError = FString::Printf(
				TEXT("stake %d does not divide evenly across the %d-member roster at position %d -- position totals would differ"),
				StakeAmountPerPosition, Pair.Value, Pair.Key);
			return false;
		}
	}

	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FAFLMatchParticipant& P : Result.Participants)
	{
		const TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
		E->SetStringField(TEXT("playFabId"), P.ReconcileId);
		E->SetNumberField(TEXT("finishingPosition"), P.FinishingPosition);
		E->SetNumberField(TEXT("entryAmount"), StakeAmountPerPosition / RosterSize[P.FinishingPosition]);
		Entries.Add(MakeShared<FJsonValueObject>(E));
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("matchId"), MatchIdToWire(Result.MatchId));
	Body->SetStringField(TEXT("currencyCode"), CurrencyCode);
	Body->SetStringField(TEXT("terminalState"), TerminalState);
	Body->SetArrayField(TEXT("entries"), Entries);
	OutJson = SerializeObject(Body);
	return true;
}

bool FAFLMatchReporter::BuildCancelBody(const FAFLEscrowLedger& Ledger, FString& OutJson, FString& OutError)
{
	OutJson.Reset();
	OutError.Reset();

	if (!Ledger.MatchId.IsValid())
	{
		OutError = TEXT("the ledger carries no MatchId -- a refund keys off it, so an unset id refunds nothing");
		return false;
	}
	if (Ledger.CurrencyCode != TEXT("VO") && Ledger.CurrencyCode != TEXT("WA"))
	{
		OutError = FString::Printf(TEXT("currency must be VO or WA (the pools are SEALED), got '%s'"), *Ledger.CurrencyCode);
		return false;
	}
	if (Ledger.Entries.Num() == 0)
	{
		// Not a failure at the call site -- it means nothing was ever taken. The caller logs it as such.
		OutError = TEXT("the ledger is empty -- nothing was escrowed for this match, so there is nothing to refund");
		return false;
	}

	// --- one finishing position per TEAM, ascending by team id. ---
	//
	// Deterministic ordering matters only because a body should be reproducible when read back out of a log;
	// the positions themselves are inert under 'cancelled-refund' (see the header). Density from 1 falls out
	// of indexing the sorted team list, which is the property `validateRequest` actually requires.
	TArray<int32> TeamIds;
	for (const FAFLEscrowedEntry& E : Ledger.Entries)
	{
		if (E.ReconcileId.IsEmpty())
		{
			OutError = TEXT("a ledger entry has no reconcile id -- that entry could not be refunded to anyone");
			return false;
		}
		if (E.Amount <= 0)
		{
			OutError = FString::Printf(
				TEXT("ledger entry for %s has amount %d -- currency is positive integers only (E1), and a zero entry never funded the pot"),
				*E.ReconcileId, E.Amount);
			return false;
		}
		if (E.TeamId == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("ledger entry for %s has no team -- there is no position to place it in"), *E.ReconcileId);
			return false;
		}
		TeamIds.AddUnique(E.TeamId);
	}
	TeamIds.Sort();

	// UNIFORM STAKE PER POSITION, checked HERE rather than left to the backend. EscrowTeamSeries already
	// guarantees it by construction, so a violation means the ledger was built somewhere else or mutated after
	// the fact -- and the backend's rejection ("positions staked unequal amounts") would send whoever reads it
	// hunting through the payout curve for a fault that is actually in the ledger.
	TMap<int32, int32> TotalByTeam;
	for (const FAFLEscrowedEntry& E : Ledger.Entries)
	{
		TotalByTeam.FindOrAdd(E.TeamId) += E.Amount;
	}
	for (const TPair<int32, int32>& Team : TotalByTeam)
	{
		if (Team.Value != TotalByTeam[TeamIds[0]])
		{
			OutError = FString::Printf(
				TEXT("ledger teams staked unequal totals (team %d staked %d, team %d staked %d) -- every finishing position must hold one stake unit"),
				TeamIds[0], TotalByTeam[TeamIds[0]], Team.Key, Team.Value);
			return false;
		}
	}

	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FAFLEscrowedEntry& E : Ledger.Entries)
	{
		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("playFabId"), E.ReconcileId);
		Obj->SetNumberField(TEXT("finishingPosition"), TeamIds.IndexOfByKey(E.TeamId) + 1);   // 1-based, dense
		// EXACTLY what was debited. `computePayouts` hands this straight back for 'cancelled-refund', and
		// `verifyAgainstEscrow` compares it against the row we wrote -- so any adjustment here is either a
		// mint or a short refund, never a correction.
		Obj->SetNumberField(TEXT("entryAmount"), E.Amount);
		Entries.Add(MakeShared<FJsonValueObject>(Obj));
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("matchId"), MatchIdToWire(Ledger.MatchId));
	Body->SetStringField(TEXT("currencyCode"), Ledger.CurrencyCode);
	Body->SetStringField(TEXT("terminalState"), TEXT("cancelled-refund"));
	Body->SetArrayField(TEXT("entries"), Entries);
	OutJson = SerializeObject(Body);
	return true;
}

bool FAFLMatchReporter::BuildRatingBody(const FAFLMatchResult& Result, FString& OutJson, FString& OutError)
{
	OutJson.Reset();
	OutError.Reset();

	if (!Result.bRanked)
	{
		OutError = TEXT("match is not rated (LEAGUE PLAY is unrated, R87) -- there is no ladder to move");
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Participants;
	for (const FAFLMatchParticipant& P : Result.Participants)
	{
		if (P.bIsBot)
		{
			// Should be unreachable: Validate() bars bots from rated matches. Filtered anyway, because
			// sending one would be rejected by the endpoint and take the whole rated match down with it.
			continue;
		}
		const TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
		E->SetStringField(TEXT("playFabId"), P.ReconcileId);
		E->SetNumberField(TEXT("teamId"), P.TeamId);
		E->SetNumberField(TEXT("finishingPosition"), P.FinishingPosition);
		Participants.Add(MakeShared<FJsonValueObject>(E));
	}

	if (Participants.Num() < 2)
	{
		OutError = FString::Printf(TEXT("only %d rateable (non-bot) participant(s) -- there is no outcome to rate"), Participants.Num());
		return false;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("matchId"), MatchIdToWire(Result.MatchId));
	Body->SetStringField(TEXT("ruleset"), RulesetToWire(Result.Ruleset));
	Body->SetBoolField(TEXT("ranked"), true);
	Body->SetArrayField(TEXT("participants"), Participants);
	// NO currencyCode, NO stake, NO entryAmount. The endpoint rejects them (§10.1). See the header.
	OutJson = SerializeObject(Body);
	return true;
}

bool FAFLMatchReporter::ValidateFreeForAllEscrow(int32 HumanCount, int32 BotCount, int32 StakePerPosition, FString& OutError)
{
	OutError.Reset();

	// R85: the staked tiers permit no bots. One bot means the pot is short by a whole share and the match can
	// never settle -- refuse before charging anyone rather than discover it at payout. This is the FIRST check
	// because it is the one that must never be reachable past this point.
	if (BotCount > 0)
	{
		OutError = FString::Printf(
			TEXT("%d bot(s) in a STAKED free-for-all -- bots are barred from staked play (R85). Nobody debited."),
			BotCount);
		return false;
	}
	if (HumanCount < 2)
	{
		OutError = FString::Printf(
			TEXT("staked free-for-all has %d human player(s) -- a contest needs at least 2. Nobody debited."),
			HumanCount);
		return false;
	}
	if (StakePerPosition <= 0)
	{
		OutError = FString::Printf(
			TEXT("stake per position is %d -- currency is positive integers only (E1)."), StakePerPosition);
		return false;
	}
	// NO DIVISIBILITY CHECK, and its ABSENCE is the whole difference from EscrowTeamSeries. A solo player funds
	// a whole unit alone, so nothing is divided. Squad BR (R92) would make a position a squad again and would
	// need `StakePerPosition % Size` back -- see the header.
	return true;
}

TSharedPtr<FAFLEscrowLedger> FAFLMatchReporter::EscrowFreeForAll(const UObject* WorldContext, const FGuid& MatchId,
	const FMatchEconomics& Economics)
{
	const FString Wire = MatchIdToWire(MatchId);

	// Every early return yields a NULL ledger, and that reads correctly in each case: nothing was debited, so
	// there is no pot, so a later cancellation has nothing to refund.
	if (!Economics.IsStaked())
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: LEAGUE PLAY match %s -- no buy-in, nothing to escrow (R85)."), *Wire);
		return nullptr;
	}

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(WorldContext);
	if (!Online || !Online->IsMatchReportingConfigured())
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: economy not wired -- match %s not escrowed."), *Wire);
		return nullptr;
	}

	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: no game state -- match %s NOT escrowed."), *Wire);
		return nullptr;
	}

	// --- gather. NOTHING is debited until every check below passes. ---
	struct FSoloEntry { FString ReconcileId; int32 TeamId = INDEX_NONE; };
	TArray<FSoloEntry> Humans;
	int32 BotCount = 0;

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		if (PS->IsABot()) { ++BotCount; continue; }

		FSoloEntry Entry;
		if (const UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>())
		{
			Entry.ReconcileId = IdComp->GetReconcileId();
		}
		if (Entry.ReconcileId.IsEmpty())
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_MATCHREPORT: player '%s' has no reconcile id -- match %s NOT escrowed (nobody debited)."),
				*PS->GetPlayerName(), *Wire);
			return nullptr;
		}

		// THE LEDGER'S TeamId, WHICH IS A POSITION-GROUP KEY AND NOT A CLAIM ABOUT TEAMS. BuildCancelBody
		// groups by it to synthesise finishing positions and REFUSES INDEX_NONE outright, so a solo entry must
		// carry something distinct. The live runtime id is exactly that -- BR seats every player on their own
		// team. The RESULT struct still carries INDEX_NONE, per the ruling; these are two fields with two jobs.
		if (const IGenericTeamAgentInterface* Agent = Cast<IGenericTeamAgentInterface>(PS))
		{
			const FGenericTeamId Assigned = Agent->GetGenericTeamId();
			if (Assigned != FGenericTeamId::NoTeam) { Entry.TeamId = Assigned.GetId(); }
		}
		if (Entry.TeamId == INDEX_NONE)
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_MATCHREPORT: player '%s' has no runtime team id -- the refund body could place no position for them. Match %s NOT escrowed (nobody debited)."),
				*PS->GetPlayerName(), *Wire);
			return nullptr;
		}
		Humans.Add(MoveTemp(Entry));
	}

	FString ValidateError;
	if (!ValidateFreeForAllEscrow(Humans.Num(), BotCount, Economics.StakePerPosition, ValidateError))
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: match %s NOT escrowed -- %s"), *Wire, *ValidateError);
		return nullptr;
	}

	// Two players on the same runtime team would be a squad, and a squad splits one unit (R92). Refuse rather
	// than debit each of them a whole unit, which would make their position total differ from everyone else and
	// settlement would refuse the match after the money moved.
	TSet<int32> DistinctTeams;
	for (const FSoloEntry& E : Humans) { DistinctTeams.Add(E.TeamId); }
	if (DistinctTeams.Num() != Humans.Num())
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_MATCHREPORT: %d human(s) hold only %d distinct team id(s) in free-for-all match %s -- two players sharing a position is a SQUAD (R92), which this path does not fund. NOT escrowed (nobody debited)."),
			Humans.Num(), DistinctTeams.Num(), *Wire);
		return nullptr;
	}

	// --- every check passed: debit. One whole unit each. ---
	const TSharedRef<FAFLEscrowLedger> Ledger = MakeShared<FAFLEscrowLedger>();
	Ledger->MatchId = MatchId;
	Ledger->CurrencyCode = Economics.CurrencyCode;
	Ledger->StakePerPosition = Economics.StakePerPosition;

	for (const FSoloEntry& Human : Humans)
	{
		FAFLEscrowedEntry Entry;
		Entry.ReconcileId = Human.ReconcileId;
		Entry.TeamId = Human.TeamId;
		Entry.Amount = Economics.StakePerPosition;
		const int32 EntryIndex = Ledger->Entries.Add(Entry);

		const FString Body = BuildEscrowBody(MatchId, Human.ReconcileId, Economics.CurrencyCode, Economics.StakePerPosition);
		const FString Id = Human.ReconcileId;
		const int32 Share = Economics.StakePerPosition;
		TWeakObjectPtr<const UObject> WeakContext(WorldContext);
		Online->PostServerEscrow(Body, [Wire, Id, Share, Ledger, EntryIndex, WeakContext](bool bOk, const FString& Response)
		{
			if (Ledger->Entries.IsValidIndex(EntryIndex))
			{
				Ledger->Entries[EntryIndex].bConfirmed = bOk;
			}
			if (bOk)
			{
				UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: escrowed %d from %s for match %s"), Share, *Id, *Wire);
				if (WeakContext.IsValid())
				{
					if (UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::EnsureOn(WeakContext.Get()))
					{
						if (APlayerState* PS = ResolveByReconcileId(WeakContext.Get(), Id))
						{
							Outcome->ServerRecordStake(PS, Share);
						}
					}
				}
			}
			else
			{
				UE_LOG(LogAFLGameCore, Error,
					TEXT("AFL_MATCHREPORT: ESCROW FAILED for %s in match %s -- settlement will refuse this match: %s"),
					*Id, *Wire, *Response.Left(300));
			}
		});
	}

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFL_MATCHREPORT: free-for-all match %s escrowed -- %d player(s) x %d %s = %d pot."),
		*Wire, Ledger->Entries.Num(), Economics.StakePerPosition, *Economics.CurrencyCode,
		Ledger->Entries.Num() * Economics.StakePerPosition);
	return Ledger;
}

TSharedPtr<FAFLEscrowLedger> FAFLMatchReporter::EscrowTeamSeries(const UObject* WorldContext, const FGuid& MatchId,
	const FMatchEconomics& Economics)
{
	const FString Wire = MatchIdToWire(MatchId);

	// EVERY early return below yields a NULL ledger, and that is the correct reading of each of them: nothing
	// was debited, so there is no pot, so a later cancellation has nothing to refund. A caller that receives
	// null and later cancels correctly does nothing rather than posting an empty settlement.
	if (!Economics.IsStaked())
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: LEAGUE PLAY match %s -- no buy-in, nothing to escrow (R85)."), *Wire);
		return nullptr;
	}

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(WorldContext);
	if (!Online || !Online->IsMatchReportingConfigured())
	{
		// Expected in a plain PIE session: the env vars are absent by design. One line, not one per player.
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: economy not wired -- match %s not escrowed."), *Wire);
		return nullptr;
	}

	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: no game state -- match %s NOT escrowed."), *Wire);
		return nullptr;
	}

	// --- gather, grouped by team. NOTHING is debited until every check below passes. ---
	TMap<int32, TArray<FString>> HumansByTeam;
	int32 BotCount = 0;

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		if (PS->IsABot()) { ++BotCount; continue; }

		FString ReconcileId;
		if (const UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>())
		{
			ReconcileId = IdComp->GetReconcileId();
		}
		if (ReconcileId.IsEmpty())
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_MATCHREPORT: player '%s' has no reconcile id -- match %s NOT escrowed (nobody debited)."),
				*PS->GetPlayerName(), *Wire);
			return nullptr;
		}

		int32 TeamId = INDEX_NONE;
		if (const IGenericTeamAgentInterface* Agent = Cast<IGenericTeamAgentInterface>(PS))
		{
			const FGenericTeamId Assigned = Agent->GetGenericTeamId();
			if (Assigned != FGenericTeamId::NoTeam) { TeamId = Assigned.GetId(); }
		}
		if (TeamId == INDEX_NONE)
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_MATCHREPORT: player '%s' has no team -- match %s NOT escrowed (nobody debited)."),
				*PS->GetPlayerName(), *Wire);
			return nullptr;
		}
		HumansByTeam.FindOrAdd(TeamId).Add(MoveTemp(ReconcileId));
	}

	// R85: the staked tiers permit no bots. One here means the pot would be short by a whole share, so the
	// match cannot settle -- refuse before charging anyone rather than discover it at payout.
	if (BotCount > 0)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_MATCHREPORT: %d bot(s) in STAKED match %s -- bots are barred from staked play (R85). NOT escrowed (nobody debited)."),
			BotCount, *Wire);
		return nullptr;
	}
	if (HumansByTeam.Num() < 2)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_MATCHREPORT: staked match %s has %d team(s) with players -- need 2. NOT escrowed (nobody debited)."),
			*Wire, HumansByTeam.Num());
		return nullptr;
	}

	// Per-team share. Validate EVERY team before debiting ANY player.
	TMap<int32, int32> ShareByTeam;
	for (const TPair<int32, TArray<FString>>& Team : HumansByTeam)
	{
		const int32 Size = Team.Value.Num();
		if (Size == 0 || Economics.StakePerPosition % Size != 0)
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_MATCHREPORT: stake %d does not divide evenly across team %d's %d player(s) -- position totals would differ. Match %s NOT escrowed (nobody debited)."),
				Economics.StakePerPosition, Team.Key, Size, *Wire);
			return nullptr;
		}
		ShareByTeam.Add(Team.Key, Economics.StakePerPosition / Size);
	}

	// --- every check passed: debit. ---
	//
	// The ledger is built HERE, from the same loop that posts the debits, so it cannot drift from what was
	// actually asked for. It is shared (not copied) into each completion callback: those land many frames
	// later, on the game thread, and write their outcome back into the entry they debited.
	const TSharedRef<FAFLEscrowLedger> Ledger = MakeShared<FAFLEscrowLedger>();
	Ledger->MatchId = MatchId;
	Ledger->CurrencyCode = Economics.CurrencyCode;
	Ledger->StakePerPosition = Economics.StakePerPosition;

	int32 Posted = 0;
	for (const TPair<int32, TArray<FString>>& Team : HumansByTeam)
	{
		const int32 Share = ShareByTeam[Team.Key];
		for (const FString& Id : Team.Value)
		{
			FAFLEscrowedEntry Entry;
			Entry.ReconcileId = Id;
			Entry.TeamId = Team.Key;
			Entry.Amount = Share;
			const int32 EntryIndex = Ledger->Entries.Add(MoveTemp(Entry));

			const FString Body = BuildEscrowBody(MatchId, Id, Economics.CurrencyCode, Share);
			// Weak, not raw: this completion can outlive the world it was issued from (a player leaving, or a
			// match torn down while an escrow is still in flight).
			TWeakObjectPtr<const UObject> WeakContext(WorldContext);
			Online->PostServerEscrow(Body, [Wire, Id, Share, Ledger, EntryIndex, WeakContext](bool bOk, const FString& Response)
			{
				// Index rather than a pointer: Entries is fully populated by the loop above before any of these
				// callbacks can run (the HTTP completions are delivered on the game thread, from the HTTP
				// manager's tick), so the index is stable and the array is never reallocated under a callback.
				if (Ledger->Entries.IsValidIndex(EntryIndex))
				{
					Ledger->Entries[EntryIndex].bConfirmed = bOk;
				}

				if (bOk)
				{
					UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: escrowed %d from %s for match %s"), Share, *Id, *Wire);

					// Publish the STAKE now, at escrow time, rather than waiting for settlement. It is known
					// and true the moment the debit confirms, and recording it here means the results board
					// can state what the match was played for even if settlement later fails or is slow --
					// which is precisely the case where a player most wants to know what they put in.
					if (WeakContext.IsValid())
					{
						if (UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::EnsureOn(WeakContext.Get()))
						{
							if (APlayerState* PS = ResolveByReconcileId(WeakContext.Get(), Id))
							{
								Outcome->ServerRecordStake(PS, Share);
							}
						}
					}
				}
				else
				{
					// This player is NOT funded. Settlement will refuse the whole match on the count/amount
					// check -- loud and safe, rather than quietly paying a pot that is short.
					UE_LOG(LogAFLGameCore, Error,
						TEXT("AFL_MATCHREPORT: ESCROW FAILED for %s in match %s -- settlement will refuse this match: %s"),
						*Id, *Wire, *Response.Left(300));
				}
			});
			++Posted;
		}
	}
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: match %s -- %d escrow request(s) posted (%d %s per position across %d teams)."),
		*Wire, Posted, Economics.StakePerPosition, *Economics.CurrencyCode, HumansByTeam.Num());

	return Ledger;
}

namespace
{
	/**
	 * PlayFab id -> the PlayerState holding it. Server-only, and only valid while the player is still
	 * connected -- which is why the outcome is published the moment the service answers rather than being
	 * reconstructed later from the ledger.
	 */
	APlayerState* ResolveByReconcileId(const UObject* WorldContext, const FString& PlayFabId)
	{
		if (PlayFabId.IsEmpty())
		{
			return nullptr;
		}
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState)
		{
			return nullptr;
		}
		for (APlayerState* PS : GameState->PlayerArray)
		{
			const UAFLReconcileIdComponent* IdComp = PS ? PS->FindComponentByClass<UAFLReconcileIdComponent>() : nullptr;
			if (IdComp && IdComp->GetReconcileId() == PlayFabId)
			{
				return PS;
			}
		}
		return nullptr;
	}

	/** settle-match response -> per-player payouts on the replicated outcome component. */
	void PublishSettleOutcome(const UObject* WorldContext, const FString& Response)
	{
		UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::EnsureOn(WorldContext);
		if (!Outcome)
		{
			return;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_OUTCOME: settle response was not JSON -- board will show no payout."));
			return;
		}

		// A REPLAYED settle carries no payouts array: the match was already claimed and this call moved
		// nothing. Publishing zeroes here would tell every player they were paid nothing, when in fact they
		// were paid earlier (or refunded). Leave the entries unset so the board shows pending, not a lie.
		const TArray<TSharedPtr<FJsonValue>>* Payouts = nullptr;
		if (!Root->TryGetArrayField(TEXT("payouts"), Payouts) || !Payouts)
		{
			UE_LOG(LogAFLGameCore, Log, TEXT("AFL_OUTCOME: settle response carried no payouts (replayed?) -- nothing published."));
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Payouts)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry)
			{
				continue;
			}
			FString PlayFabId;
			int32 Amount = 0;
			(*Entry)->TryGetStringField(TEXT("playFabId"), PlayFabId);
			(*Entry)->TryGetNumberField(TEXT("amount"), Amount);
			if (APlayerState* PS = ResolveByReconcileId(WorldContext, PlayFabId))
			{
				Outcome->ServerRecordPayout(PS, Amount);
			}
		}

		// A player absent from `payouts` lost and was paid nothing -- that IS their result, so record it
		// explicitly. Without this a loser's row would sit on "pending" forever, because the only signal
		// that they were paid zero is their absence from a list.
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		if (const AGameStateBase* GameState = World ? World->GetGameState() : nullptr)
		{
			for (APlayerState* PS : GameState->PlayerArray)
			{
				if (PS && !PS->IsABot() && !PS->IsOnlyASpectator())
				{
					const FAFLPlayerOutcome* Existing = Outcome->FindOutcome(PS);
					if (!Existing || !Existing->bHasSettle)
					{
						Outcome->ServerRecordPayout(PS, 0);
					}
				}
			}
		}
	}

	/** update-rating response -> per-player signed displayDelta. */
	void PublishRatingOutcome(const UObject* WorldContext, const FString& Response)
	{
		UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::EnsureOn(WorldContext);
		if (!Outcome)
		{
			return;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
		if (!Root->TryGetArrayField(TEXT("changes"), Changes) || !Changes)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Changes)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry)
			{
				continue;
			}
			FString PlayFabId;
			double Delta = 0.0;
			(*Entry)->TryGetStringField(TEXT("playFabId"), PlayFabId);
			(*Entry)->TryGetNumberField(TEXT("displayDelta"), Delta);
			if (APlayerState* PS = ResolveByReconcileId(WorldContext, PlayFabId))
			{
				Outcome->ServerRecordRatingDelta(PS, static_cast<float>(Delta));
			}
		}
	}
}

/**
 * ⚠ THERE IS NO UNSTAKED EARN BRANCH HERE, AND ITS ABSENCE IS THE DESIGN. DO NOT ADD ONE.
 *
 * This function has exactly two branches -- `bStaked` -> settle and `bRanked` -> rate -- so a LEAGUE PLAY
 * MatchPlay match passes the config gate, evaluates both conditions to false, and deliberately does nothing.
 * That looks like a gap and is not one. Operator ruling, 2026-08-14:
 *
 *     MATCHPLAY AWARDS NO WATTS. Loot is the entire reward for League elimination. Watts are earned through
 *     EXTRACTION, and through staked settlement. Nothing is missing.
 *
 * WHY THIS COMMENT EXISTS. The absence is indistinguishable from an oversight from inside this file, and it
 * was investigated as one: `bagman-currency-earn` showed 17 lifetime invocations, none of them from a match,
 * while a full evening of 1v1/2v2/8v8 League gates concluded without paying a single Watt. The conclusion
 * looked inescapable -- "the LeaguePlay terminal was never wired" -- and it was wrong. There is no terminal
 * to wire. Somebody will run that same query again and reach for the same fix; this paragraph is the answer
 * they should find first.
 *
 * WHERE WATTS ACTUALLY COME FROM, so the next reader does not have to grep for it:
 *
 *   EXTRACTION   UAFLAG_Extract::HandleChannelComplete -> Wallet->EarnWattsAuthority(Reward, "extraction")
 *                Reward = round(CarriedEnergy * WattsPerEnergy * ExtractMult). Per EXTRACTION, mid-match,
 *                on the ability -- never at match end, and never from here. MatchPlay has no extraction
 *                phase, which is exactly why an elimination match pays nothing.
 *   SETTLEMENT   the bStaked branch below. Pot minus rake, to the finishing positions.
 *
 * A third branch would need an award rule that does not exist -- no placement curve, no kill bounty, no
 * round-win payment, no participation floor -- so anyone adding one is inventing economy design, not
 * repairing an omission. That is a ruling to seek, not a patch to write.
 */
bool FAFLMatchReporter::ReportMatchEnd(const UObject* WorldContext, const FAFLMatchResult& Result,
	int32 StakeAmountPerPosition, const FString& CurrencyCode)
{
	const FString Wire = MatchIdToWire(Result.MatchId);

	// ⚠ EVERY `return false` BELOW MEANS A STAKED POT IS STILL IN ESCROW. They are not tidy early-outs; each
	// one is a caller's refund backstop being told it still has work to do. See the header for the defect that
	// made this a bool.

	// VALIDATE FIRST, ALWAYS. A settlement built from a malformed result is worse than no settlement,
	// because the backend has no way to know the result was malformed -- it would accept it.
	FString ValidationError;
	if (!Result.Validate(ValidationError))
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: REFUSING to report match %s -- invalid result: %s"), *Wire, *ValidationError);
		return false;
	}

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(WorldContext);
	if (!Online)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_MATCHREPORT: no online subsystem -- match %s not reported."), *Wire);
		return false;
	}
	if (!Online->IsMatchReportingConfigured())
	{
		// One clear line rather than three identical per-endpoint skips. Expected in a plain PIE session,
		// where the env vars are absent by design.
		//
		// FALSE, even though this is the ordinary PIE case: nothing was sent. A caller with a pot must still
		// refund it. In practice an unwired economy also escrowed nothing, so its ledger is null and its
		// backstop no-ops -- but that is the ESCROW path's guarantee to make, not an assumption to bake in here.
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: economy not wired (AFL_EARN_HMAC_KEY / AFL_*_URL absent) -- match %s not reported."), *Wire);
		return false;
	}

	// bStaked and bRanked are checked INDEPENDENTLY. R85 makes them coincide today, but R77 keeps them
	// separate booleans on purpose, and a future tier that splits them must not silently skip one report.
	// THE POT'S FATE, tracked separately from the rating's. An unstaked match has no pot, so there is nothing
	// for a backstop to refund and it starts true; a staked one only earns it by dispatching a settlement.
	bool bPotDispatched = !Result.bStaked;

	if (Result.bStaked)
	{
		FString Body, Error;
		if (BuildSettleBody(Result, StakeAmountPerPosition, CurrencyCode, TEXT("settled"), Body, Error))
		{
			bPotDispatched = true;
			// WeakContext, not a raw capture: this lambda runs on an HTTP completion, seconds after match end
			// and after travel may already have begun. A raw UObject* here would be a use-after-free on any
			// player who left first, which is exactly the window this callback lives in.
			TWeakObjectPtr<const UObject> WeakContext(WorldContext);
			Online->PostServerSettle(Body, [Wire, WeakContext](bool bOk, const FString& Response)
			{
				// Branch rather than a ternary verbosity: UE_LOG's verbosity is a compile-time token, not a value.
				if (bOk) { UE_LOG(LogAFLGameCore, Log,   TEXT("AFL_MATCHREPORT: settle OK for match %s -- %s"), *Wire, *Response.Left(300)); }
				else     { UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: settle FAILED for match %s -- %s"), *Wire, *Response.Left(300)); }

				// Publish the payouts so the results board can show what the match actually paid. Only on
				// success: a failed settle means nobody has been paid, and writing zeroes would render as
				// "you won nothing" on a match whose settlement is still unresolved.
				if (bOk && WeakContext.IsValid())
				{
					PublishSettleOutcome(WeakContext.Get(), Response);
				}
			});
		}
		else
		{
			UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: cannot settle match %s -- %s"), *Wire, *Error);
		}
	}

	if (Result.bRanked)
	{
		FString Body, Error;
		if (BuildRatingBody(Result, Body, Error))
		{
			TWeakObjectPtr<const UObject> WeakContext(WorldContext);
			Online->PostServerRating(Body, [Wire, WeakContext](bool bOk, const FString& Response)
			{
				if (bOk) { UE_LOG(LogAFLGameCore, Log,   TEXT("AFL_MATCHREPORT: rating OK for match %s -- %s"), *Wire, *Response.Left(300)); }
				else     { UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: rating FAILED for match %s -- %s"), *Wire, *Response.Left(300)); }

				if (bOk && WeakContext.IsValid())
				{
					PublishRatingOutcome(WeakContext.Get(), Response);
				}
			});
		}
		else
		{
			UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: cannot rate match %s -- %s"), *Wire, *Error);
		}
	}

	// ⚠ THE POT ONLY. A rating that failed to build is a real fault and is logged as one, but it moves no
	// money -- refunding a settled pot because the LADDER could not be written would be far worse than a
	// missing rating. The caller is asking one question and it is about the money.
	if (!bPotDispatched)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_MATCHREPORT: match %s reported NOTHING for the pot -- any escrow taken for it is still held."), *Wire);
	}
	return bPotDispatched;
}

void FAFLMatchReporter::ReportMatchCancelled(const UObject* WorldContext, const FAFLEscrowLedger& Ledger,
	const FString& ReasonText)
{
	const FString Wire = MatchIdToWire(Ledger.MatchId);

	int32 Pot = 0;
	for (const FAFLEscrowedEntry& E : Ledger.Entries)
	{
		Pot += E.Amount;
	}

	if (!Ledger.IsStaked())
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: match %s CANCELLED (%s) -- nothing was staked, so there is nothing to refund."),
			*Wire, *ReasonText);
		return;
	}

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(WorldContext);
	if (!Online || !Online->IsMatchReportingConfigured())
	{
		// ERROR, not the usual quiet "economy not wired" Log line. Everywhere else that message means a report
		// was skipped; here it means money we already took is not being given back, which is the one skip
		// nobody should have to infer from silence.
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_MATCHREPORT: match %s CANCELLED (%s) but the economy is not wired -- %d %s across %d player(s) REMAINS IN ESCROW."),
			*Wire, *ReasonText, Pot, *Ledger.CurrencyCode, Ledger.Entries.Num());
		return;
	}

	FString Body, Error;
	if (!BuildCancelBody(Ledger, Body, Error))
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_MATCHREPORT: match %s CANCELLED (%s) but the refund body could not be built -- %s. %d %s REMAINS IN ESCROW."),
			*Wire, *ReasonText, *Error, Pot, *Ledger.CurrencyCode);
		return;
	}

	// NAME THE ENTRY THAT IS ABOUT TO SINK THIS REFUND, before sending rather than after. An escrow POST that
	// never came back OK means the ledger either has no row for that player or holds one in an unconfirmed
	// status, and `verifyAgainstEscrow` refuses the WHOLE settlement on either -- so one silent escrow failure
	// at match start becomes a refund that fails for everyone, an hour later, for reasons nothing in the log
	// connects. The reconciliation this needs is manual, and it starts from these lines.
	if (Ledger.ConfirmedCount() != Ledger.Entries.Num())
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_MATCHREPORT: match %s refund posts with only %d of %d escrow entries CONFIRMED -- expect the ledger to refuse it:"),
			*Wire, Ledger.ConfirmedCount(), Ledger.Entries.Num());
		for (const FAFLEscrowedEntry& E : Ledger.Entries)
		{
			if (!E.bConfirmed)
			{
				UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT:   UNCONFIRMED -- %s (%d %s, team %d)"),
					*E.ReconcileId, E.Amount, *Ledger.CurrencyCode, E.TeamId);
			}
		}
	}

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFL_MATCHREPORT: match %s CANCELLED (%s) -- refunding %d %s to %d player(s), NO rake, NO rating."),
		*Wire, *ReasonText, Pot, *Ledger.CurrencyCode, Ledger.Entries.Num());

	Online->PostServerSettle(Body, [Wire, Pot](bool bOk, const FString& Response)
	{
		if (bOk)
		{
			UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: refund OK for cancelled match %s -- %s"), *Wire, *Response.Left(300));
		}
		else
		{
			// The stake is still held. Loud enough to alert on: this is the exact condition the cancellation
			// path exists to prevent, and a failed refund leaves it in place.
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_MATCHREPORT: REFUND FAILED for cancelled match %s -- %d REMAINS IN ESCROW: %s"),
				*Wire, Pot, *Response.Left(300));
		}
	});

	// NO rating call. Not an omission -- see the header. A cancelled match moves no ladder.
}
