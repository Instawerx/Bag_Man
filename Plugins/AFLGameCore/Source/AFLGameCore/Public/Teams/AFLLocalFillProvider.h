// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Teams/AFLTeamAssignmentTypes.h"

#include "AFLLocalFillProvider.generated.h"

/**
 * UAFLLocalFillProvider  (Team SSOT §2 -- the offline / casual / PIE assignment source)
 *
 * Deterministic, balanced least-populated assignment. REAL PLAYERS ONLY in RequestAssignments -- the batch is
 * exactly what a T2 MatchmakerDataProvider would deliver (real players + their teams); bots are FILL, balanced
 * per-join via ChooseBalancedTeam (§0.2/§3). IsAuthoritative() = false.
 *
 * BALANCE RULE = live-count least-populated with a deterministic tie-break (lowest team id). It is LIVE-COUNT
 * based, never a PlayerId cache: bots arrive with an uninitialised PlayerId, so keying assignment by it piles
 * every bot onto one team (the v1 2v3 trap). The live team-population is the only key -- bot-safe by
 * construction.
 */
UCLASS()
class AFLGAMECORE_API UAFLLocalFillProvider : public UObject, public IAFLTeamAssignmentProvider
{
	GENERATED_BODY()

public:
	//~IAFLTeamAssignmentProvider
	virtual void RequestAssignments(const TArray<APlayerController*>& Players,
		const FOnAFLTeamAssignmentsReady& OnReady) override;
	virtual bool IsAuthoritative() const override { return false; }
	//~End of IAFLTeamAssignmentProvider

	/**
	 * The single balance rule: the live least-populated team (deterministic tie-break = lowest team id).
	 * Live-count based -> bot-safe (no PlayerId). Used per-join for late humans AND bots.
	 */
	FGenericTeamId ChooseBalancedTeam(const UObject* WorldContext) const;

private:
	/** Team-id set (ULyraTeamSubsystem, sorted ascending) + current per-team member counts (GameState). */
	static bool BuildLiveCounts(const UObject* WorldContext, TArray<int32>& OutTeamIds, TMap<int32, int32>& OutCounts);

	/** Least-populated of TeamIds given Counts; TeamIds ascending so ties resolve to the lowest id. INDEX_NONE if none. */
	static int32 PickLeastPopulated(const TArray<int32>& TeamIds, const TMap<int32, int32>& Counts);
};
