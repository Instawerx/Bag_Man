// Copyright C12 AI Gaming. All Rights Reserved.

#include "Online/AFLQueueDirectorySubsystem.h"

#include "AFLGameCore.h"                 // LogAFLGameCore
#include "AFLOnlineSubsystem.h"
#include "Algo/Count.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLQueueDirectorySubsystem)

namespace
{
	/**
	 * A JSON number field that is legitimately `null`.
	 *
	 * ⚠ THIS IS THE HONESTY RULE AT THE WIRE. `playersMatching` and `estimatedWaitSeconds` are documented as
	 * `number | null`, and `null` means WE COULD NOT READ IT -- not zero. `TryGetNumberField` fails on a null
	 * and leaves its out-param untouched, so a caller that ignored the return value would silently keep
	 * whatever was in the variable. Returning INDEX_NONE makes the absence explicit and impossible to
	 * mistake for a population.
	 */
	int32 ReadNullableInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		double Value = 0.0;
		if (Obj.IsValid() && Obj->TryGetNumberField(Field, Value))
		{
			return FMath::RoundToInt(Value);
		}
		return INDEX_NONE;
	}
}

namespace
{
	const TCHAR* StateName(EAFLPopulationState State)
	{
		switch (State)
		{
		case EAFLPopulationState::Live:    return TEXT("live");
		case EAFLPopulationState::Warm:    return TEXT("warm");
		case EAFLPopulationState::Stalled: return TEXT("STALLED");
		case EAFLPopulationState::Cold:    return TEXT("cold");
		case EAFLPopulationState::NotOpen: return TEXT("NOT OPEN");
		default:                           return TEXT("unknown");
		}
	}

	/**
	 * `afl.Lobby.Directory` -- read the live ladder and print it.
	 *
	 * The unit tests hold the parsers against captured bodies, which proves the SHAPE. This proves the PATH:
	 * config -> endpoint -> parse -> join, in the real engine, against whatever the stack is actually
	 * serving. A parser that is correct about a fixture and wrong about production is the failure this
	 * catches, and it is not one a test can catch by construction.
	 */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLobbyDirectoryCmd(
		TEXT("afl.Lobby.Directory"),
		TEXT("Refresh the S1 queue directory from /queues + /population and print the joined ladder. Client-safe; both endpoints are public reads."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				UAFLQueueDirectorySubsystem* Directory = UAFLQueueDirectorySubsystem::Get(World);
				if (!Directory)
				{
					Ar.Log(TEXT("afl.Lobby.Directory -- no game instance."));
					return;
				}

				// Optional filter, e.g. `afl.Lobby.Directory LeaguePlay_Haywire_MatchPlay_Arena`. 72 cells is
				// a lot of console output when you only want to see one door's ladder.
				const FString Filter = Args.Num() > 0 ? Args[0] : FString();

				// ⚠ THE DEFERRED HALF LOGS, IT DOES NOT WRITE TO `Ar`. The FOutputDevice belongs to the
				// console invocation and is gone by the time an HTTP round trip lands -- capturing it by
				// reference here would be a dangling write on every successful run, which is the kind of
				// thing that survives testing because it usually gets away with it.
				//
				// One-shot: print on the NEXT settle, then unsubscribe. Printing the current set inline
				// would show the PREVIOUS read and make a working refresh look like a no-op.
				TWeakObjectPtr<UAFLQueueDirectorySubsystem> WeakDirectory(Directory);
				TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
				*Handle = Directory->OnUpdated.AddLambda(
					[WeakDirectory, Handle, Filter](EAFLQueueDirectoryState State)
					{
						UAFLQueueDirectorySubsystem* Self = WeakDirectory.Get();
						if (!Self)
						{
							return;
						}
						Self->OnUpdated.Remove(*Handle);

						if (State != EAFLQueueDirectoryState::Ready)
						{
							UE_LOG(LogAFLGameCore, Error, TEXT("AFL_QUEUES: directory refresh FAILED -- no ladder."));
							return;
						}

						int32 Shown = 0;
						UE_LOG(LogAFLGameCore, Log, TEXT("AFL_QUEUES: %d cells, population %s%s"),
							Self->GetQueues().Num(),
							Self->IsPopulationKnown() ? TEXT("read") : TEXT("UNAVAILABLE -- every cell reads `Count unavailable`"),
							Filter.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", filter '%s'"), *Filter));

						for (const FAFLLobbyQueue& Queue : Self->GetQueues())
						{
							if (!Filter.IsEmpty() && !Queue.QueueId.Contains(Filter))
							{
								continue;
							}
							UE_LOG(LogAFLGameCore, Log, TEXT("AFL_QUEUES:   %-48s %-9s waiting=%s wait=%s bands=%d"),
								*Queue.QueueId,
								StateName(Queue.State),
								Queue.HasCount() ? *FString::FromInt(Queue.PlayersMatching) : TEXT("--"),
								Queue.HasEstimate() ? *FString::Printf(TEXT("%ds"), Queue.EstimatedWaitSeconds) : TEXT("--"),
								Queue.Bands.Num());
							++Shown;
						}
						UE_LOG(LogAFLGameCore, Log, TEXT("AFL_QUEUES: %d shown."), Shown);
					});

