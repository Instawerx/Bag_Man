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
#include "HAL/PlatformMisc.h"                    // FPlatformMisc::GetEnvironmentVariable (server-only earn key/URL)
#include "Misc/CoreMisc.h"                       // IsRunningDedicatedServer()
#include "Misc/DateTime.h"                       // FDateTime::UtcNow (canary ts)
#include "Misc/Guid.h"                           // FGuid (canary matchId/nonce)
#include "GameFramework/CheatManagerDefines.h"   // UE_WITH_CHEAT_MANAGER (canary guard)

// A1.3b earn request signer: OpenSSL HMAC-SHA256. AFLONLINE_USE_OPENSSL + the OpenSSL ThirdParty dep are declared
// in AFLOnline.Build.cs (mirroring PlatformCryptoContext); the module-private macro gates the include so a platform
// without OpenSSL still compiles (the signer then returns empty and PostServerEarn refuses to sign).
#if AFLONLINE_USE_OPENSSL
// OpenSSL's <openssl/ossl_typ.h> does `typedef struct ui_st UI;`, which collides with UE's `namespace UI`
// (ObjectMacros.h, force-included into this UObject module via the shared PCH) -> C2365 'UI' redefinition.
// HMAC/EVP never use OpenSSL's UI type, so rename it away for the span of these includes only; UE's namespace UI
// -- already parsed from the PCH before this point -- is untouched, and #undef restores UI for the rest of the file.
#define UI OPENSSL_UI_UNUSED
THIRD_PARTY_INCLUDES_START
#include <openssl/evp.h>
#include <openssl/hmac.h>
THIRD_PARTY_INCLUDES_END
#undef UI
#endif

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

	// A1.3b earn signer config. Resolve the HMAC key + full /earn URL from the environment ONCE, only on a
	// DEDICATED SERVER (production) or in the EDITOR (dev canary). Both are absent from any COOKED CLIENT
	// (GIsEditor=false AND not a dedicated server there) and from a cooked listen-host, so the key never exists
	// in a shipped client process. Env is fixed at launch -> read once. The key is NEVER logged (held/MISSING only).
	if (IsRunningDedicatedServer() || GIsEditor)
	{
		EarnHmacKey = FPlatformMisc::GetEnvironmentVariable(TEXT("AFL_EARN_HMAC_KEY"));
		EarnUrl     = FPlatformMisc::GetEnvironmentVariable(TEXT("AFL_EARN_URL"));
		UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] Earn signer (%s): key=%s url=%s"),
			IsRunningDedicatedServer() ? TEXT("dedicated server") : TEXT("editor"),
			EarnHmacKey.IsEmpty() ? TEXT("MISSING") : TEXT("held"),
			EarnUrl.IsEmpty() ? TEXT("MISSING") : *EarnUrl);
	}
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

FString UAFLOnlineSubsystem::SignHmacSha256Hex(const FString& Body, const FString& Key)
{
#if AFLONLINE_USE_OPENSSL
	// Sign the EXACT UTF-8 bytes of Body with the UTF-8 bytes of Key. The backend (Node createHmac('sha256'))
	// digests the raw request bytes and compares LOWERCASE hex constant-time -> we must (a) feed UTF-8, not raw
	// TCHAR, and (b) lowercase. Use the length HMAC returns (32 for sha256), never a hardcoded 32.
	const FTCHARToUTF8 KeyUtf8(*Key);
	const FTCHARToUTF8 BodyUtf8(*Body);
	uint8 Digest[EVP_MAX_MD_SIZE];
	unsigned int DigestLen = 0;
	HMAC(EVP_sha256(),
		KeyUtf8.Get(), KeyUtf8.Length(),
		reinterpret_cast<const unsigned char*>(BodyUtf8.Get()), static_cast<size_t>(BodyUtf8.Length()),
		Digest, &DigestLen);
	return BytesToHex(Digest, static_cast<int32>(DigestLen)).ToLower();
#else
	// OpenSSL unavailable on this platform -> no signer. Callers (PostServerEarn) treat empty as "cannot sign".
	return FString();
#endif
}

