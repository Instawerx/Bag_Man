// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameModes/LyraBotCreationComponent.h"

#include "AFLBotFillComponent.generated.h"

class AController;
class AGameModeBase;

/**
 * UAFLBotFillComponent  (Team SSOT §7 T1.3 -- human-aware bot FILL, displace/re-fill)
 *
 * Subclass-override of ULyraBotCreationComponent that makes the bot COUNT human-aware and CONVERGENT to a true
 * N-per-team:  Target = TeamSize * NumTeams;  bots aim for max(0, Target - HumanCount)
 * (3v3 = 6 total: 2 humans -> 4 bots, 1 -> 5, 0 -> 6). Stock is a FLAT NumBotsToCreate; this replaces ONLY the
 * count decision and reuses the stock spawn/possess/team-routing (SpawnOneBot), so every bot still flows through
 * OnGameModePlayerInitialized -> UAFLTeamCreationComponent::ServerChooseTeamForPlayer -> the provider's
 * live-count balance (the proven path).
 *
 * WHY DISPLACE/RE-FILL (not a one-shot present-count): on a listen server the host loads the experience BEFORE
 * remote clients connect, so "humans present at experience-load" is 1 and a one-shot fill overshoots to 4v3/7
 * when the 2nd human joins. The T2-correct source is EXPECTED humans -- but there is no clean expected count
 * offline (no matchmaker; GameSession::MaxPlayers is a ceiling; the match-start signal lives in the AFLCombat
 * GameFeature, unreachable from this always-loaded core module). So the fill CONVERGES: keep the experience-load
 * fill (bots through warmup), then reconcile to Target on each late HUMAN join/leave --
 *   join  (ALyraGameMode::OnGameModePlayerInitialized) -> total > Target -> RemoveOneBot from the FULLER team
 *   leave (FGameModeEvents::OnGameModeLogoutEvent)      -> total < Target -> SpawnOneBot (down to the floor)
 * removing from the fuller team so the balanced split holds. Converges to Target regardless of connect order.
 *
 * FIELD SIZE IS DECLARED BY THE PLAYLIST (`?FieldSize=N`), NOT BY THE TEAM COUNT. For a SOLO battle royale
 * TeamSize is 1, so the structural target `TeamSize * NumTeams` collapses to "however many teams the team-setup
 * asset happens to author" -- and one `B_AFL_TeamSetup_Solo` (36 teams) is shared by every BR bracket. That made
 * BR9 and BR20 fill to 36, measured in PIE: 35 bots in a nine-player match. The bracket is a property of the
 * PLAYLIST, so the playlist declares it, via `ULyraUserFacingExperienceDefinition::ExtraArgs` -> URL option.
 *
 * Why the URL and not a team-setup asset per bracket: a per-bracket team setup needs a per-bracket Teams action
 * set to reference it, and `FGameFeatureComponentEntry` is not BlueprintType -- the `AddComponents` row cannot be
 * scripted (BR_ZONE_SYSTEM §"three that must close"), so that route costs three hand-clicked rows and three more
 * assets to express one integer. The URL is also the seam a dedicated-server matchmaker already speaks: it hands
 * the server a map URL, exactly where `Experience` and `NumBots` already live.
 *
 * Absent the option, `ComputeTargetTotal()` returns the structural product unchanged -- every non-BR mode keeps
 * its existing behaviour byte for byte. A declared size is CLAMPED to the structural capacity and warns if it
 * exceeds it, because seating more players than there are team slots silently breaks the solo invariant.
 *
 * SEAM-GATED FOR T2 (the Option-A structure; SSOT §0.2/§3): the converge hooks bind ONLY when the active
 * assignment provider is NON-authoritative (UAFLTeamCreationComponent::IsAssignmentAuthoritative()==false --
 * LocalFill / offline / PIE). A T2 MatchmakerDataProvider (authoritative) seats all humans pre-start, so this
 * path stays INERT and a future one-shot `Target - IAFLTeamAssignmentProvider::GetExpectedHumanCount()`
 * (Option A) replaces the present-count fill. FILL stays OUT of the provider's SPLIT: the provider only EXPOSES
 * the authoritative flag -- it never spawns or assigns bots (the T2 matchmaker never bot-fills).
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

#if WITH_SERVER_CODE
private:
	/** Live team count (ULyraTeamSubsystem::GetTeamIDs). */
	int32 GetNumTeams() const;

	/** Total seats for the mode. The playlist's `?FieldSize=N` wins when present (clamped to the structural
	 *  capacity); otherwise the structural product max(0, TeamSize) * GetNumTeams(). Single source for BOTH the
	 *  one-shot fill and the converge pass -- they computed it separately before, which is how they could drift. */
	int32 ComputeTargetTotal() const;

	/** Real humans currently in PlayerArray (bots + spectators excluded). */
	int32 CountHumans() const;

	/**
	 * ARE BOTS BARRED FROM THIS MATCH? The single gate for BOTH fills (they must never disagree).
	 *
	 * Replaces the tier-BLIND `IsRosterExternallyOwned()` test, which stood bots down for every GameLift
	 * placement regardless of tier and so made bot-fill unreachable in production entirely.
	 *
	 * ⚠ IT FAILS CLOSED ON AN UNKNOWN TIER, AND THAT IS THE WHOLE DESIGN. `ReadEconomics` cannot say "I do
	 * not know" -- with no payload it returns LEAGUE PLAY, which permits bots. Measured 2026-08-13, the
	 * one-shot fill runs 4.085s BEFORE onStartGameSession delivers the payload (23:26:30.640 vs
	 * 23:26:34.725), so a naive `IsRosterExternallyOwned() && !BotsPermitted(Tier)` reads `true && !true` =
	 * false in that window and spawns bots into a match that may turn out to be STAKED. An externally-owned
	 * roster that has not yet named its tier is therefore treated as barring bots.
	 */
	bool ShouldBarBots() const;

	/**
	 * HOW MANY HUMANS THIS MATCH WILL SEAT -- Option A, the payload roster, not who has connected yet.
	 *
	 * The roster names every human the match was committed for, so `Target - Expected` is stable from the
	 * first evaluation and does not oscillate as players travel in. Falls back to CountHumans() only when
	 * there is no roster (PIE / offline / LocalFill), where expectation is genuinely unknowable.
	 */
	int32 ResolveHumanBaseline() const;

	/** Bring the bot count to max(0, Target - ResolveHumanBaseline()): trim the fuller team / spawn to the floor. */
	void ReconcileBotFill();

	/** Remove one spawned bot sitting on the CURRENTLY fuller team (keeps the balanced split). */
	void RemoveOneBotOnFullerTeam();

	/** Late HUMAN join -> reconcile now (PlayerArray + the human's team assignment are already applied). */
	void HandlePlayerJoined(AGameModeBase* GameMode, AController* NewPlayer);

	/** HUMAN logout -> reconcile NEXT tick (the leaver is still in PlayerArray during the logout broadcast). */
	void HandlePlayerLoggedOut(AGameModeBase* GameMode, AController* Exiting);

	/** Converge hooks bound once, after the one-shot fill (LocalFill / non-authoritative only). */
	bool bConvergeHooksBound = false;

	/** Re-entrancy guard: our own Spawn/RemoveOneBot re-fire the join/logout hooks (bot-filtered). */
	bool bReconciling = false;
#endif // WITH_SERVER_CODE
};
