// Copyright C12 AI Gaming. All Rights Reserved.

#include "Social/AFLSocialSubsystem.h"

#include "AFLOnlineSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "IWebSocket.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"
#include "WebSocketsModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSocialSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogAFLSocial, Log, All);

namespace
{
	// Parse one DM JSON object into FAFLDirectMessage (defensive -- missing fields keep their defaults).
	FAFLDirectMessage ParseDM(const TSharedPtr<FJsonObject>& O)
	{
		FAFLDirectMessage M;
		if (!O.IsValid()) { return M; }
		O->TryGetStringField(TEXT("conversationId"), M.ConversationId);
		O->TryGetStringField(TEXT("msgId"), M.MsgId);
		O->TryGetStringField(TEXT("senderId"), M.SenderId);
		O->TryGetStringField(TEXT("recipientId"), M.RecipientId);
		O->TryGetStringField(TEXT("body"), M.Body);
		double Ts = 0.0;
		if (O->TryGetNumberField(TEXT("serverTs"), Ts)) { M.ServerEpochMs = static_cast<int64>(Ts); }
		const TSharedPtr<FJsonValue> ReadAt = O->TryGetField(TEXT("readAt"));
		M.bRead = ReadAt.IsValid() && !ReadAt->IsNull();
		return M;
	}
}

UAFLSocialSubsystem* UAFLSocialSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UAFLSocialSubsystem>() : nullptr;
}

void UAFLSocialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Client-only: a dedicated server carries no player DM session.
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
	{
		LoggedInHandle = Online->OnLoggedIn.AddUObject(this, &UAFLSocialSubsystem::ConnectWebSocket);
		if (Online->IsLoggedIn())
		{
			ConnectWebSocket();
		}
	}
}

void UAFLSocialSubsystem::Deinitialize()
{
	bWantConnected = false;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().ClearTimer(ReconnectTimer);
		}
	}
	if (LoggedInHandle.IsValid())
	{
		if (UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
		{
			Online->OnLoggedIn.Remove(LoggedInHandle);
		}
		LoggedInHandle.Reset();
	}
	CloseWebSocket();
	Super::Deinitialize();
}

FString UAFLSocialSubsystem::DmWebSocketUrl() const
{
	// Env wins for server-side tooling; a shipping client reads config (it has no environment).
	const FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("AFL_DM_WS_URL"));
	if (!Env.IsEmpty())
	{
		return Env;
	}
	FString Url;
	GConfig->GetString(TEXT("AFL.Online"), TEXT("DirectMessageWebSocketUrl"), Url, GGameIni);
	Url.TrimStartAndEndInline();
	return Url;
}

void UAFLSocialSubsystem::ConnectWebSocket()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online || !Online->IsLoggedIn())
	{
		UE_LOG(LogAFLSocial, Log, TEXT("AFL_DM: connect deferred -- not logged in."));
		return;
	}
	if (Socket.IsValid() && Socket->IsConnected())
	{
		return; // already up -- don't stack sockets
	}

	const FString Base = DmWebSocketUrl();
	const FString Ticket = Online->GetSessionTicket();
	if (Base.IsEmpty() || Ticket.IsEmpty())
	{
		UE_LOG(LogAFLSocial, Warning, TEXT("AFL_DM: no WS url or session ticket -- DM offline."));
		return;
	}

	bWantConnected = true;
	// Session ticket rides the query (a UE WebSocket cannot reliably set custom headers at connect); the
	// backend ws-connect accepts ?ticket= exactly for this.
	const FString Url = FString::Printf(TEXT("%s?ticket=%s"), *Base, *FGenericPlatformHttp::UrlEncode(Ticket));

	FWebSocketsModule& Mod = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
	Socket = Mod.CreateWebSocket(Url, FString());
	Socket->OnConnected().AddUObject(this, &UAFLSocialSubsystem::HandleWsConnected);
	Socket->OnConnectionError().AddUObject(this, &UAFLSocialSubsystem::HandleWsConnectionError);
	Socket->OnClosed().AddUObject(this, &UAFLSocialSubsystem::HandleWsClosed);
	Socket->OnMessage().AddUObject(this, &UAFLSocialSubsystem::HandleWsMessage);
	Socket->Connect();
	UE_LOG(LogAFLSocial, Log, TEXT("AFL_DM: connecting DM socket."));
}

void UAFLSocialSubsystem::CloseWebSocket()
{
	if (Socket.IsValid())
	{
		Socket->OnConnected().RemoveAll(this);
		Socket->OnConnectionError().RemoveAll(this);
		Socket->OnClosed().RemoveAll(this);
		Socket->OnMessage().RemoveAll(this);
		if (Socket->IsConnected())
		{
			Socket->Close();
		}
		Socket.Reset();
	}
	bConnected = false;
}

void UAFLSocialSubsystem::HandleWsConnected()
{
	bConnected = true;
	ReconnectAttempts = 0;
	UE_LOG(LogAFLSocial, Log, TEXT("AFL_DM: socket connected."));
	OnConnectionChanged.Broadcast(true);
}

