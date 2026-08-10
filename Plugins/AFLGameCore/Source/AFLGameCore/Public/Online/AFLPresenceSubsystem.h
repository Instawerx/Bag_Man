// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"

#include "AFLPresenceSubsystem.generated.h"

/**
 * UAFLPresenceSubsystem -- says "I am here", on a timer, for as long as the client is running.
 *
 * ══ THE WRITE HALF OF THE ONLINE COUNT ════════════════════════════════════════════════════════════════
 *
 * `GET /presence` counts clients that checked in inside a 90s window. This is what checks them in. Without
 * it the endpoint is correct and permanently reads zero -- the read half wired to nothing.
 *
 * ══ WHY IT LIVES IN THE ALWAYS-LOADED MODULE, AND NOT NEXT TO THE LOBBY ═══════════════════════════════
 *
 * **A PLAYER IN A MATCH IS ONLINE.** A heartbeat that only ran on the front end would undercount to near
 * zero at exactly the moment the number matters most -- a busy evening, everyone in a game, the lobby
 * cheerfully reporting that nobody is playing. So presence is not a lobby concern and cannot live in
 * AFLCombat, which is `ExplicitlyLoaded: true` and whose GameInstanceSubsystems are therefore never
 * instantiated at all.
 *
 * ══ WHY A TIMER AND NOT JOIN/LEAVE EVENTS ═════════════════════════════════════════════════════════════
 *
 * A crashed client, a killed process or a pulled cable never sends "I left". Every increment/decrement
 * scheme therefore drifts upward forever and needs a reconciler to walk it back. A repeated "still here"
 * with a short server-side TTL has no such failure: stopping IS the departure signal, and the server
 * forgets on its own. The cost is that the count lags a hard crash by up to the window; the benefit is that
 * it cannot be wrong in the direction that flatters us.
 *
 * ⚠ THE BEAT IS 30s AGAINST A 90s WINDOW, ON PURPOSE. Two consecutive misses are tolerated before a player
 * drops out of the count, so a hitch, a GC pause or one dropped request does not blink someone offline.
 * Beating at the window length would make every missed request a visible population dip.
 */
UCLASS()
class AFLGAMECORE_API UAFLPresenceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End of UGameInstanceSubsystem

	static UAFLPresenceSubsystem* Get(const UObject* WorldContextObject);

	/** Beat once, now. The timer calls this; the console command and login completion also do. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Presence")
	void SendHeartbeat();

	/** How many beats the server has accepted this session. Zero after login means something is wrong. */
	int32 GetAcceptedBeats() const { return AcceptedBeats; }

	/** Seconds between beats. 30 against the server's 90s window -- see the class comment. */
	static constexpr float BeatIntervalSeconds = 30.f;

private:
	void StartBeating();
	void HandleLoggedIn();

	FTimerHandle BeatTimer;
	FDelegateHandle LoginHandle;

	int32 AcceptedBeats = 0;
	int32 FailedBeats = 0;

	/** One line when the first beat lands and one when the first fails -- never one per beat, forever. */
	bool bLoggedFirstSuccess = false;
	bool bLoggedFirstFailure = false;
};
