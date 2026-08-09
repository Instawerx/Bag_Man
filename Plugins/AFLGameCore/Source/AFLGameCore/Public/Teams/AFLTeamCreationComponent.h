// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Teams/LyraTeamCreationComponent.h"
#include "Teams/AFLTeamAssignmentTypes.h"   // IAFLTeamAssignmentProvider (the seam this component holds)

#include "AFLTeamCreationComponent.generated.h"

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

public:
	/**
	 * True when an AUTHORITATIVE (T2 matchmaker) provider is active; false for LocalFill / offline / PIE.
	 * UAFLBotFillComponent gates its displace/re-fill converge on this -- it runs ONLY when NON-authoritative
	 * (a matchmade roster is seated pre-start, so there are no late joins to re-fill). The provider never spawns
	 * or assigns bots -- it only EXPOSES this flag (SSOT §0.2/§3).
	 */
	bool IsAssignmentAuthoritative() const;

#if WITH_SERVER_CODE
protected:
	//~ULyraTeamCreationComponent interface
	virtual void ServerAssignPlayersToTeams() override;
	virtual void ServerChooseTeamForPlayer(ALyraPlayerState* PS) override;
	//~End of ULyraTeamCreationComponent interface

private:
	/** Late-join hook (humans AND bots) -> per-join resolution via ServerChooseTeamForPlayer. */
	void HandlePlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer);

	/**
	 * Lazily construct and cache the active provider, SELECTED ONCE per match.
	 *
	 * SELECTION SIGNAL: the presence of `?MatchmakerData=` on the game mode's OptionsString -- the same
	 * OptionsString `UAFLBotFillComponent` reads `NumBots` from, and the same source
	 * `UAFLMatchmakerDataProvider::ResolveGameSessionData` already falls back to. Present -> the AUTHORITATIVE
	 * matchmaker provider; absent -> LocalFill.
	 *
	 * **Chosen because it is the honest signal**: a roster is what makes a match matchmaker-authoritative, so
	 * its arrival is the condition itself rather than a proxy for it. A dedicated-server check would claim
	 * authority a server without a roster cannot exercise, and a cvar would let the two disagree.
	 */
	IAFLTeamAssignmentProvider* GetProvider();
#endif

private:
	/**
	 * The active team-assignment provider, held as the INTERFACE.
	 *
	 * ⚠ THIS FIELD WAS `TObjectPtr<UAFLLocalFillProvider>` -- concrete -- until the Phase-0 unlock, which is why
	 * `UAFLMatchmakerDataProvider` was **structurally unassignable**: written, unit-tested, and reachable by
	 * nothing. `IsAssignmentAuthoritative()` could therefore only ever return false, which in turn made
	 * `UAFLBotFillComponent`'s authoritative gate a dead branch. **One type was holding three systems shut.**
	 */
	UPROPERTY(Transient)
	TScriptInterface<IAFLTeamAssignmentProvider> Provider;

	/**
	 * S12 — the PROVISIONAL provider, used only while the real decision is still pending.
	 *
	 * Under GameLift the roster arrives ASYNCHRONOUSLY, and the first call to GetProvider() happens at
	 * experience load with zero players — 55 seconds before onStartGameSession in a measured run. Caching a
	 * LocalFill choice there is a decision made BEFORE its input exists, and it silently sticks: the observed
	 * result was both players on one team and the match unstaked, while the log showed the payload arriving.
	 *
	 * So while the SDK is ready but has not delivered, answer from HERE and do NOT populate `Provider`. The
	 * next call re-evaluates, and once the payload lands the real provider is chosen and cached exactly once.
	 * This preserves the single-assigner rule — `Provider` still changes never — while refusing to commit to
	 * an answer that has not been asked properly yet.
	 *
	 * Safe because GameLift ACTIVATES a session before routing any player to it: by the time a real player can
	 * join, the payload has arrived. The only caller that ever sees this is the zero-player batch at
	 * experience load, for which LocalFill and matchmaker are indistinguishable — both assign nobody.
	 */
	UPROPERTY(Transient)
	TScriptInterface<IAFLTeamAssignmentProvider> ProvisionalProvider;
};
