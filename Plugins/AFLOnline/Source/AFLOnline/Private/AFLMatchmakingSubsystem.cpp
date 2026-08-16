// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLMatchmakingSubsystem.h"

#include "AFLOnlineSubsystem.h"
#include "Containers/Ticker.h"                  // FTSTicker -- the multi-queue canary's step pump
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CheatManagerDefines.h"  // UE_WITH_CHEAT_MANAGER -- without it the commands SILENTLY vanish
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"                // FAutoConsoleCommandWithWorldArgsAndOutputDevice
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

	// ⚠ CLEARED ON THE WAY OUT OF QUEUING, NOT IN EVERY CALLER. Idle and Failed are the only states that mean
	// "no ticket of ours is live"; Cancelling still has one until the server answers, and a cancel that FAILS
	// deliberately returns to Queued with the ticket intact. Clearing at each call site would need five
	// correct decisions instead of one, and the failure mode is a lobby row offering to cancel a cell the
	// player is not in -- or worse, not offering it for one they are.
	//
	// ⚠ SAFE ONLY BECAUSE Failed IS NO LONGER SET WHILE ENTRIES SURVIVE. A refused join with two live entries
	// used to land here as Failed and would now wipe both. That path goes through RederiveState instead, which
	// cannot produce Failed -- Failed is reached only by the give-up cap and by fatal service errors, where
	// every entry really is over. If a future call site sets Failed with entries still live, it is dropping
	// them on the floor and this is where it happens.
	if (NewState == EAFLMatchmakingState::Idle || NewState == EAFLMatchmakingState::Failed)
	{
		Entries.Reset();
	}

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
	// anywhere. Hence one function.
	//
	// THE CLOCK IS NOW THE ENTRY LIST ITSELF. Each entry carries its own start stamp, so "reset the progress"
	// is "no entries are live" -- there is no separate scalar left to drift out of step. Clearing entries here
	// would be wrong, though: this runs on cancel paths that have already decided what survives.
	ClearPollTimer();
}

float UAFLMatchmakingSubsystem::ElapsedQueuedSeconds() const
{
	// THE OLDEST ENTRY GOVERNS, and that is not an arbitrary pick. FlexMatch commits a bot-permitted match on
	// `expansionAgeSelection: 'oldest'` -- the longest-waiting ticket in a partial match is what fires the
	// expansion -- so the oldest entry is also the one whose deadline actually arrives first. Running the poll
	// ladder off the newest would put the client on the 3s rung for a player who has been waiting ten minutes,
	// and running it off an average would track nothing real.
	double Oldest = 0.0;
	for (const FEntry& Entry : Entries)
	{
		if (Entry.StartRealSeconds > 0.0 && (Oldest <= 0.0 || Entry.StartRealSeconds < Oldest))
		{
			Oldest = Entry.StartRealSeconds;
		}
	}
	if (Oldest <= 0.0)
	{
		return 0.0f;
	}
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	return World ? static_cast<float>(World->GetRealTimeSeconds() - Oldest) : 0.0f;
}

bool UAFLMatchmakingSubsystem::IsQueuedIn(const FString& InQueueId) const
{
	if (InQueueId.IsEmpty())
	{
		return false;
	}
	return Entries.ContainsByPredicate([&InQueueId](const FEntry& E) { return E.QueueId == InQueueId; });
}

TArray<FString> UAFLMatchmakingSubsystem::GetQueuedQueueIds() const
{
	TArray<FString> Out;
	Out.Reserve(Entries.Num());
	for (const FEntry& Entry : Entries)
	{
		Out.Add(Entry.QueueId);
	}
	return Out;
}

FString UAFLMatchmakingSubsystem::GetOldestQueuedQueueId() const
{
	const FEntry* Best = nullptr;
	for (const FEntry& Entry : Entries)
	{
		if (!Best || Entry.StartRealSeconds < Best->StartRealSeconds)
		{
			Best = &Entry;
		}
	}
	return Best ? Best->QueueId : FString();
}

