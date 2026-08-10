// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLOnlineSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"                       // FWorldContext (PIEInstance -- per-PIE-client dev identity)
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
#include "Misc/ConfigCacheIni.h"                 // GConfig -- the CLIENT's API base URL (no env on a player's box)
#include "Misc/DateTime.h"                       // FDateTime::UtcNow (canary ts)
#include "Misc/Guid.h"                           // FGuid (canary matchId/nonce)
#include "GameFramework/CheatManagerDefines.h"   // UE_WITH_CHEAT_MANAGER (canary guard)

// D17 shipping login (EOS Default). WITH_EOS_SDK is defined PUBLICLY by the EOSSDK module (declared in
// AFLOnline.Build.cs); on a platform without the SDK it is absent, every block below compiles out, and
// TryGetEosIdToken fails closed with a diagnostic instead of the module failing to build.
#if WITH_EOS_SDK
#include "IEOSSDKManager.h"
THIRD_PARTY_INCLUDES_START
#include "eos_sdk.h"          // EOS_Platform_GetAuthInterface
#include "eos_common.h"       // EOS_EResult_ToString, EOS_EpicAccountId_IsValid
#include "eos_auth.h"         // EOS_Auth_CopyIdToken + the logged-in-account accessors
#include "eos_auth_types.h"
THIRD_PARTY_INCLUDES_END
#endif

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

// D17: PlayFab OpenID Connect connection id for Epic Account Services. NON-SECRET -- it names a connection,
// it is not a credential (the secret half lives in PlayFab Game Manager). Empty by default and deliberately
// NOT guessed: set it in Config/DefaultEngine.ini under [ConsoleVariables].
static TAutoConsoleVariable<FString> CVarOnlineEosOidcConnectionId(
	TEXT("afl.Online.EosOidcConnectionId"),
	TEXT(""),
	TEXT("PlayFab OpenID Connect connection id for Epic Account Services (non-secret). Set in DefaultEngine.ini [ConsoleVariables]. Empty = shipping login refuses to run."),
	ECVF_Default);

// Exercise the SHIPPING login path from a non-shipping build. Without this the EOS path would only ever run
// in the one configuration that is hardest to debug, which is how a login ships broken.
static TAutoConsoleVariable<bool> CVarOnlineForceEosLogin(
	TEXT("afl.Online.ForceEosLogin"),
	false,
	TEXT("Non-shipping only: use the SHIPPING EOS/OIDC login instead of the dev CustomID login."),
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
FString UAFLOnlineSubsystem::ResolveDevCustomId() const
{
	const FString Base = CVarOnlineDevCustomId.GetValueOnGameThread();

#if WITH_EDITOR
	// PIE runs every client inside ONE process, and this cvar is process-global -- so without a per-instance
	// suffix EVERY PIE client logs in as the SAME PlayFabId. That is invisible in ordinary play and fatal to a
	// staked match: escrow is keyed (matchId, playFabId), so two participants collapse onto a single escrow row
	// and settlement then correctly refuses the whole match on its count check. It would read as a transport
	// bug when it is really an identity one.
	//
	// A 2-client PIE is the cheapest thing that can stand in for two real players, and MATCH PLAY needs exactly
	// that: FAFLMatchResult::Validate demands exactly two finishing positions (so a solo staked match is
	// impossible) and bars bots from anything staked or rated (so it cannot be filled out with AI either).
	//
	// PIE instance 0 keeps the bare id, so the already-seeded primary account is still the one it logs into and
	// only the additional clients need funding.
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const FWorldContext* WorldContext = GI->GetWorldContext())
		{
			if (WorldContext->PIEInstance > 0)
			{
				return FString::Printf(TEXT("%s_P%d"), *Base, WorldContext->PIEInstance + 1);
			}
		}
	}
