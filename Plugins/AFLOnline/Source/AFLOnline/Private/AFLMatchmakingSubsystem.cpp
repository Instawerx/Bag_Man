// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLMatchmakingSubsystem.h"

#include "AFLOnlineSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchmakingSubsystem)

// Its own category, not AFLOnlineSubsystem's: that one is DEFINE_LOG_CATEGORY_STATIC and therefore private to
// its translation unit. A separate category is the better answer anyway -- the PLAY flow spans several
// seconds of polling, and being able to filter it apart from login and the signed economy calls is what
// makes a failed queue readable.
DEFINE_LOG_CATEGORY_STATIC(LogAFLMatchmaking, Log, All);

namespace
{
	/** Parse a Lambda JSON body. Returns null on anything unparseable -- callers treat that as a failure. */
	TSharedPtr<FJsonObject> ParseJson(const FString& Body)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		return (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()) ? Root : nullptr;
	}

	/** The backend's error bodies are {"error": "..."} -- surface that text rather than a generic failure. */
	FText ErrorTextFrom(const FString& Body, const FText& Fallback)
	{
		if (const TSharedPtr<FJsonObject> Root = ParseJson(Body))
		{
			FString Err;
			if (Root->TryGetStringField(TEXT("error"), Err) && !Err.IsEmpty())
			{
				return FText::FromString(Err);
			}
		}
		return Fallback;
	}
}

void UAFLMatchmakingSubsystem::Deinitialize()
{
	StopPolling();
	Super::Deinitialize();
}

void UAFLMatchmakingSubsystem::SetState(EAFLMatchmakingState NewState, const FText& Reason)
{
	State = NewState;
	LastReason = Reason;
	OnStateChanged.Broadcast(NewState, Reason);
}

void UAFLMatchmakingSubsystem::ClearPollTimer()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().ClearTimer(PollTimer);
		}
	}
}

void UAFLMatchmakingSubsystem::StopPolling()
{
	// STOP = the attempt is OVER. Clearing the timer and forgetting the clock belong together here, because
	// every caller of this is a terminal transition: matched, failed, gave up, subsystem torn down.
	//
	// ⚠ CANCEL IS NOT ONE OF THEM AND MUST NOT USE THIS. A cancel that FAILS leaves the player queued, and
	// resetting the clock there would tell the ladder they had just arrived -- restarting at 3s and, worse,
	// restarting the 12-hour give-up on someone who had already waited most of it. Cancel clears the timer
	// only; it resets progress in its SUCCESS branch, where the attempt really is over.
	ClearPollTimer();
	ResetPollProgress();
}

void UAFLMatchmakingSubsystem::ResetPollProgress()
{
	// ⚠ THE TIMER AND THE CLOCK RESET TOGETHER OR NOT AT ALL. This used to be `PollCount = 0` sitting at the
	// bottom of StopPolling, and the ladder adds a SECOND piece of progress beside it. Two assignments in one
	// function is exactly how they drift: someone adds an early-out above the reset, or a third caller clears
	// the timer directly, and the queue clock survives into the next attempt. The player then presses PLAY
	// and their first poll lands on the 60-second tail -- a dead button, from the first tick, with no error
	// anywhere. Hence one function, and the ensure in ArmNextPoll's caller that proves it ran.
	QueueStartRealSeconds = 0.0;
}

float UAFLMatchmakingSubsystem::ElapsedQueuedSeconds() const
{
	if (QueueStartRealSeconds <= 0.0)
	{
		return 0.0f;
	}
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	return World ? static_cast<float>(World->GetRealTimeSeconds() - QueueStartRealSeconds) : 0.0f;
}

float UAFLMatchmakingSubsystem::PollIntervalForWait(float WaitedSeconds)
{
	if (WaitedSeconds < 60.0f)   { return 3.0f; }
	if (WaitedSeconds < 600.0f)  { return 10.0f; }
	if (WaitedSeconds < 3600.0f) { return 30.0f; }
	return 60.0f;
}