				Directory->Refresh();
				Ar.Log(TEXT("afl.Lobby.Directory -- refreshing; the ladder prints to the log when both reads land."));
			}));

	/**
	 * `afl.Lobby.Band <tier> <amount>` -- prove the R59 round trip against the live endpoint.
	 *
	 * The unit tests cannot cover this: the whole point of the band is that the CLIENT DOES NOT COMPUTE IT,
	 * so the only thing worth verifying is that the real server answers and the real client renders what it
	 * said. Defaults to the staked door's worked example.
	 */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLobbyBandCmd(
		TEXT("afl.Lobby.Band"),
		TEXT("Resolve a stake band server-side: afl.Lobby.Band [VoltsPlay|WattsPlay|LeaguePlay] [amount]. Prints to the log."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				UAFLQueueDirectorySubsystem* Directory = UAFLQueueDirectorySubsystem::Get(World);
				if (!Directory)
				{
					Ar.Log(TEXT("afl.Lobby.Band -- no game instance."));
					return;
				}

				EAFLPlayTier Tier = EAFLPlayTier::VoltsPlay;
				if (Args.Num() > 0) { FAFLLobbyQueueId::FromWire(Args[0], Tier); }
				const int32 Amount = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 450;

				// Logs rather than writing to Ar: the output device belongs to this console invocation and
				// is gone by the time the round trip lands.
				Directory->ResolveBand(Tier, Amount,
					FAFLOnBandResolved::CreateLambda([Amount](bool bOk, const FText& Label, bool bInSomeBand)
					{
						UE_LOG(LogAFLGameCore, Log,
							TEXT("AFL_BAND: amount=%d ok=%s inBand=%s label='%s'"),
							Amount, bOk ? TEXT("yes") : TEXT("NO"),
							bInSomeBand ? TEXT("yes") : TEXT("no"), *Label.ToString());
					}));
				Ar.Log(TEXT("afl.Lobby.Band -- asked; the answer prints to the log."));
			}));

	/** `afl.Lobby.Presence` -- read the measured online count. Expected to print 0 with nobody playing. */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLobbyPresenceCmd(
		TEXT("afl.Lobby.Presence"),
		TEXT("Read the measured online count from GET /presence. 0 is the correct answer when nobody is playing."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* World, FOutputDevice& Ar)
			{
				UAFLQueueDirectorySubsystem* Directory = UAFLQueueDirectorySubsystem::Get(World);
				if (!Directory)
				{
					Ar.Log(TEXT("afl.Lobby.Presence -- no game instance."));
					return;
				}
				TWeakObjectPtr<UAFLQueueDirectorySubsystem> Weak(Directory);
				TSharedRef<FDelegateHandle> Handle = MakeShared<FDelegateHandle>();
				*Handle = Directory->OnPresenceUpdated.AddLambda([Weak, Handle](int32 Online)
				{
					if (UAFLQueueDirectorySubsystem* Self = Weak.Get())
					{
						Self->OnPresenceUpdated.Remove(*Handle);
					}
					UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PRESENCE: online=%d (measured heartbeats, 90s window)"), Online);
				});
				Directory->RefreshPresence();
				Ar.Log(TEXT("afl.Lobby.Presence -- asked; the count prints to the log."));
			}));
}

