// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameModes/LyraBotCreationComponent.h"

#include "AFLBotFillComponent.generated.h"

/**
 * UAFLBotFillComponent  (Team SSOT §7 T1.3 -- human-aware bot FILL)
 *
 * Subclass-override of ULyraBotCreationComponent that makes the bot COUNT human-aware:
 *   EffectiveBotCount = max(0, TeamSize * NumTeams - HumanCount)
 * so a mode fills to a true N-per-team (3v3 = 6 total: 2 humans -> 4 bots, 1 -> 5, 0 -> 6). Stock is a FLAT
 * NumBotsToCreate; this replaces ONLY the count decision and reuses the stock spawn/possess/team-routing
 * (SpawnOneBot), so every bot still flows through OnGameModePlayerInitialized ->
 * UAFLTeamCreationComponent::ServerChooseTeamForPlayer -> the provider's live-count balance (the proven path).
 *
 * FILL is LOCAL-ONLY and deliberately KEPT OUT OF the assignment provider (SSOT §0.2/§3): the T2 matchmaker
 * never bot-fills, so IAFLTeamAssignmentProvider stays untouched -- preserving the drop-in contract.
 *
 * NOTE (scope): the count is computed at experience-load (stock LowPriority, after team creation). Late-join
 * bot rebalancing (removing a bot when a human joins mid-match) is a later refinement, out of T1.3.
 */
UCLASS()
class AFLGAMECORE_API UAFLBotFillComponent : public ULyraBotCreationComponent
{
	GENERATED_BODY()

protected:
	/** Players per team for this mode (3v3 -> 3). NumTeams is read live from ULyraTeamSubsystem. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Bots")
	int32 TeamSize = 3;

	//~ULyraBotCreationComponent interface
	virtual void ServerCreateBots_Implementation() override;
	//~End of ULyraBotCreationComponent interface
};