void UAFLMatchmakingSubsystem::RederiveState(const FText& Reason)
{
	// ⚠ STATE IS DERIVED FROM THE ENTRY LIST, NOT ASSIGNED BY WHOEVER ACTED LAST. With one entry the two were
	// the same thing and assignment was fine. With several they are not: a REFUSED join while two entries are
	// live must not set Failed, because the player is very much still queued -- and the old code set Failed
	// from the refusal handler unconditionally. Deriving makes that class of bug unrepresentable rather than
	// something every future call site has to remember.
	//
	// Joining is terminal and Cancelling is a claim about an in-flight request, so neither is derivable from
	// the list; both are still set explicitly and this function is not called on those paths.
	const EAFLMatchmakingState Derived =
		Entries.Num() > 0            ? EAFLMatchmakingState::Queued
		: RequestsInFlight > 0       ? EAFLMatchmakingState::Requesting
		:                              EAFLMatchmakingState::Idle;

	SetState(Derived, Reason);
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
	// ⚠ THE BLANKET GUARD IS GONE, AND WHAT REPLACES IT IS NARROWER ON PURPOSE.
	//
	// It used to refuse outright while Requesting or Queued -- one entry, ever. That made the backend's
	// first-to-fill-wins design unreachable: /cancel-ticket grew a per-cell selector, the allocator learned to
	// withdraw superseded entries and release their exposure, and no player could produce the second entry any
	// of it existed for.
	//
	// What remains is a DUPLICATE guard, and it is not cosmetic. `bagman-queue-ticket-index` is keyed
	// (queueId, playFabId), so a second ticket in the SAME cell overwrites that row -- and the row is the only
	// record of the first ticket's id and its exposureKey. The first FlexMatch ticket would keep searching
	// with nothing able to name it and its play-limit reservation would be stranded until the window rolled.
	// One entry per cell is a storage invariant, not a preference.
	if (IsQueuedIn(QueueId))
	{
		UE_LOG(LogAFLMatchmaking, Warning,
			TEXT("AFL_MM: StartMatchmaking ignored -- already queued in %s. A second ticket in one cell would "
			     "overwrite its index row and strand the first."), *QueueId);
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

	// ⚠ COUNTED BEFORE THE STATE IS DERIVED, because RederiveState reads it. An in-flight join is what makes
	// Requesting true when no entry has been accepted yet; with entries already live the derived state stays
	// Queued, which is correct -- the player IS queued, and is additionally joining somewhere else.
	++RequestsInFlight;
	RederiveState(NSLOCTEXT("AFL", "MMRequesting", "Joining queue..."));

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
		[WeakThis, QueueId, Stake](bool bOk, const FString& Resp)
		{
			// The player may have quit to desktop while this was in flight.
			UAFLMatchmakingSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}

			Self->RequestsInFlight = FMath::Max(0, Self->RequestsInFlight - 1);

			if (!bOk)
			{
				// Surface the SERVER's reason. These are the ones a player can act on -- "queue is staked, a
				// positive stake is required", "insufficient balance", "no map backs it yet" -- and replacing
				// them with a generic failure would make every one of them look like a network problem.
				const FText Why = ErrorTextFrom(Resp, NSLOCTEXT("AFL", "MMTicketFailed", "Could not join the queue."));
				UE_LOG(LogAFLMatchmaking, Error, TEXT("AFL_MM: /create-ticket REFUSED for %s -- %s"),
					*QueueId, *Resp.Left(300));

				// ⚠ A REFUSED JOIN IS NOT A FAILED SESSION WHEN OTHER ENTRIES ARE LIVE. This was an
				// unconditional SetState(Failed), which was right while one entry was the maximum and becomes
				// destructive the moment it is not: Failed clears the entry list, so being turned away from a
				// third cell would have silently dropped the two the player was still queued in -- and they
				// would remain live at FlexMatch, unwatched, with the client reporting Idle.
				//
				// RederiveState yields Queued while entries survive and Failed is not reachable from it, so
				// the reason is still delivered and nothing is thrown away.
				if (Self->Entries.Num() > 0)
				{
					Self->RederiveState(Why);
				}
				else
				{
					Self->SetState(EAFLMatchmakingState::Failed, Why);
				}
				return;
			}

			UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MM: ticket accepted for %s -- %s"), *QueueId, *Resp.Left(200));

			// ⚠ THE STALE-CLOCK ENSURE THAT USED TO LIVE HERE IS GONE, AND ITS REMOVAL IS NOT A RELAXATION.
			// It asserted ElapsedQueuedSeconds() < 1.0f on every accepted ticket, to catch a previous
			// attempt's clock surviving into a new one. Under multi-entry that condition is LEGITIMATELY
			// false: joining a second cell while the first has been waiting five minutes reports 300s,
			// because the elapsed figure is deliberately the OLDEST entry's. The assert would fire on correct
			// behaviour, every time, which is worse than not having it -- an ensure that cries wolf gets
			// disabled and takes the real signal with it. The bug it guarded is now structurally impossible:
			// the clock is per-entry and an entry is stamped at the moment it is created, below.
			FEntry Entry;
			Entry.QueueId = QueueId;
			Entry.Stake = Stake;
			if (const UGameInstance* GI = Self->GetGameInstance())
			{
				if (const UWorld* World = GI->GetWorld())
				{
					Entry.StartRealSeconds = World->GetRealTimeSeconds();
				}
			}
			Self->Entries.Add(MoveTemp(Entry));

			Self->RederiveState(Self->Entries.Num() > 1
				? FText::Format(NSLOCTEXT("AFL", "MMQueuedMulti", "Searching {0} queues..."),
					FText::AsNumber(Self->Entries.Num()))
				: NSLOCTEXT("AFL", "MMQueued", "Searching for a match..."));

			// Poll immediately, then on the ladder: at high population a match can be waiting before the
			// first tick would fire, and a player staring at a spinner for three seconds after the match is
			// already placed is three seconds of the match running without them. PollMatchStatus arms the
			// next one itself, so there is no looping timer to set here.
			//
			// ⚠ ONE POLL SERVES EVERY ENTRY. /match-status is keyed by the PLAYER -- it posts an empty body and
			// the server resolves identity from the session ticket -- so a second cell adds no polling load
			// and needs no second timer. That is what makes multi-entry cheap on the client. Re-polling on an
			// additional join is still right: it re-arms from the ladder position of the OLDEST entry.
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
	// EMPTY = EVERY CELL, which is what a bare Cancel button has always meant and must keep meaning.
	CancelQueue(FString());
}

void UAFLMatchmakingSubsystem::CancelQueue(const FString& QueueId)
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

	// THE BODY CARRIES A SELECTOR AND NEVER AN IDENTITY. /cancel-ticket still resolves WHO from the session
	// ticket and finds their entries through the byPlayerEntry index; `queueId` only narrows that set to one
	// cell. An empty body means every cell, which is the shipped behaviour and the bare Cancel button.
	//
	// Sending no field at all rather than `"queueId":""` on the all-path: the endpoint treats both as "all",
	// but an absent key is the request a build predating this made, and keeping them byte-identical means the
	// common path is the one already proven in the gate.
	const FString Body = QueueId.IsEmpty()
		? FString(TEXT("{}"))
		: FString::Printf(TEXT("{\"queueId\":\"%s\"}"), *QueueId);

	TWeakObjectPtr<UAFLMatchmakingSubsystem> WeakThis(this);
	Online->PostPlayerApi(TEXT("/cancel-ticket"), Body,
		[WeakThis, QueueId](bool bOk, const FString& Resp)
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
			// HOW MANY ENTRIES ARE STILL LIVE AFTER THIS CALL. Zero on a bare Cancel by construction; non-zero
			// only when one cell was named and the player remains in others. The server reports it because the
			// client cannot infer it -- it never knew how many cells the player was in.
			int32 StillQueued = 0;
			bool bAnyUnaddressable = false;
			if (const TSharedPtr<FJsonObject> Root = ParseJson(Resp))
			{
				Root->TryGetNumberField(TEXT("cancelled"), Cancelled);
				Root->TryGetNumberField(TEXT("stillQueued"), StillQueued);
				// Renamed off `Entries` -- the subsystem now HAS a member by that name and shadowing the live
				// entry list with a JSON array inside its own cancel handler is a trap waiting for an edit.
				const TArray<TSharedPtr<FJsonValue>>* EntryArray = nullptr;
				if (Root->TryGetArrayField(TEXT("entries"), EntryArray) && EntryArray)
				{
					for (const TSharedPtr<FJsonValue>& E : *EntryArray)
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

			UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MM: /cancel-ticket OK -- %d entr(ies) withdrawn%s, %d still live"),
				Cancelled, bAnyUnaddressable ? TEXT(", EXPOSURE NOT RELEASED") : TEXT(""), StillQueued);

			// DROP WHAT WAS WITHDRAWN. A named cell removes that entry; an empty QueueId was a bare Cancel and
			// takes every one. Done from the SERVER's answer rather than optimistically at the call site: the
			// entry is not gone until it says so, and a cancel that fails leaves the list untouched below.
			if (QueueId.IsEmpty())
			{
				Self->Entries.Reset();
			}
			else
			{
				Self->Entries.RemoveAll([&QueueId](const FEntry& E) { return E.QueueId == QueueId; });
			}

			// ⚠ THE SERVER'S COUNT IS THE ONE THAT DECIDES, NOT OUR LIST. They can disagree: an entry that timed
			// out or was swept is gone server-side while the client still lists it, and the client cannot see
			// either event. Trusting `stillQueued` means a stale local entry cannot keep the player in a
			// Queued state with nothing behind it -- the direction that matters, because the opposite mistake
			// leaves someone believing they are searching when they are not.
			if (StillQueued != Self->Entries.Num())
			{
				UE_LOG(LogAFLMatchmaking, Warning,
					TEXT("AFL_MM: entry list disagreed with the server after cancel -- local %d, server %d. ")
					TEXT("Trusting the server."), Self->Entries.Num(), StillQueued);
				if (StillQueued <= 0)
				{
					Self->Entries.Reset();
				}
			}

			// ⚠ STILL QUEUED IS NOT IDLE, AND POLLING MUST NOT STOP. Cancelling one cell out of three leaves a
			// player matchable in two, and going Idle here would be the same lie this whole endpoint was built
			// to delete -- just aimed at the cells the player deliberately kept. The clock keeps running too:
			// they have been waiting in those cells the whole time, so resetting the ladder would restart an
			// estimate that never paused.
			if (StillQueued > 0)
			{
				Self->RederiveState(bAnyUnaddressable
					? NSLOCTEXT("AFL", "MMCancelledOneHeld",
						"Left that queue. Your stake hold could not be released automatically -- it clears within 24 hours.")
					: NSLOCTEXT("AFL", "MMCancelledOne", "Left that queue -- still searching the others."));
				Self->ArmNextPoll();
				return;
			}

			// The attempt is genuinely over now, so the clock goes with it.
			Self->ResetPollProgress();
			Self->SetState(EAFLMatchmakingState::Idle, bAnyUnaddressable
				? NSLOCTEXT("AFL", "MMCancelledHeld",
					"Search stopped. Your stake hold could not be released automatically -- it clears within 24 hours.")
				: NSLOCTEXT("AFL", "MMCancelled", "Search stopped."));
		});
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// MULTI-QUEUE HARNESS -- dev-only console commands, and the scripted canary that proves the feature.
//
// âš  WHY A CANARY AND NOT "CLICK TWO ROWS AND LOOK". Multi-queue's claims are about STATE, not pixels: that a
// second entry is accepted while the first is live, that leaving one cell keeps the other, and that a refused
// join does not wipe entries that survive. Every one of those is a proposition with a truth value, and this
// project's doctrine prefers a deterministic assertion over an eyeball for exactly that kind of claim.
// Watching a lobby cannot distinguish "two entries" from "one entry drawn twice", and cannot see the entry
// list at all.
//
// What it does NOT prove, and no console command could: that the LEAVE control renders where a person expects
// it. That still needs eyes on the screen.
//
// Uses FREE LEAGUE PLAY cells by default and never defaults to anything staked -- the canary creates REAL
// tickets against the DEPLOYED backend, and a harness that quietly moves money is not a harness.
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

#if UE_WITH_CHEAT_MANAGER

namespace AFLMultiQueueCanary
{
	/** Default pair: two DIFFERENT free cells, so a pass cannot come from one cell answering twice. */
	static const TCHAR* DefaultQueueA = TEXT("LeaguePlay_Haywire_MatchPlay_Arena_2v2");
	static const TCHAR* DefaultQueueB = TEXT("LeaguePlay_Haywire_MatchPlay_Map_1v1");

	static UAFLMatchmakingSubsystem* Get(UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UAFLMatchmakingSubsystem>() : nullptr;
	}

	static void LogState(UAFLMatchmakingSubsystem* MM, const TCHAR* Tag)
	{
		const TArray<FString> Ids = MM->GetQueuedQueueIds();
		UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MQ %s count=%d [%s]"),
			Tag, MM->GetQueuedCount(), *FString::Join(Ids, TEXT(", ")));
	}

	/**
	 * The scripted run. A ticker rather than nested callbacks: every step is "wait until a PREDICATE about the
	 * entry list holds, or time out", which is what makes each assertion independent of how many round trips
	 * the step happened to take.
	 */
	struct FRun
	{
		TWeakObjectPtr<UAFLMatchmakingSubsystem> MM;
		FString A, B;
		int32 Step = 0;
		double StepStarted = 0.0;
		int32 Failures = 0;

		static constexpr double StepTimeout = 25.0;

		void Fail(const TCHAR* What)
		{
			++Failures;
			UE_LOG(LogAFLMatchmaking, Error, TEXT("AFL_MQ_CANARY FAIL step=%d -- %s"), Step, What);
		}

		void Advance(double Now) { ++Step; StepStarted = Now; }

		void Finish(UAFLMatchmakingSubsystem* M)
		{
			LogState(M, TEXT("final"));
			// ALWAYS clean up. A canary that leaves live tickets behind poisons the next run -- and step 0
			// refuses to start dirty, so it would poison it visibly rather than silently. Bare cancel = all.
			M->CancelMatchmaking();
			UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MQ_CANARY RESULT %s (failures=%d)"),
				Failures == 0 ? TEXT("PASS") : TEXT("FAIL"), Failures);
		}

		bool Tick(float)
		{
			UAFLMatchmakingSubsystem* M = MM.Get();
			if (!M)
			{
				UE_LOG(LogAFLMatchmaking, Error, TEXT("AFL_MQ_CANARY ABORT -- subsystem gone"));
				return false;
			}

			const double Now = FPlatformTime::Seconds();
			const bool bTimedOut = (Now - StepStarted) > StepTimeout;

			switch (Step)
			{
			case 0:
			{
				// ⚠ WAIT FOR LOGIN FIRST, WITH A LONGER BUDGET THAN A NORMAL STEP. This canary is meant to be
				// driven by -ExecCmds at launch, which fires as soon as a console exists -- comfortably before
				// PlayFab has answered. Without this gate every run would fail at "cell A never became live"
				// and the diagnosis would be a matchmaking bug that does not exist.
				UGameInstance* GI = M->GetGameInstance();
				UAFLOnlineSubsystem* Online = GI ? GI->GetSubsystem<UAFLOnlineSubsystem>() : nullptr;
				if (!Online || !Online->IsLoggedIn())
				{
					if ((Now - StepStarted) > 90.0)
					{
						Fail(TEXT("not signed in after 90s -- the canary never got to test anything"));
						Finish(M);
						return false;
					}
					return true;   // keep waiting; deliberately do NOT advance
				}
				UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MQ_CANARY signed in -- proceeding"));
				Advance(Now);
				break;
			}

			case 1:
				UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MQ_CANARY BEGIN A=%s B=%s"), *A, *B);
				LogState(M, TEXT("before"));
				if (M->GetQueuedCount() != 0)
				{
					// Refuse to start dirty. A pass that began with entries already live would prove nothing.
					Fail(TEXT("player is ALREADY queued -- run afl.MM.Leave first"));
					Finish(M);
					return false;
				}
				M->StartMatchmaking(A, 0);
				Advance(Now);
				break;

			case 2:   // A accepted
				if (M->IsQueuedIn(A))
				{
					LogState(M, TEXT("A-live"));
					M->StartMatchmaking(B, 0);
					Advance(Now);
				}
				else if (bTimedOut) { Fail(TEXT("cell A never became live")); Finish(M); return false; }
				break;

			case 3:   // B accepted ALONGSIDE A -- THE CLAIM
				if (M->IsQueuedIn(A) && M->IsQueuedIn(B))
				{
					LogState(M, TEXT("both-live"));
					UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MQ_CANARY PASS-1 two entries live at once"));
					if (M->GetQueuedCount() != 2) { Fail(TEXT("both cells report live but count != 2")); }
					M->CancelQueue(A);
					Advance(Now);
				}
				else if (bTimedOut)
				{
					Fail(TEXT("second entry never became live -- the guard may still be refusing it"));
					Finish(M);
					return false;
				}
				break;

			case 4:   // A gone, B survives -- THE OTHER CLAIM
				if (!M->IsQueuedIn(A) && M->IsQueuedIn(B))
				{
					LogState(M, TEXT("after-leave-A"));
					UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MQ_CANARY PASS-2 per-cell leave kept the other entry"));
					if (M->GetQueuedCount() != 1) { Fail(TEXT("B survives but count != 1")); }
					Advance(Now);
				}
				else if (!M->IsQueuedIn(A) && !M->IsQueuedIn(B))
				{
					// The failure this whole design exists to prevent: leaving one cell took both.
					Fail(TEXT("leaving A ALSO dropped B -- per-cell cancel is behaving as cancel-all"));
					Finish(M);
					return false;
				}
				else if (bTimedOut) { Fail(TEXT("cell A never cleared")); Finish(M); return false; }
				break;

			default:
				Finish(M);
				return false;
			}
			return true;
		}
	};

	static TSharedPtr<FRun> ActiveRun;
}

