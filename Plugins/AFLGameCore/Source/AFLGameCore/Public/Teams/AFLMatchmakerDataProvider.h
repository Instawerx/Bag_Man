// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Teams/AFLTeamAssignmentTypes.h"

#include "AFLMatchmakerDataProvider.generated.h"

class APlayerController;

/**
 * UAFLMatchmakerDataProvider  (Team SSOT §3 -- the ranked / online assignment source, T2)
 *
 * The AUTHORITATIVE sibling of UAFLLocalFillProvider. It reconciles the GameLift-delivered matchmaker roster (the
 * "GameSessionData" the dedicated server reads at onStartGameSession) against the connected controllers, so each
 * real player lands on the team the matchmaker assigned. IsAuthoritative() == true -> UAFLBotFillComponent's
 * converge goes inert (ranked has no bots, SSOT §0.2/§3).
 *
 * THE LOCKED BACKEND CONTRACT (Bag_Man_Backend match-allocator -> GameSessionData JSON):
 *     { "matchId": "...",
 *       "members": [ { "id": "<PlayFab entity id>", "type": "title_player_account", "team": "0" }, ... ] }
 *   - member.id  = the identity-join key == FAFLTeamAssignment.PlayerId == the reconcile key a client carries in
 *                  its ?PlayFabId= connect option.
 *   - member.team = a 0-BASED STRING team index ("0"/"1"/...) -> the 1-BASED AFL team id (AFL team = roster
 *     index + 1). The AFL team setup B_AFL_TeamSetup_TwoTeams creates ids {1,2} (confirmed 2026-07-17), so
 *     roster "0"->AFL 1, "1"->AFL 2. N-TEAM GENERIC; never hardcode 2. The +1 is THE ONE convention point in
 *     ResolveAssignments -- if a real S12 roster ever arrives already 1-based, drop it there only.
 *
 * GameSessionData SOURCE (isolated behind ResolveGameSessionData for a one-point S12 swap):
 *   - NOW: SetGameSessionData (unit tests) or the ?MatchmakerData= server launch option (the same OptionsString
 *          UAFLBotFillComponent reads NumBots from).
 *   - S12: the real GameLift Server SDK onStartGameSession -> GetGameSessionData(), on the validated LyraServer
 *          build. Swap only the source; the parse + reconcile never change.
 *
 * RECONCILE KEY: each controller carries its PlayFab id (UAFLOnlineSubsystem::GetReconcileKey()) in ?PlayFabId=,
 * stashed server-side at AAFLGameMode::InitNewPlayer onto a UAFLReconcileIdComponent; GetReconcileId reads it. If
 * the real roster keys on EntityToken.Entity.Id instead, ONLY GetReconcileKey() changes -- this provider is
 * unaffected (swap-gated).
 *
 * NOT the active provider yet: UAFLTeamCreationComponent::GetProvider stays LocalFill (T1) until the online path
 * is live (S12). This class is a built + unit-tested drop-in (afl.Teams.Matchmaker.Test).
 */
UCLASS()
class AFLGAMECORE_API UAFLMatchmakerDataProvider : public UObject, public IAFLTeamAssignmentProvider
{
	GENERATED_BODY()

public:
	//~IAFLTeamAssignmentProvider
	virtual void RequestAssignments(const TArray<APlayerController*>& Players,
		const FOnAFLTeamAssignmentsReady& OnReady) override;
	virtual bool IsAuthoritative() const override { return true; }

	/**
	 * Per-join: the team THE ROSTER ALREADY GAVE this participant, looked up by reconcile key. Never a balance
	 * decision -- an authoritative match's sides were settled before anyone connected, and re-balancing a late
	 * arrival would silently contradict the roster the stake settles against.
	 *
	 * Returns NoTeam for a bot (an authoritative match is human-only -- `ai-bots.md` §6.3 / R74), for a
	 * participant with no reconcile key, and for a key the roster does not name. **NoTeam is the correct answer,
	 * not a failure**: in a staked match a fabricated side is worse than a visible gap.
	 */
	virtual FGenericTeamId ChooseTeamForJoiningPlayer(const UObject* WorldContext,
		const APlayerState* JoiningPlayer) const override;

	/** The roster's member count -- what an authoritative match expects to seat. INDEX_NONE if no roster. */
	virtual int32 GetExpectedHumanCount(const UObject* WorldContext) const override;
	//~End of IAFLTeamAssignmentProvider

	/** Inject the matchmaker roster JSON (unit tests now; S12 swaps the source to onStartGameSession). */
	void SetGameSessionData(const FString& InGameSessionDataJson) { InjectedGameSessionData = InGameSessionDataJson; }

	/**
	 * PURE reconcile core (WORLD-FREE -> the unit-test acceptance): parse the locked-contract GameSessionData JSON
	 * and, INDEX-PARALLEL to OrderedReconcileIds, emit one FAFLTeamAssignment per id -- the team the roster gives
	 * that id (NoTeam if the id is not in the roster). N-team generic. No live objects: feed it the fixture + a
	 * list of ids and assert right-roster->right-teams (order-independent: reconciled BY id, not by index).
	 */
	static TArray<FAFLTeamAssignment> ResolveAssignments(const FString& GameSessionDataJson,
		const TArray<FString>& OrderedReconcileIds);

	/**
	 * THE ONE PLACE that answers "what is the authoritative matchmaker payload for this server?".
	 * GameLift's onStartGameSession if it has ARRIVED, else the ?MatchmakerData= launch option, else empty.
	 *
	 * ⚠ EVERY reader of the payload must call THIS, never ParseOption(OptionsString, "MatchmakerData") directly.
	 * S12 was originally designed as a one-point swap inside ResolveGameSessionData, and that was WRONG: three
	 * separate places keyed off the raw launch option, and fixing only one produced a run where GameLift
	 * delivered the roster correctly and the match still came out LocalFill and unstaked. The three readers:
	 *
	 *   1. ResolveGameSessionData      -- the roster data itself
	 *   2. UAFLTeamCreationComponent   -- which PROVIDER gets selected (matchmaker vs local fill)
	 *   3. FAFLMatchReporter           -- ReadEconomics, the tier/stake source
	 *
	 * Miss any one and the failure is silent and misleading: the log shows the payload arriving while the
	 * match behaves as if it had not. Static, because the selection in (2) happens BEFORE any provider exists.
	 */
	static FString ResolveAuthoritativeMatchmakerData(const UObject* WorldContext);

private:
	/** Injected test data wins; otherwise defers to ResolveAuthoritativeMatchmakerData. */
	FString ResolveGameSessionData(const UObject* WorldContext) const;

	/** The reconcile key a controller carries (UAFLReconcileIdComponent on its PlayerState, set at InitNewPlayer). */
	static FString GetReconcileId(const APlayerController* PC);

	/** The same key read straight off a PlayerState -- the per-join path has no controller in hand. */
	static FString GetReconcileIdFromState(const APlayerState* PS);

	/** How many members the roster names. INDEX_NONE when the JSON is absent or unparseable. */
	static int32 CountRosterMembers(const FString& GameSessionDataJson);

	/** Injected roster JSON (SetGameSessionData). Empty -> fall back to the launch option. */
	FString InjectedGameSessionData;
};
