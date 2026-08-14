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

FGenericTeamId UAFLLocalFillProvider::ChooseBalancedTeam(const UObject* WorldContext,
	const TMap<int32, int32>* SeedCounts) const
{
	TArray<int32> TeamIds;
	TMap<int32, int32> Counts;
	if (!BuildLiveCounts(WorldContext, TeamIds, Counts))
	{
		return FGenericTeamId::NoTeam;
	}

	// SEED COUNTS: seats that are SPOKEN FOR but not yet occupied. Live counts alone describe who is standing
	// on the field right now, which is the wrong question when a roster names humans who are still travelling
	// -- the bots fill first, balance among themselves, and the arriving humans then land on their rostered
	// side and overload it. MEASURED 2026-08-14: a solo LEAGUE PLAY roster produced a 3/2 bot split, the human
	// was seated by roster onto the 3 side, and the match ran 4v2 on a 3v3 field (and was swept 7-0).
	//
	// LocalFill itself never passes these -- it has no roster, and with SeedCounts null this function is
	// byte-identical to what it always was. The caller that HAS a roster (UAFLMatchmakerDataProvider) supplies
	// the tally. ONE balance rule either way; the knowledge of who is expected lives with whoever has it.
	if (SeedCounts)
	{
		for (const TPair<int32, int32>& Seed : *SeedCounts)
		{
			if (int32* Count = Counts.Find(Seed.Key))
			{
				*Count += Seed.Value;
			}
			// A seeded team id that is not in the live team set is IGNORED, not added: TeamIds comes from
			// ULyraTeamSubsystem and is the authority on which teams exist. A roster naming a team this mode
			// does not author is a roster/mode mismatch, and inventing the team here would hide it.
		}
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

FGenericTeamId UAFLLocalFillProvider::ChooseTeamForJoiningPlayer(const UObject* WorldContext,
	const APlayerState* /*JoiningPlayer*/) const
{
	// The participant is intentionally unused. Balance reads the LIVE POPULATION, never the arriver's identity:
	// bots arrive with an uninitialised PlayerId, and keying on it piles every bot onto one team (the v1 2v3
	// trap this provider exists to avoid). Delegating keeps ONE balance rule rather than two that can drift.
	return ChooseBalancedTeam(WorldContext);
}