#endif

	return Base;
}

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
		ResolveUrl  = FPlatformMisc::GetEnvironmentVariable(TEXT("AFL_RESOLVE_URL"));   // A1.4 (reuses the earn HMAC key)
		EscrowUrl   = FPlatformMisc::GetEnvironmentVariable(TEXT("AFL_ESCROW_URL"));   // match lifecycle, same key
		SettleUrl   = FPlatformMisc::GetEnvironmentVariable(TEXT("AFL_SETTLE_URL"));
		RatingUrl   = FPlatformMisc::GetEnvironmentVariable(TEXT("AFL_RATING_URL"));
		UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] Server signer (%s): key=%s earnUrl=%s resolveUrl=%s escrowUrl=%s settleUrl=%s ratingUrl=%s"),
			IsRunningDedicatedServer() ? TEXT("dedicated server") : TEXT("editor"),
			EarnHmacKey.IsEmpty() ? TEXT("MISSING") : TEXT("held"),
			EarnUrl.IsEmpty() ? TEXT("MISSING") : *EarnUrl,
			ResolveUrl.IsEmpty() ? TEXT("MISSING") : *ResolveUrl,
			EscrowUrl.IsEmpty() ? TEXT("MISSING") : *EscrowUrl,
			SettleUrl.IsEmpty() ? TEXT("MISSING") : *SettleUrl,
			RatingUrl.IsEmpty() ? TEXT("MISSING") : *RatingUrl);
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
	// SHIPPING GATE (non-negotiable): CustomID is a DEV path only and is not even compiled in here. A fixed
	// CustomID in a shipped build is an account-spoof hole -- anyone who learns the id becomes that account.
	// D17 ("EOS Default") makes this a real login rather than a refusal: an Epic Account Services ID token,
	// which the client cannot forge, exchanged at PlayFab via OpenID Connect.
	StartLoginWithEOS();
#else
	// Dev defaults to CustomID (no Epic sign-in required to iterate), but the shipping path is one cvar away
	// so it can actually be tested: afl.Online.ForceEosLogin 1.
	if (CVarOnlineForceEosLogin.GetValueOnGameThread())
	{
		UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] afl.Online.ForceEosLogin=1 -- using the SHIPPING EOS/OIDC login path."));
		StartLoginWithEOS();
	}
	else
	{
		StartLoginWithCustomID();
	}
#endif
}

FString UAFLOnlineSubsystem::ResolveEosOidcConnectionId() const
{
	return CVarOnlineEosOidcConnectionId.GetValueOnGameThread();
}

