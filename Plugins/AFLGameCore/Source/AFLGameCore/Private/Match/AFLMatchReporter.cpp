// Copyright C12 AI Gaming. All Rights Reserved.

#include "Match/AFLMatchReporter.h"

#include "AFLGameCore.h"                 // LogAFLGameCore
#include "AFLOnlineSubsystem.h"          // the signed server transport (server-only key)
#include "Teams/AFLReconcileIdComponent.h"   // the per-player reconcile key the matchmaker roster matched on
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
	const FString MatchmakerData = UGameplayStatics::ParseOption(Options, TEXT("MatchmakerData"));
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

void FAFLMatchReporter::EscrowAll(const UObject* WorldContext, const FGuid& MatchId,
	const TArray<FString>& ReconcileIds, const FString& CurrencyCode, int32 AmountPerPlayer)
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(WorldContext);
	if (!Online)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_MATCHREPORT: no online subsystem -- escrow skipped for match %s"), *MatchIdToWire(MatchId));
		return;
	}
	if (AmountPerPlayer <= 0)
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: unstaked match %s -- nothing to escrow."), *MatchIdToWire(MatchId));
		return;
	}

	const FString Wire = MatchIdToWire(MatchId);
	for (const FString& Id : ReconcileIds)
	{
		if (Id.IsEmpty()) { continue; }   // a bot has no reconcile id, and bots do not stake
		const FString Body = BuildEscrowBody(MatchId, Id, CurrencyCode, AmountPerPlayer);
		Online->PostServerEscrow(Body, [Wire, Id](bool bOk, const FString& Response)
		{
			if (bOk)
			{
				UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: escrowed %s for match %s"), *Id, *Wire);
			}
			else
			{
				// A failed escrow means this player is NOT funded. Settlement will refuse the whole match
				// on the count mismatch, which is the correct outcome -- loud, not silently short.
				UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: ESCROW FAILED for %s in match %s -- settlement will refuse this match: %s"),
					*Id, *Wire, *Response.Left(300));
			}
		});
	}
}

void FAFLMatchReporter::EscrowTeamSeries(const UObject* WorldContext, const FGuid& MatchId, const FMatchEconomics& Economics)
{
	const FString Wire = MatchIdToWire(MatchId);

	if (!Economics.IsStaked())
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: LEAGUE PLAY match %s -- no buy-in, nothing to escrow (R85)."), *Wire);
		return;
	}

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(WorldContext);
	if (!Online || !Online->IsMatchReportingConfigured())
	{
		// Expected in a plain PIE session: the env vars are absent by design. One line, not one per player.
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: economy not wired -- match %s not escrowed."), *Wire);
		return;
	}

	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: no game state -- match %s NOT escrowed."), *Wire);
		return;
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
			return;
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
			return;
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
		return;
	}
	if (HumansByTeam.Num() < 2)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_MATCHREPORT: staked match %s has %d team(s) with players -- need 2. NOT escrowed (nobody debited)."),
			*Wire, HumansByTeam.Num());
		return;
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
			return;
		}
		ShareByTeam.Add(Team.Key, Economics.StakePerPosition / Size);
	}

	// --- every check passed: debit. ---
	int32 Posted = 0;
	for (const TPair<int32, TArray<FString>>& Team : HumansByTeam)
	{
		const int32 Share = ShareByTeam[Team.Key];
		for (const FString& Id : Team.Value)
		{
			const FString Body = BuildEscrowBody(MatchId, Id, Economics.CurrencyCode, Share);
			Online->PostServerEscrow(Body, [Wire, Id, Share](bool bOk, const FString& Response)
			{
				if (bOk)
				{
					UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: escrowed %d from %s for match %s"), Share, *Id, *Wire);
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
}

void FAFLMatchReporter::ReportMatchEnd(const UObject* WorldContext, const FAFLMatchResult& Result,
	int32 StakeAmountPerPosition, const FString& CurrencyCode)
{
	const FString Wire = MatchIdToWire(Result.MatchId);

	// VALIDATE FIRST, ALWAYS. A settlement built from a malformed result is worse than no settlement,
	// because the backend has no way to know the result was malformed -- it would accept it.
	FString ValidationError;
	if (!Result.Validate(ValidationError))
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: REFUSING to report match %s -- invalid result: %s"), *Wire, *ValidationError);
		return;
	}

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(WorldContext);
	if (!Online)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_MATCHREPORT: no online subsystem -- match %s not reported."), *Wire);
		return;
	}
	if (!Online->IsMatchReportingConfigured())
	{
		// One clear line rather than three identical per-endpoint skips. Expected in a plain PIE session,
		// where the env vars are absent by design.
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_MATCHREPORT: economy not wired (AFL_EARN_HMAC_KEY / AFL_*_URL absent) -- match %s not reported."), *Wire);
		return;
	}

	// bStaked and bRanked are checked INDEPENDENTLY. R85 makes them coincide today, but R77 keeps them
	// separate booleans on purpose, and a future tier that splits them must not silently skip one report.
	if (Result.bStaked)
	{
		FString Body, Error;
		if (BuildSettleBody(Result, StakeAmountPerPosition, CurrencyCode, TEXT("settled"), Body, Error))
		{
			Online->PostServerSettle(Body, [Wire](bool bOk, const FString& Response)
			{
				// Branch rather than a ternary verbosity: UE_LOG's verbosity is a compile-time token, not a value.
				if (bOk) { UE_LOG(LogAFLGameCore, Log,   TEXT("AFL_MATCHREPORT: settle OK for match %s -- %s"), *Wire, *Response.Left(300)); }
				else     { UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: settle FAILED for match %s -- %s"), *Wire, *Response.Left(300)); }
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
			Online->PostServerRating(Body, [Wire](bool bOk, const FString& Response)
			{
				if (bOk) { UE_LOG(LogAFLGameCore, Log,   TEXT("AFL_MATCHREPORT: rating OK for match %s -- %s"), *Wire, *Response.Left(300)); }
				else     { UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: rating FAILED for match %s -- %s"), *Wire, *Response.Left(300)); }
			});
		}
		else
		{
			UE_LOG(LogAFLGameCore, Error, TEXT("AFL_MATCHREPORT: cannot rate match %s -- %s"), *Wire, *Error);
		}
	}
}
