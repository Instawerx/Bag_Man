// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "AFLLeagueBotFallbackSubsystem.generated.h"

/**
 * UAFLLeagueBotFallbackSubsystem -- Option A, the HOST half of the 30s LEAGUE bot-fallback.
 *
 * AFLOnline's UAFLMatchmakingSubsystem raises OnLeagueFallbackDue when a purely-LEAGUE (unstaked) queue has
 * waited its fallback window with no real match placed (operator ruling: "wait 30s for live players, then
 * fill with bots"). The DETECTION lives in the always-loaded online module, which deliberately carries NO
 * LyraGame dependency; the HOST lives HERE in AFLGameCore, which already sees LyraGame + CommonSession.
 *
 * On the signal we resolve the cell's queueId -> a ULyraUserFacingExperienceDefinition playlist (which
 * bundles the map + LyraExperienceDefinition), CreateHostingRequest, force OnlineMode=Offline, and
 * UCommonSessionSubsystem::HostSession. Offline is a bare ServerTravel with NO ?listen, no reservation
 * beacon, and no EOS session -- so there are no remote connections and no S12-E ?PlayerSessionId= PreLogin.
 * Bots then fill via UAFLBotFillComponent (non-authoritative LocalFill on a standalone host). EVERY unstaked
 * LEAGUE cell fills -- MatchPlay (Arena + Map) AND Battle Royale -- because an uncovered venue+bracket falls
 * back to the guaranteed ShantyTown playlist of the same bracket+league (operator ruling 2026-09-05). Staked
 * never reaches here (ShouldLeagueBotFallback bars any staked entry, R85).
 *
 * The client cannot read the server queue registry (R18 keeps map/experience off the /queues wire), so the
 * cell->playlist resolution is a naming map to the authored DA_AFL_* playlist assets; a missing asset is
 * handled gracefully (the host tells the matchmaking subsystem the fallback could not run). PIE-unproven --
 * the offline host + bot fill are watched-in-PIE gates.
 */
UCLASS()
class AFLGAMECORE_API UAFLLeagueBotFallbackSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	/** Bound to UAFLMatchmakingSubsystem::OnLeagueFallbackDue: resolve the playlist and host offline, or, if
	 *  no playlist backs the cell, tell the matchmaking subsystem the fallback failed so the UI recovers. */
	void HandleLeagueFallbackDue(const FString& QueueId);

	/** queueId (Tier_League_Ruleset_Venue_Bracket) -> the PREFERRED DA_AFL_* playlist asset name (venue-specific
	 *  where one exists), or NAME_None only for a non-LEAGUE / malformed cell. The caller falls back to the
	 *  guaranteed ShantyTown playlist if the preferred one does not load. */
	static FName ResolvePlaylistAsset(const FString& QueueId);

	/** The ALWAYS-authored ShantyTown playlist for a cell's bracket+league (all 12 MatchPlay + 3 BR exist), so
	 *  any uncovered Arena bracket or missing venue-specific asset still fills. NAME_None only if the cell is
	 *  not a LEAGUE cell or is malformed. */
	static FName ShantyTownFallbackFor(const FString& QueueId);

	FDelegateHandle FallbackHandle;
};