void UAFLMatchmakingSubsystem::ArmNextPoll()
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	// A RE-ARMING ONE-SHOT, NOT A LOOPING TIMER. A looping timer has one fixed rate for its whole life, so
	// the ladder cannot exist inside it -- changing the interval means clearing and re-setting anyway. Doing
	// it explicitly at the end of each poll also means the NEXT interval is chosen from the wait we have
	// actually accrued, rather than from whatever it was when the queue began.
	const float Interval = PollIntervalForWait(ElapsedQueuedSeconds());
	World->GetTimerManager().SetTimer(PollTimer, this,
		&UAFLMatchmakingSubsystem::PollMatchStatus, Interval, /*bLoop=*/false);
}

void UAFLMatchmakingSubsystem::StartMatchmaking(const FString& QueueId, int32 Stake)
{
	if (State == EAFLMatchmakingState::Requesting || State == EAFLMatchmakingState::Queued)
	{
		// De-duplication, and the two cases are NOT the same thing -- saying so matters. Requesting means a
		// /create-ticket is still in flight; Queued means one was accepted. Logging both as "already in the
		// queue" reads, in a log where the next line is a 400, as though a refused ticket had left the client
		// stuck queued. It has not: the refusal path sets Failed, and the next attempt goes out normally.
		// That misreading cost a diagnosis on 2026-08-11, so the message now names the actual state.
		UE_LOG(LogAFLMatchmaking, Warning, TEXT("AFL_MM: StartMatchmaking ignored -- %s."),
			State == EAFLMatchmakingState::Requesting
				? TEXT("a ticket request is already in flight")
				: TEXT("already queued"));
		return;
	}
	if (QueueId.IsEmpty())
	{
		SetState(EAFLMatchmakingState::Failed, NSLOCTEXT("AFL", "MMNoQueue", "No queue selected."));
		return;
	}

	UAFLOnlineSubsystem* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAFLOnlineSubsystem>() : nullptr;
	if (!Online)
	{
		SetState(EAFLMatchmakingState::Failed, NSLOCTEXT("AFL", "MMNoOnline", "Online services unavailable."));
		return;
	}
	if (!Online->IsLoggedIn())
	{
		// Distinct from a refusal: the player is not signed in yet, which is fixable and worth saying plainly.
		SetState(EAFLMatchmakingState::Failed, NSLOCTEXT("AFL", "MMNotSignedIn", "Sign in to play."));
		return;
	}

	SetState(EAFLMatchmakingState::Requesting, NSLOCTEXT("AFL", "MMRequesting", "Joining queue..."));

	// Built by hand rather than a serializer for the same reason the signed bodies are: the shape is two
	// fields and the wire form should be obvious at the call site.
	//
	// STAKE IS OMITTED WHEN ZERO. An unstaked (LEAGUE PLAY) queue REJECTS a stake outright (R85), so sending
	// "stake":0 would turn a valid free match into a 400.
	FString Body;
	if (Stake > 0)
	{
		Body = FString::Printf(TEXT("{\"queueId\":\"%s\",\"stake\":%d}"), *QueueId, Stake);
	}
	else
	{
		Body = FString::Printf(TEXT("{\"queueId\":\"%s\"}"), *QueueId);
	}

	UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MM: /create-ticket queue='%s' stake=%d"), *QueueId, Stake);

	TWeakObjectPtr<UAFLMatchmakingSubsystem> WeakThis(this);
	Online->PostPlayerApi(TEXT("/create-ticket"), Body,
		[WeakThis](bool bOk, const FString& Resp)
		{
			// The player may have quit to desktop while this was in flight.
			UAFLMatchmakingSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}

			if (!bOk)
			{
				// Surface the SERVER's reason. These are the ones a player can act on -- "queue is staked, a
				// positive stake is required", "insufficient balance", "no map backs it yet" -- and replacing
				// them with a generic failure would make every one of them look like a network problem.
				const FText Why = ErrorTextFrom(Resp, NSLOCTEXT("AFL", "MMTicketFailed", "Could not join the queue."));
				UE_LOG(LogAFLMatchmaking, Error, TEXT("AFL_MM: /create-ticket REFUSED -- %s"), *Resp.Left(300));
				Self->SetState(EAFLMatchmakingState::Failed, Why);
				return;
			}

			UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MM: ticket accepted -- %s"), *Resp.Left(200));
			Self->SetState(EAFLMatchmakingState::Queued, NSLOCTEXT("AFL", "MMQueued", "Searching for a match..."));

			// ⚠ THE TRAP, ASSERTED RATHER THAN REMEMBERED. If a previous attempt's clock survived, this queue
			// starts partway up the ladder -- the first poll waits 60s instead of 3s and the PLAY button
			// looks dead from the very first tick, with nothing logged and nothing failing. That is a bug
			// that reads as a network problem. StopPolling -> ResetPollProgress is what clears it; this
			// ensure is what proves the clear actually happened, on every single queue attempt, in every
			// non-shipping build.
			ensureMsgf(Self->ElapsedQueuedSeconds() < 1.0f,
				TEXT("AFL_MM: queue clock was NOT reset -- %.0fs already on it before the first poll. A stale ")
				TEXT("clock starts this attempt on the ladder's 60s tail."), Self->ElapsedQueuedSeconds());

			if (UGameInstance* GI = Self->GetGameInstance())
			{
				if (UWorld* World = GI->GetWorld())
				{
					Self->QueueStartRealSeconds = World->GetRealTimeSeconds();
				}
			}

			// Poll immediately, then on the ladder: at high population a match can be waiting before the
			// first tick would fire, and a player staring at a spinner for three seconds after the match is
			// already placed is three seconds of the match running without them. PollMatchStatus arms the
			// next one itself, so there is no looping timer to set here.
			Self->PollMatchStatus();
		});
}

