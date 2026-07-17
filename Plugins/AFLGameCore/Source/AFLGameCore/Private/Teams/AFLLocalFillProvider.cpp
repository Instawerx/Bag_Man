// Copyright C12 AI Gaming. All Rights Reserved.

#include "Teams/AFLLocalFillProvider.h"

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "Teams/LyraTeamSubsystem.h"           // team registry (GetTeamIDs / FindTeamFromObject)
#include "Teams/LyraTeamAgentInterface.h"      // IntegerToGenericTeamId
#include "Engine/Engine.h"                     // GEngine->GetWorldFromContextObject
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLocalFillProvider)

bool UAFLLocalFillProvider::BuildLiveCounts(const UObject* WorldContext, TArray<int32>& OutTeamIds,
	TMap<int32, int32>& OutCounts)
{
	OutTeamIds.Reset();
	OutCounts.Reset();

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return false;
	}

	const ULyraTeamSubsystem* Teams = World->GetSubsystem<ULyraTeamSubsystem>();
	if (!Teams)
	{
		return false;
	}

	OutTeamIds = Teams->GetTeamIDs();
	if (OutTeamIds.Num() == 0)
	{
		return false;
	}
	OutTeamIds.Sort();   // ascending -> deterministic lowest-id tie-break in PickLeastPopulated
	for (const int32 Id : OutTeamIds)
	{
		OutCounts.Add(Id, 0);
	}

	// Count current members off the live team registry (NOT a PlayerId cache) -- the bot-safe key.
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* PS : GameState->PlayerArray)
		{
			if (!PS)
			{
				continue;
			}
			const int32 TeamId = Teams->FindTeamFromObject(PS);
			if (int32* Count = OutCounts.Find(TeamId))
			{
				++(*Count);
			}
		}
	}

	return true;
}

int32 UAFLLocalFillProvider::PickLeastPopulated(const TArray<int32>& TeamIds, const TMap<int32, int32>& Counts)
{
	int32 BestId = INDEX_NONE;
	int32 BestCount = MAX_int32;
	for (const int32 Id : TeamIds)   // ascending: strict-less keeps the FIRST (lowest-id) among equal minima
	{
		const int32 Count = Counts.FindRef(Id);
		if (Count < BestCount)
		{
			BestCount = Count;
			BestId = Id;
		}
	}
	return BestId;
}

FGenericTeamId UAFLLocalFillProvider::ChooseBalancedTeam(const UObject* WorldContext) const
{
	TArray<int32> TeamIds;
	TMap<int32, int32> Counts;
	if (!BuildLiveCounts(WorldContext, TeamIds, Counts))
	{
		return FGenericTeamId::NoTeam;
	}

	const int32 Best = PickLeastPopulated(TeamIds, Counts);
	return (Best == INDEX_NONE) ? FGenericTeamId::NoTeam : IntegerToGenericTeamId(Best);
}

void UAFLLocalFillProvider::RequestAssignments(const TArray<APlayerController*>& Players,
	const FOnAFLTeamAssignmentsReady& OnReady)
{
	TArray<FAFLTeamAssignment> Out;
	Out.Reserve(Players.Num());

	// Start from the live counts and keep a running tally so an assignment made earlier in THIS batch influences
	// the next pick -> a greedy even split. Deterministic: input order + ascending team ids + lowest-id tie-break.
	TArray<int32> TeamIds;
	TMap<int32, int32> Counts;
	const UObject* Ctx = (Players.Num() > 0) ? static_cast<const UObject*>(Players[0]) : static_cast<const UObject*>(this);
	if (!BuildLiveCounts(Ctx, TeamIds, Counts) || TeamIds.Num() == 0)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFLTeams: LocalFill RequestAssignments -- no live teams; returning empty."));
		OnReady.ExecuteIfBound(Out);
		return;
	}

	for (const APlayerController* PC : Players)
	{
		if (!PC || !PC->PlayerState)
		{
			continue;
		}

		const int32 Best = PickLeastPopulated(TeamIds, Counts);
		if (Best == INDEX_NONE)
		{
			continue;
		}
		++Counts[Best];   // running tally -> even split across the batch

		const FString PlayerId = PC->PlayerState->GetPlayerName();   // readable id for logs/contract; application
		                                                             // is index-parallel (T1 sidesteps identity-join, §3)
		Out.Emplace(PlayerId, IntegerToGenericTeamId(static_cast<uint8>(Best)));

		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFLTeams: LocalFill split -- real player '%s' -> team %d"), *PlayerId, Best);
	}

	OnReady.ExecuteIfBound(Out);
}