void UAFLSocialSubsystem::HandleWsConnectionError(const FString& Error)
{
	bConnected = false;
	UE_LOG(LogAFLSocial, Warning, TEXT("AFL_DM: connection error: %s"), *Error);
	OnConnectionChanged.Broadcast(false);
	ScheduleReconnect();
}

void UAFLSocialSubsystem::HandleWsClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	bConnected = false;
	UE_LOG(LogAFLSocial, Log, TEXT("AFL_DM: socket closed code=%d clean=%d %s"), StatusCode, bWasClean ? 1 : 0, *Reason);
	OnConnectionChanged.Broadcast(false);
	if (bWantConnected && !bWasClean)
	{
		ScheduleReconnect();
	}
}

void UAFLSocialSubsystem::ScheduleReconnect()
{
	if (!bWantConnected || ReconnectAttempts >= MaxReconnectAttempts)
	{
		return;
	}
	const UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	++ReconnectAttempts;
	const float Delay = FMath::Min(30.f, 2.f * static_cast<float>(ReconnectAttempts)); // simple backoff
	World->GetTimerManager().SetTimer(ReconnectTimer, this, &UAFLSocialSubsystem::ConnectWebSocket, Delay, /*loop*/ false);
	UE_LOG(LogAFLSocial, Log, TEXT("AFL_DM: reconnect #%d in %.0fs"), ReconnectAttempts, Delay);
}

void UAFLSocialSubsystem::HandleWsMessage(const FString& MessageJson)
{
	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageJson);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		UE_LOG(LogAFLSocial, Verbose, TEXT("AFL_DM: unparseable WS frame."));
		return;
	}
	// A DM frame carries a body + a senderId; anything else (acks/control) is ignored.
	if (!Obj->HasField(TEXT("body")) || !Obj->HasField(TEXT("senderId")))
	{
		return;
	}
	const FAFLDirectMessage Msg = ParseDM(Obj);
	OnDirectMessage.Broadcast(Msg);
}

void UAFLSocialSubsystem::SendDirectMessage(const FString& RecipientPlayFabId, const FString& Body)
{
	if (!Socket.IsValid() || !Socket->IsConnected())
	{
		UE_LOG(LogAFLSocial, Warning, TEXT("AFL_DM: send while disconnected -- dropped."));
		return;
	}
	if (RecipientPlayFabId.IsEmpty() || Body.IsEmpty())
	{
		return;
	}
	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("action"), TEXT("sendMessage"));
	Obj->SetStringField(TEXT("to"), RecipientPlayFabId);
	Obj->SetStringField(TEXT("body"), Body);
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj, Writer);
	Socket->Send(Out);
}

void UAFLSocialSubsystem::FetchInbox(int64 SinceEpochMs, TFunction<void(const TArray<FAFLDirectMessage>&, int64)> OnComplete)
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online || !Online->IsLoggedIn())
	{
		if (OnComplete) { OnComplete(TArray<FAFLDirectMessage>(), SinceEpochMs); }
		return;
	}
	const FString Base = Online->PlayerApiBaseUrl();
	const FString Ticket = Online->GetSessionTicket();
	if (Base.IsEmpty() || Ticket.IsEmpty())
	{
		if (OnComplete) { OnComplete(TArray<FAFLDirectMessage>(), SinceEpochMs); }
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/messages/inbox?since=%lld"), *Base, static_cast<long long>(SinceEpochMs));
	const FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("GET"));
	Req->SetHeader(TEXT("X-PlayFab-SessionTicket"), Ticket);
	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			TArray<FAFLDirectMessage> Out;
			int64 Cursor = 0;
			if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200)
			{
				TSharedPtr<FJsonObject> Root;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
				if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
					if (Root->TryGetArrayField(TEXT("messages"), Arr) && Arr)
					{
						for (const TSharedPtr<FJsonValue>& V : *Arr)
						{
							if (V.IsValid() && V->Type == EJson::Object)
							{
								Out.Add(ParseDM(V->AsObject()));
							}
						}
					}
					double C = 0.0;
					if (Root->TryGetNumberField(TEXT("cursor"), C)) { Cursor = static_cast<int64>(C); }
				}
			}
			if (OnComplete) { OnComplete(Out, Cursor); }
		});
	Req->ProcessRequest();
}

void UAFLSocialSubsystem::MarkConversationRead(const FString& ConversationId, TFunction<void(bool)> OnComplete)
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online || !Online->IsLoggedIn() || ConversationId.IsEmpty())
	{
		if (OnComplete) { OnComplete(false); }
		return;
	}
	const FString Base = Online->PlayerApiBaseUrl();
	const FString Ticket = Online->GetSessionTicket();
	if (Base.IsEmpty() || Ticket.IsEmpty())
	{
		if (OnComplete) { OnComplete(false); }
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/messages/%s/read"), *Base, *FGenericPlatformHttp::UrlEncode(ConversationId));
	const FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("X-PlayFab-SessionTicket"), Ticket);
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(TEXT("{}"));
	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			const bool bSuccess = bOk && Resp.IsValid() && Resp->GetResponseCode() == 200;
			if (OnComplete) { OnComplete(bSuccess); }
		});
	Req->ProcessRequest();
}
