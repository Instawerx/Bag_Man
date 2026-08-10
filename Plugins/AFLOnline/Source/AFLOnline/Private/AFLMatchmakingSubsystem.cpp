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

void UAFLMatchmakingSubsystem::StopPolling()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().ClearTimer(PollTimer);
		}
	}
	PollCount = 0;
}

void UAFLMatchmakingSubsystem::StartMatchmaking(const FString& QueueId, int32 Stake)
{
	if (State == EAFLMatchmakingState::Requesting || State == EAFLMatchmakingState::Queued)
	{
		UE_LOG(LogAFLMatchmaking, Warning, TEXT("AFL_MM: StartMatchmaking ignored -- already in the queue."));
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

			// Poll immediately, then on the interval: at high population a match can be waiting before the
			// first tick would fire, and a player staring at a spinner for three seconds after the match is
			// already placed is three seconds of the match running without them.
			Self->PollCount = 0;
			Self->PollMatchStatus();
			if (UGameInstance* GI = Self->GetGameInstance())
			{
				if (UWorld* World = GI->GetWorld())
				{
					World->GetTimerManager().SetTimer(Self->PollTimer, Self,
						&UAFLMatchmakingSubsystem::PollMatchStatus, PollIntervalSeconds, /*bLoop=*/true);
				}
			}
		});
}

void UAFLMatchmakingSubsystem::PollMatchStatus()
{
	if (State != EAFLMatchmakingState::Queued)
	{
		StopPolling();
		return;
	}

	if (++PollCount > MaxPolls)
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
			// throw the player out of a queue they may already have matched in. Keep polling; MaxPolls is
			// the only thing that gives up.
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

			FString Ip, Psid, MatchId;
			int32 Port = 0;
			Root->TryGetStringField(TEXT("ipAddress"), Ip);
			Root->TryGetNumberField(TEXT("port"), Port);
			Root->TryGetStringField(TEXT("playerSessionId"), Psid);
			Root->TryGetStringField(TEXT("matchId"), MatchId);

			// ⚠ ALL THREE OR NOTHING. Travelling on a partial tuple is worse than not travelling: without a
			// playerSessionId the server REFUSES the connection at PreLogin (S12-E), so the player would be
			// bounced to the front end with no explanation while their stake sits escrowed in a match they
			// never joined. Stay in the queue and let the next poll -- or the timeout -- decide.
			if (Ip.IsEmpty() || Port <= 0 || Psid.IsEmpty())
			{
				UE_LOG(LogAFLMatchmaking, Error,
					TEXT("AFL_MM: /match-status said READY but the tuple is incomplete (ip='%s' port=%d psid='%s') -- not travelling."),
					*Ip, Port, *Psid);
				return;
			}

			Self->StopPolling();
			UE_LOG(LogAFLMatchmaking, Log, TEXT("AFL_MM: MATCH READY match=%s -> %s:%d"), *MatchId, *Ip, Port);
			Self->SetState(EAFLMatchmakingState::Joining, NSLOCTEXT("AFL", "MMJoining", "Match found. Joining..."));
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
	StopPolling();

	// ⚠ THIS STOPS US LISTENING; IT DOES NOT WITHDRAW THE PLAYFAB TICKET. The ticket keeps living server-side
	// and may still match, at which point the player has been placed in a game they are no longer watching
	// for -- and in a staked tier their stake is escrowed against it.
	//
	// Cancelling properly needs a backend counterpart (a /cancel-ticket that calls PlayFab
	// CancelMatchmakingTicket for the caller). Deliberately NOT faked here: a Cancel button that only stops
	// the client looking is worse than none, because it tells the player something happened that did not.
	UE_LOG(LogAFLMatchmaking, Warning,
		TEXT("AFL_MM: cancelled locally -- the PlayFab ticket is NOT withdrawn (needs a /cancel-ticket endpoint)."));
	SetState(EAFLMatchmakingState::Idle, NSLOCTEXT("AFL", "MMCancelled", "Search stopped."));
}
