// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Teams/LyraTeamCreationComponent.h"

#include "AFLTeamCreationComponent.generated.h"

/**
 * UAFLTeamCreationComponent  (the team-assignment seam -- Team SSOT §1/§2)
 *
 * Subclass-override of the stock ULyraTeamCreationComponent. Stock team CREATION (TeamsToCreate /
 * TeamDA_* registration) is KEPT -- only the assignment DECISION is the extension point. This is
 * subclass-override, NOT a parallel assigner (running one alongside stock would race two assigners --
 * the drift hazard, SSOT §0.3/§2). The round-manager consumption layer (UAFLRoundManagerComponent:
 * dynamic GetTeamIDs, per-team round/extraction/match-end, the shipped BeginPlay retry-guard) is
 * UNTOUCHED -- this feeds it exactly the FGenericTeamIds stock did (SSOT §0.5).
 *
 * FIRST-INCREMENT CONTRACT -- ZERO BEHAVIOR CHANGE: the override delegates to Super:: only (no provider
 * yet). The subclass merely sits in the assignment path; behavior is byte-identical to stock. The next
 * increment swaps the body to the IAFLTeamAssignmentProvider (LocalFill: bot-fill + party-together).
 */
UCLASS()
class AFLGAMECORE_API UAFLTeamCreationComponent : public ULyraTeamCreationComponent
{
	GENERATED_BODY()

#if WITH_SERVER_CODE
protected:
	//~ULyraTeamCreationComponent interface
	virtual void ServerAssignPlayersToTeams() override;
	//~End of ULyraTeamCreationComponent interface
#endif
};