bool UAFLOnlineSubsystem::TryGetEosIdToken(FString& OutIdToken, FString& OutFailReason) const
{
	// Both cleared up front: the loop below uses OutFailReason.IsEmpty() to distinguish "nothing was signed
	// in anywhere" from "a platform tried and failed", so a caller's stale string must not survive into that.
	OutIdToken.Reset();
	OutFailReason.Reset();

#if !WITH_EOS_SDK
	OutFailReason = TEXT("built without the EOS SDK (WITH_EOS_SDK undefined on this platform)");
	return false;
#else
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		OutFailReason = TEXT("IEOSSDKManager unavailable (is the OnlineSubsystemEOS plugin enabled?)");
		return false;
	}

	// GetActivePlatforms is the only PUBLIC route to a live EOS_HPlatform. There is normally exactly one; we
	// scan because a target can legitimately hold more (e.g. an EOSPlus arrangement) and only one will have a
	// signed-in Epic account.
	const TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0)
	{
		OutFailReason = TEXT("no active EOS platform (EOS did not initialise -- check the credential block in Config/Custom/EOS/DefaultEngine.ini)");
		return false;
	}

	for (const IEOSPlatformHandlePtr& PlatformPtr : Platforms)
	{
		if (!PlatformPtr.IsValid())
		{
			continue;
		}

		const EOS_HPlatform Platform = static_cast<EOS_HPlatform>(*PlatformPtr);
		const EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(Platform);
		if (!AuthHandle || EOS_Auth_GetLoggedInAccountsCount(AuthHandle) <= 0)
		{
			continue;
		}

		const EOS_EpicAccountId AccountId = EOS_Auth_GetLoggedInAccountByIndex(AuthHandle, 0);
		if (EOS_EpicAccountId_IsValid(AccountId) == EOS_FALSE)
		{
			continue;
		}

		EOS_Auth_CopyIdTokenOptions Options = {};
		Options.ApiVersion = EOS_AUTH_COPYIDTOKEN_API_LATEST;
		Options.AccountId  = AccountId;

		EOS_Auth_IdToken* IdToken = nullptr;
		const EOS_EResult CopyResult = EOS_Auth_CopyIdToken(AuthHandle, &Options, &IdToken);

		if (CopyResult == EOS_EResult::EOS_Success && IdToken && IdToken->JsonWebToken)
		{
			OutIdToken = UTF8_TO_TCHAR(IdToken->JsonWebToken);
			// Release on EVERY path out of here -- the SDK allocated it and this is the only owner.
			EOS_Auth_IdToken_Release(IdToken);

			if (!OutIdToken.IsEmpty())
			{
				return true;
			}
			OutFailReason = TEXT("EOS returned an empty ID token");
			continue;   // an empty token from one platform is not a verdict on the others
		}

		if (IdToken)
		{
			EOS_Auth_IdToken_Release(IdToken);
		}

		// Record and KEEP SCANNING. Returning here would contradict the reason this is a loop: a platform
		// that holds a signed-in account but cannot mint a token (expired, mid-refresh) must not veto a
		// sibling platform that can. The last reason survives for the log if no platform succeeds.
		OutFailReason = FString::Printf(TEXT("EOS_Auth_CopyIdToken failed: %s"),
			ANSI_TO_TCHAR(EOS_EResult_ToString(CopyResult)));
	}

	if (OutFailReason.IsEmpty())
	{
		OutFailReason = TEXT("no Epic account is signed in on any active EOS platform");
	}
	return false;
#endif // WITH_EOS_SDK
}

void UAFLOnlineSubsystem::StartLoginWithEOS()
{
	LoginMethod = TEXT("LoginWithOpenIdConnect(EOS)");

	// FAIL CLOSED, LOUDLY, IN THIS ORDER. Both of these are configuration faults, and the whole point of
	// naming them separately is that the log line tells the operator which one to go fix.
	const FString ConnectionId = ResolveEosOidcConnectionId();
	if (ConnectionId.IsEmpty())
	{
		UE_LOG(LogAFLOnline, Error,
			TEXT("[AFLOnline] EOS login refused: afl.Online.EosOidcConnectionId is unset. Set it in Config/DefaultEngine.ini ")
			TEXT("[ConsoleVariables] to the PlayFab OpenID Connect connection configured for Epic Account Services. ")
			TEXT("Refusing rather than guessing -- authenticating against the wrong connection is worse than not authenticating."));
		ResolveLogin(false);
		return;
	}

	FString IdToken, FailReason;
	if (!TryGetEosIdToken(IdToken, FailReason))
	{
		UE_LOG(LogAFLOnline, Error, TEXT("[AFLOnline] EOS login refused: could not obtain an Epic ID token -- %s"), *FailReason);
		ResolveLogin(false);
		return;
	}

	LoginState = EAFLLoginState::InFlight;

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("TitleId"), GetTitleId());
	Body->SetStringField(TEXT("ConnectionId"), ConnectionId);
	Body->SetStringField(TEXT("IdToken"), IdToken);
	Body->SetBoolField(TEXT("CreateAccount"), true);

	FString BodyStr;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body, Writer);

	const FString Url = BaseUrl() / TEXT("Client/LoginWithOpenIdConnect");

	const FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(BodyStr);
	Req->OnProcessRequestComplete().BindUObject(this, &UAFLOnlineSubsystem::HandleLoginResponse);

	// The ID TOKEN IS A BEARER CREDENTIAL -- never log it, not even truncated. Length only, which is enough to
	// tell "we got a token" from "we got an empty string" without putting a usable secret in a log file.
	UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] LoginWithOpenIdConnect -> %s (connectionId=%s, idToken=%d chars)"),
		*Url, *ConnectionId, IdToken.Len());
	Req->ProcessRequest();
}

