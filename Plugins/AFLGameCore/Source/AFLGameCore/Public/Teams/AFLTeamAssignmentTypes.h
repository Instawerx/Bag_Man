// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GenericTeamAgentInterface.h"   // FGenericTeamId (AIModule)
#include "UObject/Interface.h"

#include "AFLTeamAssignmentTypes.generated.h"

class APlayerController;

/**
 * FAFLTeamAssignment -- one resolved (player -> team) decision produced by an IAFLTeamAssignmentProvider.
 *
 * PlayerId is a provider-scoped stable id (a PlayFab/GameLift player-session id online; a local key
 * offline/PIE). TeamId is the FGenericTeamId the assignment layer applies via ILyraTeamAgentInterface --
 * the same MyTeamID path ALyraCharacter already replicates. Plain struct (no reflection needed).
 */
struct FAFLTeamAssignment
{
	FString PlayerId;
	FGenericTeamId TeamId = FGenericTeamId::NoTeam;

	FAFLTeamAssignment() = default;
	FAFLTeamAssignment(const FString& InPlayerId, FGenericTeamId InTeamId)
		: PlayerId(InPlayerId)
		, TeamId(InTeamId)
	{
	}
};

/** Fired once a provider has resolved assignments for the requested players (may be async). */
DECLARE_DELEGATE_OneParam(FOnAFLTeamAssignmentsReady, const TArray<FAFLTeamAssignment>& /*Assignments*/);

UINTERFACE(MinimalAPI)
class UAFLTeamAssignmentProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * IAFLTeamAssignmentProvider  (the swappable team-assignment source -- Team SSOT §1)
 *
 * LocalFillProvider (offline/casual/PIE) and MatchmakerDataProvider (ranked/online) both implement this;
 * UAFLTeamCreationComponent holds one active provider and never knows which produced the teams, so the
 * round-manager consumption layer stays untouched (SSOT §0.5). RequestAssignments is async-capable -- the
 * shipped round-manager retry-guard tolerates late teams (SSOT §1).
 *
 * NOTE: no provider is constructed in this first increment (delegate-to-stock only) -- this seam is declared
 * now so the LocalFill increment (bot-fill + party-together) drops onto it without touching the consumption
 * layer.
 */
class IAFLTeamAssignmentProvider
{
	GENERATED_BODY()

public:
	/** Resolve team assignments for the given controllers; fire OnReady when ready (may be async). */
	virtual void RequestAssignments(const TArray<APlayerController*>& Players,
		const FOnAFLTeamAssignmentsReady& OnReady) = 0;

	/** True for the matchmaker-authoritative (ranked) provider; false for local fill (SSOT §0.1). */
	virtual bool IsAuthoritative() const = 0;
};
