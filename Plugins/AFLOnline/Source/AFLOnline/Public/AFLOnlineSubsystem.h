// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"   // FHttpRequestPtr / FHttpResponsePtr
#include "Templates/Function.h"

#include "AFLOnlineSubsystem.generated.h"

class FJsonObject;

/** Fires once a PlayFab login resolves successfully (SessionTicket + PlayFabId held). */
DECLARE_MULTICAST_DELEGATE(FAFLOnLoggedIn);

/**
 * UAFLOnlineSubsystem -- Phase A1.1: PlayFab CLIENT login + a thin REST transport. SECRET-FREE.
 *
 * Login mints the durable account key (PlayFabId) + the player's own SessionTicket/EntityToken. Reads
 * (GetUserInventory: owned items + VO/WA balance) use the player's OWN token -- no title secret, no Lambda,
 * no Secrets Manager (that apparatus enters A1.2 for earn + custom-validated purchases). The transport
 * (PostClientApi) is generic so A1.2 reuses it.
 *
 * DEV-ONLY LOGIN GATE (see EnsureLogin): LoginWithCustomID is compiled ONLY in non-shipping builds -- a
 * fixed CustomID in a shipped game would be an account-spoof bypass. Shipping login is platform-only
 * (LoginWithSteam / LoginWithOpenIdConnect via CommonUser+OSS), wired in the A1.1-shipped follow-on; both
 * paths yield the identical PlayFabId contract, so the persistence layer never changes.
 *
 * WITHIN LYRA: a UGameInstanceSubsystem in the always-loaded (Default-phase) AFLOnline plugin -- login is
 * resident before the wallet/loadout BeginPlay consume the PlayFabId. Resolved via a static Get (the
 * catalog/economy subsystem pattern).
 */
UCLASS()
class AFLONLINE_API UAFLOnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Resolve from any world-context object. Null before the game instance exists. */
	static UAFLOnlineSubsystem* Get(const UObject* WorldContext);

	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool IsLoggedIn() const { return LoginState == EAFLLoginState::LoggedIn; }
	const FString& GetPlayFabId() const { return PlayFabId; }
	const FString& GetSessionTicket() const { return SessionTicket; }
	const FString& GetEntityToken() const { return EntityToken; }

	/** Broadcast once login succeeds. */
	FAFLOnLoggedIn OnLoggedIn;

	/** Kick a login if not already logged-in / in-flight. Dev -> CustomID; shipping -> platform (gated). */
	void EnsureLogin();

	/** One-shot: Callback(true) when logged in (immediately if already), Callback(false) on failure/timeout.
	 *  Kicks a login if none is running. Lets the persistence LOAD path wait for auth then fall back to cache. */
	void CallWhenLoggedIn(TFunction<void(bool)> Callback, float TimeoutSeconds = 6.0f);

	/** POST a PlayFab Client API (ApiName e.g. "GetUserInventory"); adds X-Authorization: <SessionTicket>
	 *  when bRequireAuth. Parses the {code,status,data} envelope; fires OnComplete(bOk, data). Generic on
	 *  purpose -- A1.2's calls reuse it. */
	void PostClientApi(const FString& ApiName, const TSharedRef<FJsonObject>& Body,
		TFunction<void(bool, TSharedPtr<FJsonObject>)> OnComplete, bool bRequireAuth = true);

private:
	enum class EAFLLoginState : uint8 { NotStarted, InFlight, LoggedIn, Failed };
	EAFLLoginState LoginState = EAFLLoginState::NotStarted;

	FString PlayFabId;
	FString SessionTicket;
	FString EntityToken;

	/** Queued one-shot login waiters (fired on resolve). */
	TArray<TFunction<void(bool)>> PendingLoginCallbacks;

	FString GetTitleId() const;
	FString BaseUrl() const;             // https://<TitleId>.playfabapi.com
	FString ResolveDevCustomId() const;  // dev id (cvar afl.Online.DevCustomId)

	void StartLoginWithCustomID();
	void HandleLoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedOk);
	void ResolveLogin(bool bSuccess);

	/** Parse a PlayFab {code,status,data} envelope; OutData = the "data" object. */
	static bool ParseEnvelope(const FString& Body, TSharedPtr<FJsonObject>& OutData, int32& OutCode);
};
