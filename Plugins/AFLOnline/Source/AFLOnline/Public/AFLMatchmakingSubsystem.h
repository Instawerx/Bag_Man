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
	/**
	 * A /cancel-ticket is in flight. APPENDED, never inserted -- a Blueprint that stored one of the values
	 * above by index would silently mean something else if this were slotted in the middle.
	 *
	 * It exists because cancelling is no longer instantaneous or local. The client stops polling immediately,
	 * but the player is NOT out of the queue until the server says the ticket was withdrawn -- and if that
	 * call fails they are still queued and still matchable. Showing Idle before the answer arrives would be
	 * the same lie the old local-only cancel told, just with a shorter fuse.
	 */
	Cancelling,
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

	/**
	 * Stop searching, and WITHDRAW THE TICKET. Calls POST /cancel-ticket; the ticket is not gone until that
	 * answers. On failure the state returns to Queued and polling RESUMES, because the ticket is still live.
	 */
	void CancelMatchmaking();

	/**
	 * Leave ONE cell and stay in the rest. `QueueId` empty is identical to CancelMatchmaking().
	 *
	 * ⚠ THE STATE AFTER THIS IS NOT NECESSARILY Idle. The server answers with `stillQueued`, and while that is
	 * non-zero the player remains Queued and polling CONTINUES -- they are still matchable in the cells they
	 * kept. Only a cancel that empties every cell reaches Idle.
	 *
	 * Exists because entries hold their own play-limit reservation under the per-entry exposure ruling: a
	 * player in three cells is holding three reservations, and "get me out of the expensive one" is a real
	 * request that a single all-or-nothing Cancel cannot express.
	 */
	void CancelQueue(const FString& QueueId);

	EAFLMatchmakingState GetState() const { return State; }
	const FText& GetLastReason() const { return LastReason; }

	/**
	 * The cell this attempt is in, or EMPTY when nothing of ours is live.
	 *
	 * The lobby needs it to know which ROW should offer a cancel, which it could not previously ask: this
	 * subsystem tracked a state and never a cell, so "are you queued" was answerable and "queued in WHAT" was
	 * not.
	 *
	 * ⚠ ONE CELL, BECAUSE THE CLIENT CURRENTLY PERMITS ONE. StartMatchmaking refuses outright while Requesting
	 * or Queued, so multi-entry is unreachable from the UI even though the backend and /cancel-ticket both
	 * support it. When that guard is lifted this becomes a SET and the lobby's per-row lookup below is already
	 * shaped for it -- the row asks "is this cell mine", not "what is my one cell".
	 */
	const FString& GetQueuedQueueId() const { return QueuedQueueId; }

	/** Whether THIS cell is one we hold a live entry in. The shape the lobby actually asks in. */
	bool IsQueuedIn(const FString& InQueueId) const
	{
		return !QueuedQueueId.IsEmpty() && QueuedQueueId == InQueueId;
	}

	/** Fires on every transition, including Failed. The front end binds this to drive the PLAY button. */
	FAFLOnMatchmakingState OnStateChanged;

private:
	void SetState(EAFLMatchmakingState NewState, const FText& Reason = FText());
	void PollMatchStatus();
	void StopPolling();

	/**
	 * Claim a GameLift player session, THEN travel. Called the instant /match-status says `ready`.
	 *
	 * ⚠ THE CLAIM CANNOT HAPPEN EARLIER, AND THAT IS THE WHOLE POINT OF FIX B. GameLift reserves a player
	 * session for 60 SECONDS -- a service constant with no knob -- and the clock starts when the session is
	 * MINTED. Minting at placement (which is what the backend used to do) burned the entire window before any
	 * player had read the row: verified 2026-08-12, both sessions read TIMEDOUT, unclaimed, while the match
	 * sat ACTIVE and joinable by nobody. Claiming here starts the 60s at the moment we are about to spend it.
	 *
	 * /claim-session is IDEMPOTENT inside that window -- a second call returns the same id rather than holding
	 * a second reservation -- so a retry is safe to add here later without risk of consuming a teammate's slot.
	 */
	void ClaimAndTravel(const FString& MatchId);

	/** Issue the actual travel once a full tuple is in hand. */
	void TravelToMatch(const FString& IpAddress, int32 Port, const FString& PlayerSessionId);

	/** Arm the next poll at the ladder's current step. One-shot; each poll re-arms the next. */
	void ArmNextPoll();

	/** Clear the pending poll WITHOUT forgetting how long this attempt has waited. Cancel needs exactly this. */
	void ClearPollTimer();

	/**
	 * Clear BOTH pieces of poll progress. ⚠ THE ONE PLACE THAT MAY DO SO, and the reason it is a function
	 * rather than two assignments: the timer and the queue clock have to reset together or a second queue
	 * attempt inherits the first one's position on the ladder and starts on the 60-second tail, which reads
	 * to a player as a dead button from the very first tick.
	 */
	void ResetPollProgress();

	/** Seconds this queue attempt has been waiting. 0 when not queued. */
	float ElapsedQueuedSeconds() const;

	/**
	 * THE BACKOFF LADDER. A staked ticket lives 43,200s at FlexMatch while the old client gave up at 603s --
	 * a 72x disagreement in which the player was told "no match found" while the thing that could still match
	 * them ran for another eleven hours.
	 *
	 *   0-60s     3s    UNCHANGED, deliberately. Every join timing Phase 6 measured was taken at 3s, and the
	 *                   first minute is where a bot-filled match commits for a solo player.
	 *   60s-10m   10s   Past a minute the player has accepted they are waiting; 10s is imperceptible against
	 *                   that and cuts the busiest stretch from 180 polls to 54.
	 *   10m-1h    30s   Nobody watches a queue for an hour. This step keeps the ticket JOINABLE, not responsive.
	 *   1h-12h    60s   The tail, and 79% of the polls.
	 *
	 * ⚠ 60s IS BOUNDED BY MATCH_READY_TTL_SECONDS = 600, NOT CHOSEN FOR TIDINESS. The ready row written at
	 * placement expires ten minutes later, so any interval under that still finds it. 60s keeps 10x margin
	 * for clock skew, a cold Lambda, and a missed tick. 300s would halve the tail polls and cut the margin
	 * to 2x, which is not a trade worth making for ~330 requests.
	 *
	 * 834 polls covers the full 43,200s, against 14,400 for a flat 3s and 200 for the old cap.
	 */
	static float PollIntervalForWait(float WaitedSeconds);

	EAFLMatchmakingState State = EAFLMatchmakingState::Idle;
	FText LastReason;

	/** Set when an attempt commits, cleared by SetState on Idle/Failed. See GetQueuedQueueId. */
	FString QueuedQueueId;

	FTimerHandle PollTimer;

	/**
	 * When this queue attempt started, on the world's REAL-time clock -- not accumulated per tick.
	 *
	 * Real time rather than game time because a paused or dilated front end must not distort a queue clock
	 * the server is keeping in wall seconds. A stamp rather than an accumulator because an accumulator drifts
	 * by one interval per step and has to be corrected in two places; subtraction cannot.
	 */
	double QueueStartRealSeconds = 0.0;

	/**
	 * The client's own give-up, matched to the LONGEST a ticket can live server-side: staked configurations
	 * carry RequestTimeoutSeconds 43,200 and carry NO expansion, so a staked ticket really can wait twelve
	 * hours for humans. A shorter client cap does not shorten the ticket -- it only stops us watching one
	 * that is still live, which is how a player gets placed into a staked match nobody is looking at.
	 */
	static constexpr float MaxQueueSeconds = 43200.0f;
};