void UAFLMatchmakingSubsystem::PollMatchStatus()
{
	if (State != EAFLMatchmakingState::Queued)
	{
		StopPolling();
		return;
	}

	// THE CLIENT GIVES UP WHEN THE TICKET DOES, not before. The old cap was 200 polls x 3s = 603s against a
	// staked ticket that FlexMatch holds for 43,200 -- so the player was told "no match found" while the
	// thing that could still match them ran on for another eleven hours, and would have placed them into a
	// staked match they were no longer watching.
	if (ElapsedQueuedSeconds() >= MaxQueueSeconds)
	{
		StopPolling();
		SetState(EAFLMatchmakingState::Failed,
			NSLOCTEXT("AFL", "MMTimeout", "No match found. Try again, or pick a busier queue."));
		return;
	}

	UAFLOnlineSubsystem* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAFLOnlineSubsystem>() : nullptr;
	if (!Online)
	{
		StopPolling();
		SetState(EAFLMatchmakingState::Failed, NSLOCTEXT("AFL", "MMNoOnline2", "Online services unavailable."));
		return;
	}

	// ARMED BEFORE THE REQUEST GOES OUT, not in the response handler. A poll that fails, times out, or comes
	// back unparseable must still schedule the next one -- otherwise one dropped request silently ends the
	// queue while the state still says Queued, which is the failure the "a failed poll is not a failed queue"
	// note below exists to prevent. Arming here makes that structural instead of a property of every early
	// return in the callback.
	ArmNextPoll();

	TWeakObjectPtr<UAFLMatchmakingSubsystem> WeakThis(this);
	Online->PostPlayerApi(TEXT("/match-status"), TEXT("{}"),
		[WeakThis](bool bOk, const FString& Resp)
		{
			UAFLMatchmakingSubsystem* Self = WeakThis.Get();
			if (!Self || Self->State != EAFLMatchmakingState::Queued)
			{
				return;
			}

			// A FAILED POLL IS NOT A FAILED QUEUE. One dropped request while a match is forming must not
			// throw the player out of a queue they may already have matched in. Keep polling; only the
			// 43,200s cap gives up, and the next poll is already armed before this request went out.
			if (!bOk)
			{
				UE_LOG(LogAFLMatchmaking, Warning, TEXT("AFL_MM: /match-status poll failed (continuing) -- %s"), *Resp.Left(160));
				return;
			}

			const TSharedPtr<FJsonObject> Root = ParseJson(Resp);
			if (!Root.IsValid())
			{
				UE_LOG(LogAFLMatchmaking, Warning, TEXT("AFL_MM: /match-status unparseable (continuing)."));
				return;
			}

			FString Status;
			Root->TryGetStringField(TEXT("status"), Status);
			if (Status != TEXT("ready"))
			{
				return;   // "waiting" is the normal answer for most of the queue
			}

			FString Ip, MatchId;
			int32 Port = 0;
			Root->TryGetStringField(TEXT("ipAddress"), Ip);
			Root->TryGetNumberField(TEXT("port"), Port);
			Root->TryGetStringField(TEXT("matchId"), MatchId);

			// ⚠ THE STATUS PAYLOAD NO LONGER CARRIES A playerSessionId, AND MUST NOT. Fix B: a psid minted at
			// placement is dead 60s later, so /match-status answers only the DURABLE half -- where the match
			// is. The id we travel with is claimed below, at travel time. If a psid ever reappears here, read
			// the header on ClaimAndTravel before using it.
			//
			// The address still has to be whole before we spend a claim: claiming against a half-read status
			// would reserve a GameLift slot we then cannot use, and the reservation is not free -- on a 2-slot
			// match a wasted one can lock the other player out.
			if (Ip.IsEmpty() || Port <= 0)
			{
				UE_LOG(LogAFLMatchmaking, Error,
					TEXT("AFL_MM: /match-status said READY but the address is incomplete (ip='%s' port=%d) -- not claiming."),
					*Ip, Port);
				return;
			}

			Self->StopPolling();
			UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MM: MATCH READY match=%s -> %s:%d -- claiming a place."), *MatchId, *Ip, Port);
			Self->SetState(EAFLMatchmakingState::Joining, NSLOCTEXT("AFL", "MMJoining", "Match found. Joining..."));
			Self->ClaimAndTravel(MatchId);
		});
}

