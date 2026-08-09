// Copyright C12 AI Gaming. All Rights Reserved.

#include "Teams/AFLMatchmakerDataProvider.h"

#include "Online/AFLGameLiftHostSubsystem.h"   // S12: the GameLift-delivered roster, when it has arrived

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "Teams/AFLReconcileIdComponent.h"     // the per-player stashed reconcile key
#include "Teams/LyraTeamAgentInterface.h"      // IntegerToGenericTeamId
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"            // ParseOption (mirrors UAFLBotFillComponent's OptionsString read)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchmakerDataProvider)

TArray<FAFLTeamAssignment> UAFLMatchmakerDataProvider::ResolveAssignments(const FString& GameSessionDataJson,
	const TArray<FString>& OrderedReconcileIds)
{
	TArray<FAFLTeamAssignment> Out;
	Out.Reserve(OrderedReconcileIds.Num());

	// Parse the locked contract: { matchId, members: [ { id, type, team } ] } -> id -> AFL team id (the 0-based
	// roster index is mapped to the 1-based AFL team below).
	TMap<FString, int32> IdToTeam;
	FString MatchId;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GameSessionDataJson);
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		Root->TryGetStringField(TEXT("matchId"), MatchId);

		const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
		if (Root->TryGetArrayField(TEXT("members"), Members) && Members)
		{
			for (const TSharedPtr<FJsonValue>& MemberVal : *Members)
			{
				const TSharedPtr<FJsonObject> Member = MemberVal.IsValid() ? MemberVal->AsObject() : nullptr;
				if (!Member.IsValid())
				{
					continue;
				}
				FString Id;
				FString TeamStr;
				Member->TryGetStringField(TEXT("id"), Id);
				Member->TryGetStringField(TEXT("team"), TeamStr);
				if (!Id.IsEmpty())
				{
					// The roster team is a 0-BASED string index ("0"/"1"/...). Malformed -> 0.
					const int32 RosterIndex = TeamStr.IsNumeric() ? FCString::Atoi(*TeamStr) : 0;

					// >>> THE ONE TEAM-ID CONVENTION POINT <<<
					// The GameLift matchmaker roster indexes teams 0-BASED; the AFL team setup
					// (B_AFL_TeamSetup_TwoTeams -> TeamsToCreate {1,2}, confirmed 2026-07-17) is 1-BASED. Map the
					// 0-based roster index -> the 1-based AFL team id. N-TEAM GENERIC (roster "2" -> AFL team 3, ...).
					// If a real S12 roster ever arrives ALREADY 1-based, drop the +1 HERE only -- nothing else moves.
					const int32 AFLTeamId = RosterIndex + 1;

					IdToTeam.Add(Id, AFLTeamId);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFLTeams: Matchmaker ResolveAssignments -- GameSessionData is not valid JSON; roster empty."));
	}

	// Emit INDEX-PARALLEL to OrderedReconcileIds (the consumer applies Assignments[i] -> Players[i]); each id keeps
	// its OWN roster team regardless of connect order -> reconciliation is by id, not by position.
	for (const FString& ReconcileId : OrderedReconcileIds)
	{
		const int32* AFLTeamId = IdToTeam.Find(ReconcileId);
		const FGenericTeamId TeamId = AFLTeamId ? IntegerToGenericTeamId(*AFLTeamId) : FGenericTeamId::NoTeam;
		Out.Emplace(ReconcileId, TeamId);

		if (!AFLTeamId)
		{
			UE_LOG(LogAFLGameCore, Warning,
				TEXT("AFLTeams: Matchmaker -- reconcile id '%s' NOT in roster (match '%s') -> NoTeam."),
				*ReconcileId, *MatchId);
		}
	}

	return Out;
}

FString UAFLMatchmakerDataProvider::GetReconcileId(const APlayerController* PC)
{
	if (!PC || !PC->PlayerState)
	{
		return FString();
	}
	if (const UAFLReconcileIdComponent* IdComp = PC->PlayerState->FindComponentByClass<UAFLReconcileIdComponent>())
	{
		return IdComp->GetReconcileId();
	}
	return FString();
}

FString UAFLMatchmakerDataProvider::ResolveGameSessionData(const UObject* WorldContext) const
{
	if (!InjectedGameSessionData.IsEmpty())
	{
		return InjectedGameSessionData;   // the setter (unit tests) wins -- tests must be able to force a roster
	}
	return ResolveAuthoritativeMatchmakerData(WorldContext);
}

FString UAFLMatchmakerDataProvider::ResolveAuthoritativeMatchmakerData(const UObject* WorldContext)
{
	// S12: GameLift's onStartGameSession, held by UAFLGameLiftHostSubsystem. This is the PRODUCTION source.
	//
	// Consulted only when the payload has actually ARRIVED. GameLift delivers asynchronously, so "the
	// subsystem exists" is not the same question as "the roster is known" -- treating an empty payload as
	// authoritative would silently produce an unassigned-team match. Empty means "not mine to answer", and
	// the launch option below still gets its turn.
	if (const UAFLGameLiftHostSubsystem* GameLift = UAFLGameLiftHostSubsystem::Get(WorldContext))
	{
		if (GameLift->HasGameSessionData())
		{
			return GameLift->GetGameSessionData();
		}
	}

	// Fall back to the ?MatchmakerData= server launch option (the same OptionsString UAFLBotFillComponent reads
	// NumBots from). NOT removed by S12 -- it stays as the local/offline path, and it is what #20 was proven on.
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr)
	{
		return UGameplayStatics::ParseOption(GameMode->OptionsString, TEXT("MatchmakerData"));
	}
	return FString();
}

