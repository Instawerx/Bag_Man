// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLOnlineSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAFLOnline, Log, All);

namespace
{
	static const TCHAR* const GDefaultTitleId   = TEXT("1A2077");        // IRONICS (non-secret)
	static const TCHAR* const GDefaultDevCustom  = TEXT("AFL_DEV_TEST_01");
}

// PlayFab Title ID -- non-secret. Overridable for a test title.
static TAutoConsoleVariable<FString> CVarOnlineTitleId(
	TEXT("afl.Online.TitleId"),
	GDefaultTitleId,
	TEXT("PlayFab Title ID (non-secret). Default 1A2077 (IRONICS)."),
	ECVF_Default);

// DEV-ONLY LoginWithCustomID id. Point this at the seeded test account. Editor/non-shipping only (the login
// path itself is compiled out of shipping -- see EnsureLogin).
static TAutoConsoleVariable<FString> CVarOnlineDevCustomId(
	TEXT("afl.Online.DevCustomId"),
	GDefaultDevCustom,
	TEXT("DEV-ONLY PlayFab LoginWithCustomID id (non-shipping). Point at the seeded test account (e.g. AFL_DEV_TEST_01)."),
	ECVF_Default);

UAFLOnlineSubsystem* UAFLOnlineSubsystem::Get(const UObject* WorldContext)
{
	if (WorldContext)
	{
		if (const UWorld* World = WorldContext->GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				return GI->GetSubsystem<UAFLOnlineSubsystem>();
			}
		}
	}
	return nullptr;
}

FString UAFLOnlineSubsystem::GetTitleId() const { return CVarOnlineTitleId.GetValueOnGameThread(); }
FString UAFLOnlineSubsystem::BaseUrl() const { return FString::Printf(TEXT("https://%s.playfabapi.com"), *GetTitleId()); }
FString UAFLOnlineSubsystem::ResolveDevCustomId() const { return CVarOnlineDevCustomId.GetValueOnGameThread(); }

void UAFLOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] Subsystem online (title=%s). Kicking login."), *GetTitleId());
	EnsureLogin();
}

void UAFLOnlineSubsystem::Deinitialize()
{
	PendingLoginCallbacks.Empty();
	Super::Deinitialize();
}

void UAFLOnlineSubsystem::EnsureLogin()
{
	if (LoginState == EAFLLoginState::LoggedIn || LoginState == EAFLLoginState::InFlight)
	{
		return;
	}

#if UE_BUILD_SHIPPING
	// SHIPPING GATE (non-negotiable): CustomID is a DEV path only. A fixed CustomID in a shipped build is an
	// account-spoof hole (anyone could auth as that account). Shipping login is platform-only
	// (LoginWithSteam / LoginWithOpenIdConnect via CommonUser+OSS GetAuthToken), wired in the A1.1-shipped
	// follow-on. Until then a shipping build has NO PlayFab login -- deliberate; CustomID must NEVER ship.
	UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] Shipping build: platform login not yet wired; CustomID is dev-only and disabled."));
	ResolveLogin(false);
#else
	StartLoginWithCustomID();
#endif
}

void UAFLOnlineSubsystem::StartLoginWithCustomID()
{
#if UE_BUILD_SHIPPING
	// Never reachable in shipping (EnsureLogin gates it out); guard anyway so the dev path can't be linked in.
	ResolveLogin(false);
#else
	LoginState = EAFLLoginState::InFlight;

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("TitleId"), GetTitleId());
	Body->SetStringField(TEXT("CustomId"), ResolveDevCustomId());
	Body->SetBoolField(TEXT("CreateAccount"), true);

	FString BodyStr;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body, Writer);

	const FString Url = BaseUrl() / TEXT("Client/LoginWithCustomID");

	const FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(BodyStr);
	Req->OnProcessRequestComplete().BindUObject(this, &UAFLOnlineSubsystem::HandleLoginResponse);

	UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] LoginWithCustomID -> %s (customId=%s)"), *Url, *ResolveDevCustomId());
	Req->ProcessRequest();
#endif
}

void UAFLOnlineSubsystem::HandleLoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedOk)
{
	if (!bConnectedOk || !Response.IsValid())
	{
		UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] Login HTTP failed (no response)."));
		ResolveLogin(false);
		return;
	}

	const int32 Http = Response->GetResponseCode();
	TSharedPtr<FJsonObject> Data;
	int32 PfCode = 0;
	const bool bParsed = ParseEnvelope(Response->GetContentAsString(), Data, PfCode);

	if (Http != 200 || !bParsed || !Data.IsValid())
	{
		// Log PlayFab's response body (its errorMessage) so a title-config rejection is self-diagnosing
		// in the log -- e.g. PlayerCreationDisabled -> "enable client account creation on the title".
		UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] Login rejected (http=%d): %s"), Http, *Response->GetContentAsString().Left(500));
		ResolveLogin(false);
		return;
	}

	Data->TryGetStringField(TEXT("PlayFabId"), PlayFabId);
	Data->TryGetStringField(TEXT("SessionTicket"), SessionTicket);

	const TSharedPtr<FJsonObject>* EntityObj = nullptr;
	if (Data->TryGetObjectField(TEXT("EntityToken"), EntityObj) && EntityObj)
	{
		(*EntityObj)->TryGetStringField(TEXT("EntityToken"), EntityToken);
	}

	if (PlayFabId.IsEmpty() || SessionTicket.IsEmpty())
	{
		UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] Login response missing PlayFabId/SessionTicket."));
		ResolveLogin(false);
		return;
	}

	UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] LoginWithCustomID OK PlayFabId=%s (entityToken=%s)"),
		*PlayFabId, EntityToken.IsEmpty() ? TEXT("none") : TEXT("held"));
	ResolveLogin(true);
}