void UAFLMatchmakingSubsystem::ClaimAndTravel(const FString& MatchId)
{
	UAFLOnlineSubsystem* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAFLOnlineSubsystem>() : nullptr;
	if (!Online)
	{
		SetState(EAFLMatchmakingState::Failed, NSLOCTEXT("AFL", "MMNoOnlineClaim", "Online services unavailable."));
		return;
	}

	// The request body is EMPTY, and that is the security model rather than laziness. /claim-session names
	// nobody: it resolves the caller from the SessionTicket and reads the game session off that caller's OWN
	// ready row. There is no field in which to name another player's match, so a claim against someone else's
	// session is unconstructible rather than merely rejected.
	TWeakObjectPtr<UAFLMatchmakingSubsystem> WeakThis(this);
	Online->PostPlayerApi(TEXT("/claim-session"), TEXT("{}"),
		[WeakThis, MatchId](bool bOk, const FString& Resp)
		{
			UAFLMatchmakingSubsystem* Self = WeakThis.Get();
			if (!Self || Self->State != EAFLMatchmakingState::Joining)
			{
				return;   // cancelled, or the player quit while the claim was in flight
			}

			if (!bOk)
			{
				// Surface the SERVER's reason. "that match is no longer joinable" is a thing a player can act
				// on; replacing it with a generic failure would make it look like a network problem.
				const FText Why = ErrorTextFrom(Resp, NSLOCTEXT("AFL", "MMClaimFailed", "Could not join the match."));
				UE_LOG(LogAFLMatchmaking, Error, TEXT("AFL_MM: /claim-session REFUSED -- %s"), *Resp.Left(300));
				Self->SetState(EAFLMatchmakingState::Failed, Why);
				return;
			}

			const TSharedPtr<FJsonObject> Root = ParseJson(Resp);
			if (!Root.IsValid())
			{
				UE_LOG(LogAFLMatchmaking, Error, TEXT("AFL_MM: /claim-session unparseable -- not travelling."));
				Self->SetState(EAFLMatchmakingState::Failed, NSLOCTEXT("AFL", "MMClaimBad", "Could not join the match."));
				return;
			}

			FString Ip, Psid;
			int32 Port = 0;
			Root->TryGetStringField(TEXT("ipAddress"), Ip);
			Root->TryGetNumberField(TEXT("port"), Port);
			Root->TryGetStringField(TEXT("playerSessionId"), Psid);

			// ⚠ ALL THREE OR NOTHING -- unchanged in spirit, but now checked against the CLAIMED tuple rather
			// than one that arrived in the status payload. This is the authoritative set: the psid was minted
			// seconds ago and its 60s reservation is running from now. Without it the server REFUSES the
			// connection at PreLogin (S12-E) and the player is bounced to the front end with no explanation
			// while their stake sits escrowed in a match they never joined.
			//
			// FAILED, not "keep polling": polling has already stopped and the claim is a terminal act. A
			// half-answered claim means something is wrong server-side, and silently retrying would spend
			// another GameLift reservation on each attempt.
			if (Ip.IsEmpty() || Port <= 0 || Psid.IsEmpty())
			{
				UE_LOG(LogAFLMatchmaking, Error,
					TEXT("AFL_MM: /claim-session returned an incomplete tuple (ip='%s' port=%d psid='%s') -- not travelling."),
					*Ip, Port, *Psid);
				Self->SetState(EAFLMatchmakingState::Failed, NSLOCTEXT("AFL", "MMClaimBad2", "Could not join the match."));
				return;
			}

			// `reused` is logged, not acted on: it is the backend telling us this call was idempotent (a
			// retry inside the 60s window returned the id it minted before, rather than holding a second
			// reservation). Worth seeing in a log when diagnosing a double-join.
			bool bReused = false;
			Root->TryGetBoolField(TEXT("reused"), bReused);

			UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MM: CLAIMED %s match=%s -> %s:%d"),
				bReused ? TEXT("(reused)") : TEXT("(fresh)"), *MatchId, *Ip, Port);

			Self->TravelToMatch(Ip, Port, Psid);
		});
}