UAFLQueueDirectorySubsystem* UAFLQueueDirectorySubsystem::Get(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UAFLQueueDirectorySubsystem>();
		}
	}
	return nullptr;
}

void UAFLQueueDirectorySubsystem::Deinitialize()
{
	// The HTTP callbacks capture a weak `this`, so an in-flight request cannot resurrect a dead subsystem --
	// but the pending buffers can be large, and holding a whole ladder past teardown is pointless.
	PendingLadder.Empty();
	PendingPopulationBody.Empty();
	Super::Deinitialize();
}

FString UAFLQueueDirectorySubsystem::ResolveBaseUrl() const
{
	// One source, and it is the client-safe one. `PlayerApiBaseUrl` reads DefaultGame.ini with the env var
	// winning when present -- a shipping client is launched by the player and has no environment, so a
	// second env-only resolution here would work in the editor and silently have no endpoint in a build.
	if (const UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
	{
		const FString Resolved = Online->PlayerApiBaseUrl();

		// Log WHAT it resolved to, not just whether it was empty. A base URL that is present but wrong
		// produces `Could not resolve host` three layers away in libcurl, and the one fact that identifies
		// it -- the string the client actually built the request from -- is otherwise never written down.
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_QUEUES: base URL resolved to '%s' (len %d)."),
			*Resolved, Resolved.Len());
		return Resolved;
	}
	return FString();
}

void UAFLQueueDirectorySubsystem::Refresh()
{
	if (bQueuesPending || bPopulationPending)
	{
		// Not restarted. Both reads are cheap and seconds away, and a second refresh landing mid-join would
		// publish a ladder from one response joined against the other's population.
		return;
	}

	const FString BaseUrl = ResolveBaseUrl();
	if (BaseUrl.IsEmpty())
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_QUEUES: no player API base URL configured -- the lobby has no ladder to draw."));
		State = EAFLQueueDirectoryState::Failed;
		OnUpdated.Broadcast(State);
		return;
	}

	State = EAFLQueueDirectoryState::Fetching;
	bQueuesPending = true;
	bPopulationPending = true;
	bLadderOk = false;
	bPopulationOk = false;
	PendingLadder.Reset();
	PendingPopulationBody.Reset();

	FHttpModule& Http = FHttpModule::Get();
	TWeakObjectPtr<UAFLQueueDirectorySubsystem> WeakThis(this);

	// ⚠ includeUnpublished=true. Without it an unopened bracket is simply absent from the response, and
	// league §3.2 rules that it must still be DRAWN -- disabled, reading `Not open yet`. The whole designed
	// ladder is information; hiding the unshipped half would be honest but mute.
	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();
		Request->SetURL(BaseUrl + TEXT("/queues?includeUnpublished=true"));
		Request->SetVerb(TEXT("GET"));
		// PUBLIC and READ-ONLY, like /catalog. No session ticket: which queues exist is not player-scoped,
		// and sending credentials to an endpoint that does not need them widens their exposure for nothing.
		Request->OnProcessRequestComplete().BindLambda(
			[WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
			{
				if (UAFLQueueDirectorySubsystem* Self = WeakThis.Get())
				{
					Self->HandleQueuesResponse(Req, Res, bOk);
				}
			});
		Request->ProcessRequest();
	}

	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();
		Request->SetURL(BaseUrl + TEXT("/population"));
		Request->SetVerb(TEXT("GET"));
		Request->OnProcessRequestComplete().BindLambda(
			[WeakThis](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
			{
				if (UAFLQueueDirectorySubsystem* Self = WeakThis.Get())
				{
					Self->HandlePopulationResponse(Req, Res, bOk);
				}
			});
		Request->ProcessRequest();
	}
}

