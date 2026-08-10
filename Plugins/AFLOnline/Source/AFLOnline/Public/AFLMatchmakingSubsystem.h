// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"

#include "AFLMatchmakingSubsystem.generated.h"

/** Where a player is in the PLAY flow. Drives the front-end button state and nothing else. */
UENUM(BlueprintType)
enum class EAFLMatchmakingState : uint8
{
	/** Not queuing. PLAY is pressable. */
	Idle,
	/** POST /create-ticket in flight -- we have asked, the server has not yet agreed. */
	Requesting,
	/** Ticket accepted. Waiting for a match to form and place. This is where most of the time is spent. */
	Queued,
	/** /match-status returned a tuple; travel has been issued. Terminal for this subsystem. */
	Joining,
	/** Refused or gave up. Reason carries something a player can act on. */
	Failed,
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FAFLOnMatchmakingState, EAFLMatchmakingState /*State*/, const FText& /*Reason*/);

/**
 * UAFLMatchmakingSubsystem -- PLAY, end to end.
 *
 * Turns a press of PLAY into a joined dedicated server:
 *
 *   1. POST /create-ticket    queueId + stake. The SERVER authors the stake attributes; we only ask.
 *   2. poll POST /match-status until it answers `ready`
 *   3. ClientTravel to        <ip>:<port>?PlayerSessionId=<id>
 *
 * ⚠ WHY A POLL AND NOT A PUSH. There is no channel from the backend to a client sitting in a menu. The
 * player is not connected to anything yet -- that is the entire problem PLAY solves -- so the client has to
 * ask. /match-status is a cheap keyed read designed for exactly this.
 *
 * ⚠ WHY A GameInstanceSubsystem. Queuing spans the front end AND the travel that ends it. A component on a
 * widget or a controller would be destroyed by the very ClientTravel it exists to perform, and a player who
 * matched would be dropped mid-handoff.
 *
 * ⚠ THE PLAYER SESSION ID IS NOT OPTIONAL. Since S12-E a client arriving without ?PlayerSessionId= is
 * REFUSED at PreLogin, because the server derives identity from the session rather than trusting the URL.
 * So travel must never be attempted on a partial tuple -- a missing id is a rejected connection, not a
 * degraded one.
 */
UCLASS()
class AFLONLINE_API UAFLMatchmakingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~UGameInstanceSubsystem
	virtual void Deinitialize() override;
	//~End of UGameInstanceSubsystem

	/**
	 * Press PLAY. QueueId comes from /queues (never hardcoded in the UI -- the published set changes), and
	 * Stake must be one of that queue's advertised rungs.
	 *
	 * Stake is IGNORED by the server for an unstaked tier and REQUIRED for a staked one; pass 0 for
	 * LEAGUE PLAY. Sending a stake to an unstaked queue is a 400, not a courtesy.
	 */
	void StartMatchmaking(const FString& QueueId, int32 Stake);

	/** Stop polling and return to Idle. Does NOT withdraw the PlayFab ticket -- see the .cpp note. */
	void CancelMatchmaking();

	EAFLMatchmakingState GetState() const { return State; }
	const FText& GetLastReason() const { return LastReason; }

	/** Fires on every transition, including Failed. The front end binds this to drive the PLAY button. */
	FAFLOnMatchmakingState OnStateChanged;

private:
	void SetState(EAFLMatchmakingState NewState, const FText& Reason = FText());
	void PollMatchStatus();
	void StopPolling();

	/** Issue the actual travel once a full tuple is in hand. */
	void TravelToMatch(const FString& IpAddress, int32 Port, const FString& PlayerSessionId);

	EAFLMatchmakingState State = EAFLMatchmakingState::Idle;
	FText LastReason;

	FTimerHandle PollTimer;

	/**
	 * How long to keep asking before giving up. Generous: a real queue can legitimately take minutes at low
	 * population, and a client that quits early leaves a player holding a PlayFab ticket that may still
	 * match -- placing them in a game they are no longer watching for.
	 */
	static constexpr float PollIntervalSeconds = 3.0f;
	static constexpr int32 MaxPolls = 200;   // ~10 minutes
	int32 PollCount = 0;
};