void UAFLOnlineSubsystem::StartLoginWithCustomID()
{
#if UE_BUILD_SHIPPING
	// Never reachable in shipping (EnsureLogin gates it out); guard anyway so the dev path can't be linked in.
	ResolveLogin(false);
#else
	LoginState  = EAFLLoginState::InFlight;
	LoginMethod = TEXT("LoginWithCustomID(dev)");

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

	UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] %s OK PlayFabId=%s (entityToken=%s)"),
		LoginMethod.IsEmpty() ? TEXT("Login") : *LoginMethod,
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

void UAFLOnlineSubsystem::PostServerSigned(const FString& Url, const FString& Body, TFunction<void(bool, const FString&)> OnComplete)
{
	// SERVER-ONLY: the HMAC key + the target URL are read only on a dedicated server / editor (Initialize). Empty
	// => not a server (or env unset) => refuse to sign. No client process ever signs a server-authoritative call.
	if (EarnHmacKey.IsEmpty() || Url.IsEmpty())
	{
		UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] PostServerSigned SKIP -- key/URL unavailable (server/editor only; set AFL_EARN_HMAC_KEY + the endpoint URL env). IsRunningDedicatedServer()=%d GIsEditor=%d."),
			IsRunningDedicatedServer() ? 1 : 0, GIsEditor ? 1 : 0);
		OnComplete(false, TEXT("skip: key/URL unavailable (server-only)"));
		return;
	}

	// Sign-what-you-send: sign the EXACT FString handed to SetContentAsString below -- no re-serialize between.
	const FString Signature = SignHmacSha256Hex(Body, EarnHmacKey);
	if (Signature.IsEmpty())
	{
		UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] PostServerSigned SKIP -- empty signature (OpenSSL unavailable?)."));
		OnComplete(false, TEXT("skip: empty signature"));
		return;
	}

	const FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);                                          // the FULL endpoint (NOT BaseUrl()/PlayFab)
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetHeader(TEXT("X-Signature"), Signature);           // HMAC-SHA256(body) hex, lowercase
	Req->SetContentAsString(Body);                            // the SAME FString that was signed

	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedOk)
		{
			if (!bConnectedOk || !Response.IsValid())
			{
				UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] PostServerSigned HTTP failed (no response)."));
				OnComplete(false, TEXT("no response"));
				return;
			}
			// Plain HTTP (NOT the PlayFab envelope): success == 200; surface the raw body either way for diagnosis.
			const int32 Http = Response->GetResponseCode();
			const FString RespBody = Response->GetContentAsString();
			const bool bOk = (Http == 200);
			UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] PostServerSigned -> http=%d ok=%d"), Http, bOk ? 1 : 0);
			OnComplete(bOk, RespBody);
		});
	Req->ProcessRequest();
}

FString UAFLOnlineSubsystem::PlayerApiBaseUrl() const
{
	// Env first, so the dedicated-server tooling that already exports AFL_* keeps resolving exactly as before
	// and a local run can point at a different stack without editing config.
	FString Url = FPlatformMisc::GetEnvironmentVariable(TEXT("AFL_API_BASE_URL"));
	if (Url.IsEmpty())
	{
		// The shipping path. A player's process has no environment we control.
		GConfig->GetString(TEXT("AFL.Online"), TEXT("PlayerApiBaseUrl"), Url, GGameIni);
	}
	Url.TrimStartAndEndInline();
	while (Url.EndsWith(TEXT("/")))
	{
		Url.LeftChopInline(1);   // callers pass "/match-status"; a trailing slash would make "//match-status"
	}
	return Url;
}