void UAFLQueueDirectorySubsystem::ResolveBand(EAFLPlayTier Tier, int32 Amount, FAFLOnBandResolved OnResolved)
{
	const FString BaseUrl = ResolveBaseUrl();
	if (BaseUrl.IsEmpty() || Amount <= 0)
	{
		OnResolved.ExecuteIfBound(false, FText::GetEmpty(), false);
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FString::Printf(TEXT("%s/band?tier=%s&amount=%d"),
		*BaseUrl, FAFLLobbyQueueId::ToWire(Tier), Amount));
	Request->SetVerb(TEXT("GET"));

	Request->OnProcessRequestComplete().BindLambda(
		[OnResolved](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
		{
			const int32 Code = Res.IsValid() ? Res->GetResponseCode() : 0;

			// A 400 is a REAL ANSWER, not a transport failure: the amount matched no band, which the door
			// must render as `outside all bands` with the CTA disabled. Collapsing it into "the request
			// failed" would leave the player staring at a stale band while the button refused them.
			if (bOk && Res.IsValid() && Code == 400)
			{
				OnResolved.ExecuteIfBound(true, FText::GetEmpty(), /*bInSomeBand=*/false);
				return;
			}
			if (!bOk || !Res.IsValid() || Code < 200 || Code >= 300)
			{
				UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_BAND: GET /band failed (code %d)."), Code);
				OnResolved.ExecuteIfBound(false, FText::GetEmpty(), false);
				return;
			}

			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				OnResolved.ExecuteIfBound(false, FText::GetEmpty(), false);
				return;
			}

			const TSharedPtr<FJsonObject>* Band = nullptr;
			if (!Root->TryGetObjectField(TEXT("band"), Band) || !Band || !Band->IsValid())
			{
				// `band: null` on a LEAGUE tier -- a true answer meaning there is no buy-in here. Reported
				// as OK with no label, so the door renders nothing rather than an error.
				OnResolved.ExecuteIfBound(true, FText::GetEmpty(), true);
				return;
			}

			int32 Min = 0, Max = 0;
			(*Band)->TryGetNumberField(TEXT("min"), Min);
			(*Band)->TryGetNumberField(TEXT("max"), Max);

			// The page's phrasing, verbatim: "matching 400-500 V". Stated in the same place as the value,
			// never a tooltip -- a qualification the player has to go looking for was not made.
			const FText Label = FText::Format(
				NSLOCTEXT("AFLLobby", "BandMatching", "matching {0}-{1}"),
				FText::AsNumber(Min), FText::AsNumber(Max));
			OnResolved.ExecuteIfBound(true, Label, true);
		});
	Request->ProcessRequest();
}

void UAFLQueueDirectorySubsystem::RefreshPresence()
{
	const FString BaseUrl = ResolveBaseUrl();
	if (BaseUrl.IsEmpty())
	{
		return;
	}

	TWeakObjectPtr<UAFLQueueDirectorySubsystem> WeakThis(this);
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl + TEXT("/presence"));
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
		{
			UAFLQueueDirectorySubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			const int32 Code = Res.IsValid() ? Res->GetResponseCode() : 0;
			if (!bOk || !Res.IsValid() || Code < 200 || Code >= 300)
			{
				// ⚠ THE LAST GOOD VALUE SURVIVES A FAILED READ. Zeroing here would turn a network blip into
				// "the game is empty", which is a claim, not an absence.
				UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PRESENCE: GET /presence failed (code %d)."), Code);
				return;
			}

			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				return;
			}
			int32 Online = INDEX_NONE;
			if (Root->TryGetNumberField(TEXT("online"), Online))
			{
				Self->OnlineCount = Online;
				Self->OnPresenceUpdated.Broadcast(Online);
			}
		});
	Request->ProcessRequest();
}

void UAFLQueueDirectorySubsystem::HandleQueuesResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	bQueuesPending = false;

	const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
	if (!bConnectedSuccessfully || !Response.IsValid() || Code < 200 || Code >= 300)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_QUEUES: GET /queues failed (code %d)."), Code);
		SettleIfComplete();
		return;
	}

	if (!ParseQueuesResponse(Response->GetContentAsString(), PendingLadder))
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_QUEUES: GET /queues returned a body we could not parse."));
		SettleIfComplete();
		return;
	}

	bLadderOk = true;
	SettleIfComplete();
}

void UAFLQueueDirectorySubsystem::HandlePopulationResponse(FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	bPopulationPending = false;

	const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
	if (!bConnectedSuccessfully || !Response.IsValid() || Code < 200 || Code >= 300)
	{
		// NOT fatal. The ladder still draws, every cell reading Unknown -> "Count unavailable", which is
		// league §7's required behaviour rather than a fallback: never fabricate a number here.
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_QUEUES: GET /population failed (code %d) -- the ladder will read `Count unavailable`."), Code);
		SettleIfComplete();
		return;
	}

	bPopulationOk = true;
	PendingPopulationBody = Response->GetContentAsString();
	SettleIfComplete();
}

