// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Teams/LyraTeamCreationComponent.h"

#include "AFLTeamCreationComponent.generated.h"

class UAFLLocalFillProvider;
class ALyraPlayerState;
class AGameModeBase;
class AController;

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
 * ASSIGNMENT DRIVEN BY THE PROVIDER (SSOT §1/§2):
 *  - ServerAssignPlayersToTeams: resolves the REAL-player split via IAFLTeamAssignmentProvider
 *    (UAFLLocalFillProvider in T1) -- the drop-in surface a T2 MatchmakerDataProvider fills from GameLift
 *    MatchmakerData. Applied index-parallel to the gathered controllers (T1 sidesteps the identity-join, §3).
 *  - ServerChooseTeamForPlayer: routes EVERY per-join (late human AND each bot) through the provider's
 *    live-count balance -- bot-safe (no PlayerId cache; §2 note).
 */
UCLASS()
class AFLGAMECORE_API UAFLTeamCreationComponent : public ULyraTeamCreationComponent
{
	GENERATED_BODY()

#if WITH_SERVER_CODE
protected:
	//~ULyraTeamCreationComponent interface
	virtual void ServerAssignPlayersToTeams() override;
	virtual void ServerChooseTeamForPlayer(ALyraPlayerState* PS) override;
	//~End of ULyraTeamCreationComponent interface

private:
	/** Late-join hook (humans AND bots) -> per-join balance via ServerChooseTeamForPlayer. */
	void HandlePlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer);

	/** Lazily create the active provider (LocalFill in T1). */
	UAFLLocalFillProvider* GetProvider();
#endif

private:
	/** The active team-assignment provider (LocalFill in T1; a MatchmakerDataProvider swaps in at T2). */
	UPROPERTY(Transient)
	TObjectPtr<UAFLLocalFillProvider> Provider;
};