void UAFLOnlineSubsystem::PostPlayerApi(const FString& EndpointPath, const FString& JsonBody,
	TFunction<void(bool, const FString&)> OnComplete)
{
	const FString Base = PlayerApiBaseUrl();
	if (Base.IsEmpty())
	{
		// Named explicitly. "Matchmaking is broken" with no reason is the failure this message exists to
		// prevent -- a missing config line looks identical to a network fault from the player's side.
		UE_LOG(LogAFLOnline, Error,
			TEXT("[AFLOnline] PostPlayerApi('%s') SKIP -- no API base URL. Set AFL_API_BASE_URL, or "
			     "[AFL.Online] PlayerApiBaseUrl in DefaultGame.ini."), *EndpointPath);
		OnComplete(false, TEXT("skip: no api base url"));
		return;
	}

	if (SessionTicket.IsEmpty())
	{
		// Not a fault to shout about: the player simply is not logged in yet. The caller decides whether to
		// wait (CallWhenLoggedIn) or surface "sign in to play".
		UE_LOG(LogAFLOnline, Warning,
			TEXT("[AFLOnline] PostPlayerApi('%s') SKIP -- no SessionTicket (not logged in)."), *EndpointPath);
		OnComplete(false, TEXT("skip: not logged in"));
		return;
	}

	const FString FullUrl = Base + (EndpointPath.StartsWith(TEXT("/")) ? EndpointPath : TEXT("/") + EndpointPath);

	const FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(FullUrl);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	// The player's OWN credential, in a header rather than the body or a query string -- a query string ends
	// up in access logs and crash reports.
	Req->SetHeader(TEXT("X-SessionTicket"), SessionTicket);
	Req->SetContentAsString(JsonBody);

	Req->OnProcessRequestComplete().BindLambda(
		[OnComplete, EndpointPath](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnectedOk)
		{
			if (!bConnectedOk || !Response.IsValid())
			{
				UE_LOG(LogAFLOnline, Warning, TEXT("[AFLOnline] PostPlayerApi('%s') HTTP failed (no response)."), *EndpointPath);
				OnComplete(false, TEXT("no response"));
				return;
			}
			const int32 Http = Response->GetResponseCode();
			const FString RespBody = Response->GetContentAsString();
			const bool bOk = (Http == 200);
			UE_LOG(LogAFLOnline, Log, TEXT("[AFLOnline] PostPlayerApi('%s') -> http=%d ok=%d"), *EndpointPath, Http, bOk ? 1 : 0);
			OnComplete(bOk, RespBody);
		});
	Req->ProcessRequest();
}

// A1.3b earn + A1.4 resolve: thin wrappers over the shared signed-POST transport. PostServerEarn's WIRE behavior
// is byte-identical to before the extraction (URL=EarnUrl, X-Signature=HMAC(body), body-as-sent, 200-check,
// OnComplete(bOk,body)) -- the earn canary (3c69e132) stays valid; only the diag log prefix changed.
void UAFLOnlineSubsystem::PostServerEarn(const FString& EarnJsonBody, TFunction<void(bool, const FString&)> OnComplete)
{
	PostServerSigned(EarnUrl, EarnJsonBody, MoveTemp(OnComplete));
}

void UAFLOnlineSubsystem::PostServerResolve(const FString& ResolveJsonBody, TFunction<void(bool, const FString&)> OnComplete)
{
	PostServerSigned(ResolveUrl, ResolveJsonBody, MoveTemp(OnComplete));
}

// The three match-lifecycle posts. Thin by design: same signer, same key, same server gate -- only the URL
// differs, so anything more than a URL here would be a second implementation of a solved problem.
void UAFLOnlineSubsystem::PostServerEscrow(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete)
{
	PostServerSigned(EscrowUrl, JsonBody, MoveTemp(OnComplete));
}
void UAFLOnlineSubsystem::PostServerSettle(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete)
{
	PostServerSigned(SettleUrl, JsonBody, MoveTemp(OnComplete));
}
void UAFLOnlineSubsystem::PostServerRating(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete)
{
	PostServerSigned(RatingUrl, JsonBody, MoveTemp(OnComplete));
}

bool UAFLOnlineSubsystem::IsMatchReportingConfigured() const
{
	return !EarnHmacKey.IsEmpty() && !EscrowUrl.IsEmpty() && !SettleUrl.IsEmpty() && !RatingUrl.IsEmpty();
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