void UAFLQueueDirectorySubsystem::SettleIfComplete()
{
	if (bQueuesPending || bPopulationPending)
	{
		return;   // the other leg is still out; joining now would publish a half-answered set
	}

	if (!bLadderOk)
	{
		// No ladder. Deliberately NOT clearing a previously good set: a failed refresh is not evidence that
		// the queues stopped existing, and blanking the lobby on one bad response would be a worse claim
		// than keeping the last one we actually read.
		State = EAFLQueueDirectoryState::Failed;
		OnUpdated.Broadcast(State);
		return;
	}

	int32 Matched = 0;
	if (bPopulationOk)
	{
		Matched = ApplyPopulationResponse(PendingPopulationBody, PendingLadder);
	}

	Queues = MoveTemp(PendingLadder);
	PendingLadder.Reset();
	PendingPopulationBody.Reset();

	bPopulationKnown = bPopulationOk;
	State = EAFLQueueDirectoryState::Ready;

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFL_QUEUES: directory ready -- %d cells, %d published, population %s (%d matched)."),
		Queues.Num(),
		Algo::CountIf(Queues, [](const FAFLLobbyQueue& Q) { return Q.bPublished; }),
		bPopulationOk ? TEXT("read") : TEXT("UNAVAILABLE"),
		Matched);

	OnUpdated.Broadcast(State);
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  PARSING -- static so a captured response can be held by a test with no network
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

bool UAFLQueueDirectorySubsystem::ParseQueuesResponse(const FString& Json, TArray<FAFLLobbyQueue>& OutQueues)
{
	OutQueues.Reset();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
	if (!Root->TryGetArrayField(TEXT("queues"), Cells) || !Cells)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Cells)
	{
		const TSharedPtr<FJsonObject>* Cell = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Cell) || !Cell)
		{
			continue;
		}

		FAFLLobbyQueue Queue;
		if (!(*Cell)->TryGetStringField(TEXT("queueId"), Queue.QueueId) || Queue.QueueId.IsEmpty())
		{
			continue;
		}

		// The dimensions are read as their own fields rather than parsed back out of the id. The id is a
		// TRANSPORT identifier -- it happens to be readable, and leaning on that would make a future
		// renaming of any tier a silent client-side mis-scope. Parse is for the reverse direction.
		FString Wire;
		if ((*Cell)->TryGetStringField(TEXT("tier"), Wire))    { FAFLLobbyQueueId::FromWire(Wire, Queue.Tier); }
		if ((*Cell)->TryGetStringField(TEXT("league"), Wire))  { FAFLLobbyQueueId::FromWire(Wire, Queue.League); }
		if ((*Cell)->TryGetStringField(TEXT("ruleset"), Wire)) { FAFLLobbyQueueId::FromWire(Wire, Queue.Ruleset); }
		if ((*Cell)->TryGetStringField(TEXT("venue"), Wire))   { FAFLLobbyQueueId::FromWire(Wire, Queue.Venue); }
		(*Cell)->TryGetStringField(TEXT("bracket"), Queue.Bracket);

		int32 Number = 0;
		if ((*Cell)->TryGetNumberField(TEXT("positions"), Number)) { Queue.Positions = Number; }
		if ((*Cell)->TryGetNumberField(TEXT("slots"), Number))     { Queue.Slots = Number; }
		(*Cell)->TryGetBoolField(TEXT("published"), Queue.bPublished);

		const TArray<TSharedPtr<FJsonValue>>* Rungs = nullptr;
		if ((*Cell)->TryGetArrayField(TEXT("stakeRungs"), Rungs) && Rungs)
		{
			for (const TSharedPtr<FJsonValue>& Rung : *Rungs)
			{
				if (Rung.IsValid())
				{
					Queue.StakeRungs.Add(FMath::RoundToInt(Rung->AsNumber()));
				}
			}
		}

		// ⚠ `mapPool` IS PRESENT IN THE RESPONSE AND IS NOT READ. R18: the venue is a server outcome. There
		// is no field on FAFLLobbyQueue to hold it, so a row cannot name a map even by accident -- which is
		// the difference between a rule and a note asking people to follow one.

		// The starting reading, before any population overlay. An unpublished cell is NotOpen -- a fact
		// about the CONTENT (R63) -- and a published one is Unknown until /population says otherwise. Those
		// are different claims and this is the line where they must not be collapsed.
		Queue.State = Queue.bPublished ? EAFLPopulationState::Unknown : EAFLPopulationState::NotOpen;

		OutQueues.Add(MoveTemp(Queue));
	}

	return true;
}

