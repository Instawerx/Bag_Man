// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameModes/LyraGameMode.h"

#include "AFLGameMode.generated.h"

class AController;
class APlayerController;

/**
 * AAFLGameMode  (Arena round respawn gate)
 *
 * Minimal ALyraGameMode subclass whose ONLY job is the round-based respawn gate. It lives in the
 * ALWAYS-LOADED AFLGameCore plugin (NOT a GameFeature) because a map-default GameMode is instantiated at
 * WORLD INIT, before the experience's GameFeature loads -- a GameMode inside the GameFeature it bootstraps
 * is absent/unregistered at map load.
 *
 * The gate is on the virtual ControllerCanRestart (LYRAGAME_API-exported; ULyraPlayerSpawningManager-
 * Component::ControllerCanRestart is PRIVATE/non-virtual, so the game mode is the extension point). It
 * queries the IAFLRoundRestartPolicy seam on the GameState's components -- NO concrete GameFeature type
 * referenced (the round driver implements the interface; dependency direction stays GameFeature ->
 * always-loaded). SAFE as a global default: without a policy provider it falls through to Super (stock
 * Lyra respawn). Wiring this as the project/experience game mode is Task 2 (config), not C++.
 */
UCLASS()
class AFLGAMECORE_API AAFLGameMode : public ALyraGameMode
{
	GENERATED_BODY()

public:
	//~ALyraGameMode interface
	virtual bool ControllerCanRestart(AController* Controller) override;

	/**
	 * T2 identity-join (server side): stash the reconcile key the client carried in its ?PlayFabId= connect option
	 * onto its PlayerState (a UAFLReconcileIdComponent), so UAFLMatchmakerDataProvider can reconcile the matchmaker
	 * roster (member.id) against the actual connected controllers. Absent for LocalFill / offline / PIE joins (no
	 * ?PlayFabId=) -> a pure no-op, safe on the live join path. NOT read until the matchmaker provider is the
	 * active provider (S12).
	 */
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal) override;

	/**
	 * S12-E: the identity gate. Validates the client's `?PlayerSessionId=` against GameLift and REFUSES the
	 * connection if it does not hold up.
	 *
	 * ⚠ CONDITIONAL, AND THIS IS THE PART THAT MUST NOT REGRESS. The gate applies only when the roster is
	 * externally owned (a GameLift-placed match). A gate that fired on "no session id present" would lock out
	 * every PIE session, listen-server host, and offline run -- flagged as a defect in an earlier proposal
	 * and still the failure mode to avoid. The question is "does someone else own this roster", never "is the
	 * parameter there".
	 */
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;

	/**
	 * S12-E §4: a disconnect opens a RECONNECT GRACE WINDOW; it does not end the player's participation.
	 *
	 * ⚠ DELIBERATELY DOES NOT CALL RemovePlayerSession. Verified against live GameLift: removal is TERMINAL --
	 * a COMPLETED session can never be accepted again ("has a status of COMPLETED instead of RESERVED"). So
	 * releasing the seat on a dropout would permanently bar a player from a match their stake is already
	 * escrowed in, with no undo. A four-second wifi blip must not cost someone their entry.
	 */
	virtual void Logout(AController* Exiting) override;
	//~End of ALyraGameMode interface

private:
	/**
	 * PlayerSessionId -> the PlayFab entity id GameLift resolved for it, populated in PreLogin and consumed
	 * (and erased) in InitNewPlayer.
	 *
	 * Carried rather than re-resolved because PreLogin is where the accept happens and InitNewPlayer is where
	 * the PlayerState exists; re-describing would mean a second round-trip to learn something already known.
	 * Entries are erased on consumption, and a PreLogin that never becomes a join leaves at most one stale
	 * entry per abandoned connection attempt.
	 */
	TMap<FString, FString> ValidatedPlayerIds;

	/**
	 * How long a dropped player keeps their seat. Ruling (operator, 2026-08-09): a dropout that never returns
	 * resolves as CANCELLED-REFUND, not forfeit -- a power cut must not be punished like a rage-quit.
	 *
	 * 90s is a judgement call, not a measurement: long enough to survive a router reboot or a game restart,
	 * short enough that the opponent is not held indefinitely. Tune with real data.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Reconnect", meta = (ClampMin = "0"))
	float ReconnectGraceSeconds = 90.f;

	/** PlayFab id -> the grace timer holding their seat. Entry present == seat reserved, match not yet resolved. */
	TMap<FString, FTimerHandle> ReconnectTimers;

	/**
	 * PlayFab id -> the GameLift player session that identity came from. Kept because Logout is too late to
	 * read the connect options, and the session id is what ReleasePlayerSession needs once grace expires.
	 */
	TMap<FString, FString> SessionIdByPlayFabId;

	/** Grace expired for this player: the seat is genuinely gone. */
	void HandleReconnectWindowExpired(FString PlayFabId);
};