void UAFLOnlineSubsystem::ResolveLogin(bool bSuccess)
{
	LoginState = bSuccess ? EAFLLoginState::LoggedIn : EAFLLoginState::Failed;

	// Drain + fire the one-shot waiters (move first -- a callback may re-enter).
	TArray<TFunction<void(bool)>> Callbacks = MoveTemp(PendingLoginCallbacks);
	PendingLoginCallbacks.Reset();
	for (const TFunction<void(bool)>& Cb : Callbacks)
	{
		if (Cb) { Cb(bSuccess); }
	}

	if (bSuccess)
	{
		OnLoggedIn.Broadcast();
	}
}

void UAFLOnlineSubsystem::CallWhenLoggedIn(TFunction<void(bool)> Callback, float TimeoutSeconds)
{
	if (LoginState == EAFLLoginState::LoggedIn) { Callback(true);  return; }
	if (LoginState == EAFLLoginState::Failed)   { Callback(false); return; }

	// NotStarted / InFlight -> queue, ensure a login is running, arm a timeout so a hung login degrades to cache.
	PendingLoginCallbacks.Add(MoveTemp(Callback));
	EnsureLogin();

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (World)
	{
		TWeakObjectPtr<UAFLOnlineSubsystem> WeakThis(this);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, [WeakThis]()
		{
			UAFLOnlineSubsystem* Self = WeakThis.Get();
			if (Self && (Self->LoginState == EAFLLoginState::InFlight || Self->LoginState == EAFLLoginState::NotStarted))
			{
				UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] Login timeout -> pending readers fall back to cache."));
				TArray<TFunction<void(bool)>> Stragglers = MoveTemp(Self->PendingLoginCallbacks);
				Self->PendingLoginCallbacks.Reset();
				for (const TFunction<void(bool)>& Cb : Stragglers) { if (Cb) { Cb(false); } }
			}
		}, TimeoutSeconds, false);
	}
}

void UAFLOnlineSubsystem::PostClientApi(const FString& ApiName, const TSharedRef<FJsonObject>& Body,
	TFunction<void(bool, TSharedPtr<FJsonObject>)> OnComplete, bool bRequireAuth)
{
	if (bRequireAuth && SessionTicket.IsEmpty())
	{
		UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] PostClientApi(%s) needs auth but no SessionTicket."), *ApiName);
		OnComplete(false, nullptr);
		return;
	}

	FString BodyStr;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body, Writer);

	const FString Url = BaseUrl() / (FString(TEXT("Client/")) + ApiName);

	const FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (bRequireAuth)
	{
		Req->SetHeader(TEXT("X-Authorization"), SessionTicket);
	}
	Req->SetContentAsString(BodyStr);

	Req->OnProcessRequestComplete().BindLambda(
		[ApiName, OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedOk)
		{
			if (!bConnectedOk || !Response.IsValid())
			{
				UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] %s HTTP failed (no response)."), *ApiName);
				OnComplete(false, nullptr);
				return;
			}
			TSharedPtr<FJsonObject> Data;
			int32 PfCode = 0;
			const int32 Http = Response->GetResponseCode();
			const bool bOk = ParseEnvelope(Response->GetContentAsString(), Data, PfCode) && Http == 200 && Data.IsValid();
			if (bOk)
			{
				UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] %s -> http=%d ok=1"), *ApiName, Http);
			}
			else
			{
				// Surface PlayFab's errorMessage (e.g. a missing currency/catalog item) in the log.
				UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] %s -> http=%d ok=0: %s"), *ApiName, Http, *Response->GetContentAsString().Left(500));
			}
			OnComplete(bOk, Data);
		});
	Req->ProcessRequest();
}

bool UAFLOnlineSubsystem::ParseEnvelope(const FString& Body, TSharedPtr<FJsonObject>& OutData, int32& OutCode)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	Root->TryGetNumberField(TEXT("code"), OutCode);

	const TSharedPtr<FJsonObject>* DataObj = nullptr;
	if (Root->TryGetObjectField(TEXT("data"), DataObj) && DataObj)
	{
		OutData = *DataObj;
		return true;
	}
	return false;
}
