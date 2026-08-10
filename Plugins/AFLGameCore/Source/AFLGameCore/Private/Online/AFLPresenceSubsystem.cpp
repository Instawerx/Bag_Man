// Copyright C12 AI Gaming. All Rights Reserved.

#include "Online/AFLPresenceSubsystem.h"

#include "AFLGameCore.h"                 // LogAFLGameCore
#include "AFLOnlineSubsystem.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLPresenceSubsystem)

UAFLPresenceSubsystem* UAFLPresenceSubsystem::Get(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UAFLPresenceSubsystem>();
		}
	}
	return nullptr;
}

void UAFLPresenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// ⚠ NEVER ON A DEDICATED SERVER. It holds no player SessionTicket, so every beat would be a 401 on a
	// timer -- and conceptually a server is not a player. Each client counts ITSELF; a server counting on
	// behalf of its connected players would double-count everyone who is also running a client, and would
	// keep counting them for a window after the process died.
	if (IsRunningDedicatedServer())
	{
		return;
	}

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_PRESENCE: no online subsystem -- this client will not be counted."));
		return;
	}

	if (Online->IsLoggedIn())
	{
		StartBeating();
		return;
	}

	// A heartbeat before login has no ticket to send, so it would be a guaranteed 401. Wait for the login
	// that mints one rather than beating into a wall and retrying.
	LoginHandle = Online->OnLoggedIn.AddUObject(this, &UAFLPresenceSubsystem::HandleLoggedIn);
}

void UAFLPresenceSubsystem::HandleLoggedIn()
{
	StartBeating();
}

void UAFLPresenceSubsystem::StartBeating()
{
	if (BeatTimer.IsValid())
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	// Beat IMMEDIATELY, then on the interval. Waiting a full period first would leave a freshly-launched
	// client uncounted for 30s, which is most of a short visit to the front end.
	SendHeartbeat();
	GameInstance->GetTimerManager().SetTimer(BeatTimer, this, &UAFLPresenceSubsystem::SendHeartbeat,
		BeatIntervalSeconds, /*bLoop=*/true);

	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PRESENCE: heartbeat started (%.0fs interval)."), BeatIntervalSeconds);
}

void UAFLPresenceSubsystem::SendHeartbeat()
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online || !Online->IsLoggedIn())
	{
		return;
	}

	const FString BaseUrl = Online->PlayerApiBaseUrl();
	const FString SessionTicket = Online->GetSessionTicket();
	if (BaseUrl.IsEmpty() || SessionTicket.IsEmpty())
	{
		return;
	}

	TWeakObjectPtr<UAFLPresenceSubsystem> WeakThis(this);
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl + TEXT("/heartbeat"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// The ticket is the WHOLE payload's worth of identity: the server derives the PlayFabId from it and the
	// body names nobody. A heartbeat that carried an id would let one client check in as another.
	Request->SetHeader(TEXT("X-SessionTicket"), SessionTicket);
	Request->SetContentAsString(TEXT("{}"));

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
		{
			UAFLPresenceSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			const int32 Code = Res.IsValid() ? Res->GetResponseCode() : 0;
			if (bOk && Res.IsValid() && Code >= 200 && Code < 300)
			{
				++Self->AcceptedBeats;
				if (!Self->bLoggedFirstSuccess)
				{
					Self->bLoggedFirstSuccess = true;
					UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PRESENCE: first heartbeat accepted -- this client is counted."));
				}
				return;
			}

			++Self->FailedBeats;
			// ⚠ ONE LINE, NOT ONE PER BEAT. A failing heartbeat repeats every 30s for the life of the
			// process; logging each would bury a real problem under its own noise. A missed beat is also
			// not worth escalating -- the server's window tolerates two -- so this is Warning once and
			// then silence.
			if (!Self->bLoggedFirstFailure)
			{
				Self->bLoggedFirstFailure = true;
				UE_LOG(LogAFLGameCore, Warning,
					TEXT("AFL_PRESENCE: heartbeat failed (code %d) -- this client will not be counted while this persists. ")
					TEXT("Further failures are silent; check GetAcceptedBeats."), Code);
			}
		});
	Request->ProcessRequest();
}

void UAFLPresenceSubsystem::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(BeatTimer);
	}
	if (LoginHandle.IsValid())
	{
		if (UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
		{
			Online->OnLoggedIn.Remove(LoginHandle);
		}
		LoginHandle.Reset();
	}

	// NO "I AM LEAVING" CALL, DELIBERATELY. It would be the one message that cannot be relied on -- a crash
	// or a kill never sends it -- so building the count on its absence rather than its arrival is what makes
	// the number trustworthy. The row expires on its own within the window.
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_PRESENCE: stopped (%d accepted, %d failed)."), AcceptedBeats, FailedBeats);

	Super::Deinitialize();
}