void UAFLMatchmakerDataProvider::RequestAssignments(const TArray<APlayerController*>& Players,
	const FOnAFLTeamAssignmentsReady& OnReady)
{
	// Ordered reconcile ids, index-parallel to Players: each controller's stashed PlayFab id.
	TArray<FString> OrderedReconcileIds;
	OrderedReconcileIds.Reserve(Players.Num());
	for (const APlayerController* PC : Players)
	{
		OrderedReconcileIds.Add(GetReconcileId(PC));
	}

	const UObject* Ctx = (Players.Num() > 0) ? static_cast<const UObject*>(Players[0]) : static_cast<const UObject*>(this);
	const FString GameSessionData = ResolveGameSessionData(Ctx);

	const TArray<FAFLTeamAssignment> Assignments = ResolveAssignments(GameSessionData, OrderedReconcileIds);

	for (const FAFLTeamAssignment& A : Assignments)
	{
		UE_LOG(LogAFLGameCore, Log, TEXT("AFLTeams: Matchmaker split -- reconcile id '%s' -> team %d"),
			*A.PlayerId, static_cast<int32>(A.TeamId.GetId()));
	}

	OnReady.ExecuteIfBound(Assignments);
}

FString UAFLMatchmakerDataProvider::GetReconcileIdFromState(const APlayerState* PS)
{
	if (!PS)
	{
		return FString();
	}
	if (const UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>())
	{
		return IdComp->GetReconcileId();
	}
	return FString();
}

int32 UAFLMatchmakerDataProvider::CountRosterMembers(const FString& GameSessionDataJson)
{
	if (GameSessionDataJson.IsEmpty())
	{
		return INDEX_NONE;   // no roster is NOT a roster of zero -- the distinction is the whole point
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GameSessionDataJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return INDEX_NONE;
	}

	const TArray<TSharedPtr<FJsonValue>>* Members = nullptr;
	if (!Root->TryGetArrayField(TEXT("members"), Members) || !Members)
	{
		return INDEX_NONE;
	}
	return Members->Num();
}

FGenericTeamId UAFLMatchmakerDataProvider::ChooseTeamForJoiningPlayer(const UObject* WorldContext,
	const APlayerState* JoiningPlayer) const
{
	if (!JoiningPlayer)
	{
		return FGenericTeamId::NoTeam;
	}

	// A bot has no roster entry, and in an authoritative match it should not exist at all: `ai-bots.md` §6.3
	// forbids bot-fill in any match whose result carries stake or rating, and R74 keeps them out of population
	// entirely. Answering NoTeam rather than balancing means a bot that reaches here is VISIBLE as unassigned
	// instead of being quietly seated into a staked side.
	if (JoiningPlayer->IsABot())
	{
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFLTeams: Matchmaker per-join -- BOT '%s' reached an AUTHORITATIVE match. Left unassigned; "
			     "bots do not belong in a staked or rated roster (ai-bots R74)."),
			*JoiningPlayer->GetPlayerName());
		return FGenericTeamId::NoTeam;
	}

	const FString ReconcileId = GetReconcileIdFromState(JoiningPlayer);
	if (ReconcileId.IsEmpty())
	{
		// No key means ?PlayFabId= never arrived -- the connect-option append is still owed (Phase 3).
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFLTeams: Matchmaker per-join -- '%s' carries NO reconcile key -> NoTeam. The ?PlayFabId= "
			     "connect option is not being appended."),
			*JoiningPlayer->GetPlayerName());
		return FGenericTeamId::NoTeam;
	}

	const TArray<FAFLTeamAssignment> Resolved =
		ResolveAssignments(ResolveGameSessionData(WorldContext), TArray<FString>{ ReconcileId });

	const FGenericTeamId TeamId = Resolved.Num() > 0 ? Resolved[0].TeamId : FGenericTeamId::NoTeam;

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLTeams: Matchmaker per-join -- '%s' (key '%s') -> team %d"),
		*JoiningPlayer->GetPlayerName(), *ReconcileId, static_cast<int32>(TeamId.GetId()));

	return TeamId;
}

int32 UAFLMatchmakerDataProvider::GetExpectedHumanCount(const UObject* WorldContext) const
{
	return CountRosterMembers(ResolveGameSessionData(WorldContext));
}
