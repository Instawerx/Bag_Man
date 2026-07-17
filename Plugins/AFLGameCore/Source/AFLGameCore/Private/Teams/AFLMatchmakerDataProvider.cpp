// Copyright C12 AI Gaming. All Rights Reserved.

#include "Teams/AFLMatchmakerDataProvider.h"

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
		return InjectedGameSessionData;   // the setter (unit tests / S12 onStartGameSession) wins
	}

	// Fall back to the ?MatchmakerData= server launch option (the same OptionsString UAFLBotFillComponent reads
	// NumBots from). S12 replaces this whole source with GameLift onStartGameSession -> GetGameSessionData().
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