int32 UAFLQueueDirectorySubsystem::ApplyPopulationResponse(const FString& Json, TArray<FAFLLobbyQueue>& InOutQueues)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return 0;
	}

	const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
	if (!Root->TryGetArrayField(TEXT("cells"), Cells) || !Cells)
	{
		return 0;
	}

	// Index by queueId rather than nesting the loops: the ladder is 72 cells and the reading covers the
	// published subset, so a linear scan per cell is a few thousand string compares for no reason.
	TMap<FString, int32> IndexById;
	IndexById.Reserve(InOutQueues.Num());
	for (int32 Index = 0; Index < InOutQueues.Num(); ++Index)
	{
		IndexById.Add(InOutQueues[Index].QueueId, Index);
	}

	int32 Matched = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Cells)
	{
		const TSharedPtr<FJsonObject>* Cell = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Cell) || !Cell)
		{
			continue;
		}

		FString QueueId;
		if (!(*Cell)->TryGetStringField(TEXT("queueId"), QueueId))
		{
			continue;
		}
		const int32* Found = IndexById.Find(QueueId);
		if (!Found)
		{
			// A reading for a cell the ladder does not contain. Not a crash and not silent: it means the two
			// endpoints disagree about what exists, which is worth knowing about.
			UE_LOG(LogAFLGameCore, Warning,
				TEXT("AFL_QUEUES: /population reported '%s', which /queues did not list."), *QueueId);
			continue;
		}

		FAFLLobbyQueue& Queue = InOutQueues[*Found];

		FString StateWire;
		(*Cell)->TryGetStringField(TEXT("state"), StateWire);
		Queue.State = FAFLLobbyQueueId::PopulationStateFromWire(StateWire);
		Queue.PlayersMatching = ReadNullableInt(*Cell, TEXT("playersMatching"));
		Queue.EstimatedWaitSeconds = ReadNullableInt(*Cell, TEXT("estimatedWaitSeconds"));

		Queue.Bands.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Bands = nullptr;
		if ((*Cell)->TryGetArrayField(TEXT("bands"), Bands) && Bands)
		{
			for (const TSharedPtr<FJsonValue>& BandValue : *Bands)
			{
				const TSharedPtr<FJsonObject>* BandObj = nullptr;
				if (!BandValue.IsValid() || !BandValue->TryGetObject(BandObj) || !BandObj)
				{
					continue;
				}
				FAFLLobbyBand Band;
				Band.Centre = ReadNullableInt(*BandObj, TEXT("centre"));
				Band.Waiting = ReadNullableInt(*BandObj, TEXT("waiting"));
				Band.EstimatedWaitSeconds = ReadNullableInt(*BandObj, TEXT("estimatedWaitSeconds"));

				FString BandStateWire;
				(*BandObj)->TryGetStringField(TEXT("state"), BandStateWire);
				Band.State = FAFLLobbyQueueId::PopulationStateFromWire(BandStateWire);

				Queue.Bands.Add(MoveTemp(Band));
			}
		}

		// ⚠ AN EMPTY `bands` ARRAY IS LEFT EMPTY, NOT FILLED WITH ZEROES. `stakeRungs('LeaguePlay')` returns
		// [], so a league cell legitimately carries no bands -- and staked §5.1 is explicit that "bands that
		// do not exist" and "bands that are empty" are not the same and must not render alike.

		// A published cell that reports NOTHING keeps Unknown rather than inheriting a stale reading, and a
		// cell the registry says is unpublished stays NotOpen no matter what a reading claims -- the content
		// fact outranks the population fact, because a queue with no map cannot be entered at any count.
		if (!Queue.bPublished)
		{
			Queue.State = EAFLPopulationState::NotOpen;
		}

		++Matched;
	}

	return Matched;
}
