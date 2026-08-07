// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GenericTeamAgentInterface.h"   // FGenericTeamId (AIModule)
#include "UObject/Interface.h"

#include "AFLTeamAssignmentTypes.generated.h"

class APlayerController;
class APlayerState;

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
 * THE SEAM IS LIVE AS OF THE PHASE-0 UNLOCK. UAFLTeamCreationComponent holds this interface type (not a
 * concrete provider), and selects between LocalFill and MatchmakerData at first use -- see that class.
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

	/**
	 * Resolve the team for ONE participant arriving AFTER the RequestAssignments batch -- a late human, or a
	 * fill bot. Every per-join decision routes here.
	 *
	 * ⚠ THIS EXISTS BECAUSE THE BATCH PATH ALONE CANNOT CARRY THE SEAM. Before the Phase-0 unlock the per-join
	 * call was `UAFLLocalFillProvider::ChooseBalancedTeam`, which is CONCRETE-ONLY -- so the component could
	 * not hold the interface type no matter what else was changed, and the authoritative provider was
	 * unassignable by construction. Declaring per-join on the interface is what actually opens the seam.
	 *
	 * The two implementations answer DIFFERENT QUESTIONS, and that asymmetry is the point:
	 *   LocalFill  -- "which team is emptiest right now" (live-count balance; bot-safe, no PlayerId).
	 *   Matchmaker -- "which team did the roster already give this participant" (reconcile-key lookup).
	 * A provider that cannot place the participant returns FGenericTeamId::NoTeam; the caller logs and leaves
	 * the player unassigned rather than inventing a team, because in a staked match a fabricated side is worse
	 * than a visible gap.
	 */
	virtual FGenericTeamId ChooseTeamForJoiningPlayer(const UObject* WorldContext,
		const APlayerState* JoiningPlayer) const = 0;

	/**
	 * How many HUMANS this provider expects the match to seat, or INDEX_NONE when that is unknowable.
	 *
	 * Non-pure deliberately: a provider that cannot know returns INDEX_NONE and needs no implementation.
	 * LocalFill genuinely cannot know -- humans connect on their own schedule and the count is only ever
	 * observed, never predicted. An authoritative roster knows exactly.
	 *
	 * ⚠ REFERENCED IN COMMENTS SINCE T1 AND NEVER DECLARED. `UAFLBotFillComponent` names
	 * `IAFLTeamAssignmentProvider::GetExpectedHumanCount()` in both its header and its converge block as the
	 * Option-A replacement for present-count fill -- against a method that did not exist. Declared here so the
	 * reference resolves; **the bot-fill consumer is NOT switched to it in this change** (fill counts humans
	 * PRESENT, and changing that is a behaviour change, not an unlock).
	 */
	virtual int32 GetExpectedHumanCount(const UObject* /*WorldContext*/) const { return INDEX_NONE; }
};