static void HandleAFLMMStatus(const TArray<FString>&, UWorld* World, FOutputDevice& Ar)
{
	UAFLMatchmakingSubsystem* MM = AFLMultiQueueCanary::Get(World);
	if (!MM) { Ar.Log(TEXT("afl.MM.Status - no matchmaking subsystem (need a game world).")); return; }
	Ar.Logf(TEXT("afl.MM.Status state=%d count=%d oldest=%s"),
		static_cast<int32>(MM->GetState()), MM->GetQueuedCount(), *MM->GetOldestQueuedQueueId());
	for (const FString& Id : MM->GetQueuedQueueIds()) { Ar.Logf(TEXT("   entry: %s"), *Id); }
}

static void HandleAFLMMQueue(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UAFLMatchmakingSubsystem* MM = AFLMultiQueueCanary::Get(World);
	if (!MM) { Ar.Log(TEXT("afl.MM.Queue - no matchmaking subsystem.")); return; }
	if (Args.Num() == 0) { Ar.Log(TEXT("afl.MM.Queue <queueId> [stake]")); return; }
	const int32 Stake = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
	Ar.Logf(TEXT("afl.MM.Queue -> %s (stake=%d)"), *Args[0], Stake);
	MM->StartMatchmaking(Args[0], Stake);
}