void UAFLOnlineSubsystem::PostServerEarn(const FString& EarnJsonBody, TFunction<void(bool, const FString&)> OnComplete)
{
	// SERVER-ONLY: the key + URL are read only on a dedicated server (Initialize). Empty here => not a server (or
	// env unset) => refuse to sign. This is the belt to the env-var braces: no client process ever signs an earn.
	if (EarnHmacKey.IsEmpty() || EarnUrl.IsEmpty())
	{
		UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] PostServerEarn SKIP -- earn key/URL unavailable. Set AFL_EARN_HMAC_KEY + AFL_EARN_URL in the launching shell (server or editor). IsRunningDedicatedServer()=%d GIsEditor=%d."),
			IsRunningDedicatedServer() ? 1 : 0, GIsEditor ? 1 : 0);
		OnComplete(false, TEXT("skip: earn key/URL unavailable (server-only)"));
		return;
	}

	// Sign-what-you-send: sign the EXACT FString handed to SetContentAsString below -- no re-serialize between.
	const FString Signature = SignHmacSha256Hex(EarnJsonBody, EarnHmacKey);
	if (Signature.IsEmpty())
	{
		UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] PostServerEarn SKIP -- empty signature (OpenSSL unavailable?)."));
		OnComplete(false, TEXT("skip: empty signature"));
		return;
	}

	const FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(EarnUrl);                                       // the FULL /earn endpoint (NOT BaseUrl()/PlayFab)
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetHeader(TEXT("X-Signature"), Signature);            // HMAC-SHA256(body) hex, lowercase
	Req->SetContentAsString(EarnJsonBody);                     // the SAME FString that was signed

	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedOk)
		{
			if (!bConnectedOk || !Response.IsValid())
			{
				UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] PostServerEarn HTTP failed (no response)."));
				OnComplete(false, TEXT("no response"));
				return;
			}
			// Plain HTTP (NOT the PlayFab envelope): success == 200; surface the raw body either way so the canary
			// can print the 200 {success,newBalance,nonce} OR the 401/400/502 error body for diagnosis.
			const int32 Http = Response->GetResponseCode();
			const FString RespBody = Response->GetContentAsString();
			const bool bOk = (Http == 200);
			UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] PostServerEarn -> http=%d ok=%d"), Http, bOk ? 1 : 0);
			OnComplete(bOk, RespBody);
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

#if UE_WITH_CHEAT_MANAGER
// ─── A1.3b LIVE interop canary: afl.Online.EarnCanary [playFabId] ──────────────────────────────────────────
// Proves the UE OpenSSL HMAC-SHA256 signer verifies + grants against the DEPLOYED /earn Lambda BEFORE any wallet
// wiring. Server/editor-only (PostServerEarn self-gates on the env key/URL). Builds a contract-valid body
// (docs/earn-endpoint-contract.md) as EXACT bytes -- integer amount + ts, fresh server-issued nonce -- signs and
// sends that same string, then logs AFL_A13B_CANARY ok=<0|1> resp=<body>. A green line (ok=1 + newBalance) proves
// lowercase-hex + exact-UTF8-bytes are correct end-to-end. Run on a dedicated server OR in editor PIE, with
// AFL_EARN_HMAC_KEY + AFL_EARN_URL set in the shell that LAUNCHED the process (read once at subsystem init).
static void HandleAFLOnlineEarnCanary(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(World);
	if (!Online)
	{
		Ar.Log(TEXT("afl.Online.EarnCanary - no AFLOnline subsystem (need a game world)."));
		return;
	}

	const FString PlayFabId = (Args.Num() > 0 && !Args[0].IsEmpty()) ? Args[0] : TEXT("3EA058EAF36E6CA6");
	const FString MatchId   = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	const FString Nonce     = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	const int64   Ts        = FDateTime::UtcNow().ToUnixTimestamp();

	// The EXACT bytes we sign+send: compact JSON with INTEGER amount + ts (the contract wants integers; a JSON
	// writer can emit 5.0 -- Printf pins the form). None of the interpolated fields contain quotes/backslashes.
	const FString Body = FString::Printf(
		TEXT("{\"playFabId\":\"%s\",\"currencyCode\":\"WA\",\"amount\":5,\"reason\":\"canary\",\"matchId\":\"%s\",\"nonce\":\"%s\",\"ts\":%lld}"),
		*PlayFabId, *MatchId, *Nonce, static_cast<long long>(Ts));

	Ar.Logf(TEXT("afl.Online.EarnCanary -> POST /earn (playFabId=%s amount=5 WA nonce=%s)"), *PlayFabId, *Nonce);
	Online->PostServerEarn(Body, [](bool bOk, const FString& Resp)
	{
		UE_LOG(LogAFLOnline, Log, TEXT("AFL_A13B_CANARY ok=%d resp=%s"), bOk ? 1 : 0, *Resp);
	});
}

FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLOnlineEarnCanaryCmd(TEXT("afl.Online.EarnCanary"),
	TEXT("A1.3b LIVE interop canary (dedicated server OR editor PIE): sign a contract-valid earn with the OpenSSL HMAC-SHA256 signer and POST to the /earn Lambda. Proves the UE signature verifies+grants (200 + newBalance) before wallet wiring. Arg 0 = playFabId (default the smoke test id). Needs AFL_EARN_HMAC_KEY + AFL_EARN_URL set in the shell that launched the editor/server. Logs AFL_A13B_CANARY ok=<0|1> resp=<body>."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleAFLOnlineEarnCanary));
#endif // UE_WITH_CHEAT_MANAGER