void UAFLMatchmakingSubsystem::TravelToMatch(const FString& IpAddress, int32 Port, const FString& PlayerSessionId)
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		SetState(EAFLMatchmakingState::Failed, NSLOCTEXT("AFL", "MMNoPC", "Could not join: no local player."));
		return;
	}

	// UE URL options separate with '?', NOT '&' -- '&' would be swallowed into the previous option's value
	// and the server would see no PlayerSessionId at all, which since S12-E is a refused connection.
	const FString Url = FString::Printf(TEXT("%s:%d?PlayerSessionId=%s"), *IpAddress, Port, *PlayerSessionId);

	UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MM: ClientTravel -> %s"), *Url);
	PC->ClientTravel(Url, ETravelType::TRAVEL_Absolute);
}

void UAFLMatchmakingSubsystem::CancelMatchmaking()
{
	if (State != EAFLMatchmakingState::Requesting && State != EAFLMatchmakingState::Queued)
	{
		return;
	}
	// TIMER ONLY -- see the note on StopPolling. If this cancel fails the player is still queued and the
	// ladder must resume where it was, not from zero.
	ClearPollTimer();

	// THE TICKET IS NOW ACTUALLY WITHDRAWN. This function used to stop the client listening and nothing else,
	// and said so: "a Cancel button that only stops the client looking is worse than none, because it tells
	// the player something happened that did not." That comment came out with the behaviour it described.
	//
	// It mattered more every phase. At a 120s PlayFab lifetime an orphaned ticket was a two-minute window; a
	// staked FlexMatch ticket lives 43,200s, so a player who cancelled and walked away stayed matchable for
	// twelve hours, with their stake committed to a match nobody was watching.
	UAFLOnlineSubsystem* Online = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAFLOnlineSubsystem>() : nullptr;
	if (!Online)
	{
		// We cannot reach the backend, so we cannot claim the ticket is gone. Say what is true.
		SetState(EAFLMatchmakingState::Failed,
			NSLOCTEXT("AFL", "MMCancelNoOnline", "Could not stop the search -- you may still be in the queue."));
		return;
	}

	// NOT Idle YET. The player is out of the queue when the SERVER says so, not when we stop asking.
	SetState(EAFLMatchmakingState::Cancelling, NSLOCTEXT("AFL", "MMCancelling", "Stopping search..."));

	// AN EMPTY BODY, AND THAT IS THE CONTRACT. /cancel-ticket takes NOTHING from the request -- it resolves
	// the player from the session ticket and finds their live entries through the byPlayerEntry index. There
	// is no queueId or ticketId to send, and sending one would not change what gets cancelled.
	TWeakObjectPtr<UAFLMatchmakingSubsystem> WeakThis(this);
	Online->PostPlayerApi(TEXT("/cancel-ticket"), TEXT("{}"),
		[WeakThis](bool bOk, const FString& Resp)
		{
			UAFLMatchmakingSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}

			if (!bOk)
			{
				// ⚠ BACK TO QUEUED, AND POLLING RESUMES. The ticket was not withdrawn, so the player IS still
				// queued and still matchable -- and if we stopped watching they would be placed into a match
				// they had walked away from. Returning to Idle here would re-create the exact lie this change
				// deleted, just with a shorter fuse.
				UE_LOG(LogAFLMatchmaking, Error, TEXT("AFL_MM: /cancel-ticket FAILED -- still queued. %s"), *Resp.Left(300));
				Self->SetState(EAFLMatchmakingState::Queued,
					NSLOCTEXT("AFL", "MMCancelFailed", "Could not stop the search -- you are still in the queue."));
				Self->ArmNextPoll();
				return;
			}

			// EXPOSURE IS REPORTED SEPARATELY FROM THE CANCEL, because they are two different facts. The
			// search stopped; whether the staked hold came back is its own answer, and `exposureUnaddressable`
			// is the case where it did not. Folding it into a plain "Search stopped." would tell the player
			// their stake was freed when it was not.
			int32 Cancelled = 0;
			bool bAnyUnaddressable = false;
			if (const TSharedPtr<FJsonObject> Root = ParseJson(Resp))
			{
				Root->TryGetNumberField(TEXT("cancelled"), Cancelled);
				const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
				if (Root->TryGetArrayField(TEXT("entries"), Entries) && Entries)
				{
					for (const TSharedPtr<FJsonValue>& E : *Entries)
					{
						const TSharedPtr<FJsonObject>* Obj = nullptr;
						bool bUnaddressable = false;
						if (E.IsValid() && E->TryGetObject(Obj) && Obj &&
							(*Obj)->TryGetBoolField(TEXT("exposureUnaddressable"), bUnaddressable) && bUnaddressable)
						{
							bAnyUnaddressable = true;
						}
					}
				}
			}

			UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MM: /cancel-ticket OK -- %d entr(ies) withdrawn%s"),
				Cancelled, bAnyUnaddressable ? TEXT(", EXPOSURE NOT RELEASED") : TEXT(""));

			// The attempt is genuinely over now, so the clock goes with it.
			Self->ResetPollProgress();
			Self->SetState(EAFLMatchmakingState::Idle, bAnyUnaddressable
				? NSLOCTEXT("AFL", "MMCancelledHeld",
					"Search stopped. Your stake hold could not be released automatically -- it clears within 24 hours.")
				: NSLOCTEXT("AFL", "MMCancelled", "Search stopped."));
		});
}