static void HandleAFLMMLeave(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UAFLMatchmakingSubsystem* MM = AFLMultiQueueCanary::Get(World);
	if (!MM) { Ar.Log(TEXT("afl.MM.Leave - no matchmaking subsystem.")); return; }
	const FString Which = Args.Num() > 0 ? Args[0] : FString();
	Ar.Logf(TEXT("afl.MM.Leave -> %s"), Which.IsEmpty() ? TEXT("<ALL>") : *Which);
	MM->CancelQueue(Which);
}

static void HandleAFLMMVerify(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UAFLMatchmakingSubsystem* MM = AFLMultiQueueCanary::Get(World);
	if (!MM) { Ar.Log(TEXT("afl.MM.VerifyMultiQueue - no matchmaking subsystem.")); return; }

	using namespace AFLMultiQueueCanary;
	if (ActiveRun.IsValid()) { Ar.Log(TEXT("afl.MM.VerifyMultiQueue - a run is already in progress.")); return; }

	TSharedPtr<FRun> Run = MakeShared<FRun>();
	Run->MM = MM;
	Run->A = (Args.Num() > 0 && !Args[0].IsEmpty()) ? Args[0] : DefaultQueueA;
	Run->B = (Args.Num() > 1 && !Args[1].IsEmpty()) ? Args[1] : DefaultQueueB;
	Run->StepStarted = FPlatformTime::Seconds();

	if (Run->A == Run->B)
	{
		// Two names for one cell would "pass" step 2 the moment the duplicate guard refused the second join
		// while the first entry was still live -- a green light for the exact bug the guard prevents.
		Ar.Log(TEXT("afl.MM.VerifyMultiQueue - A and B must be DIFFERENT cells."));
		return;
	}

	ActiveRun = Run;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Run](float Dt) -> bool
		{
			const bool bContinue = Run->Tick(Dt);
			if (!bContinue) { AFLMultiQueueCanary::ActiveRun.Reset(); }
			return bContinue;
		}), 0.5f);

	Ar.Logf(TEXT("afl.MM.VerifyMultiQueue started -- A=%s B=%s. Watch the log for AFL_MQ_CANARY."), *Run->A, *Run->B);
}

FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLMMStatusCmd(TEXT("afl.MM.Status"),
	TEXT("Print the live matchmaking entry list (state, count, every queued cell)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLMMStatus));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLMMQueueCmd(TEXT("afl.MM.Queue"),
	TEXT("Enter a cell: afl.MM.Queue <queueId> [stake]. Multi-entry is permitted; the same cell twice is not."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLMMQueue));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLMMLeaveCmd(TEXT("afl.MM.Leave"),
	TEXT("Leave ONE cell: afl.MM.Leave <queueId>. No argument leaves every cell."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLMMLeave));

FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLMMVerifyCmd(TEXT("afl.MM.VerifyMultiQueue"),
	TEXT("Scripted canary: queue A, queue B alongside it, assert BOTH are live, leave A only, assert B survives. Args: [queueA] [queueB], defaulting to two free LeaguePlay cells. Logs AFL_MQ_CANARY PASS-1/PASS-2 and a final RESULT line."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLMMVerify));

#endif // UE_WITH_CHEAT_MANAGER

