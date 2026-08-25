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
 * TWO LOGIN PATHS, AND THE SPLIT IS A SECURITY BOUNDARY (see EnsureLogin):
 *
 *   DEV      LoginWithCustomID  -- compiled ONLY in non-shipping builds. A fixed CustomID in a shipped game
 *                                 is an account-spoof bypass: anyone who knows the id becomes that account.
 *   SHIPPING LoginWithOpenIdConnect -- D17, "EOS Default". An Epic Account Services ID token (a JWT the
 *                                 client cannot forge) is exchanged at PlayFab against an OIDC connection
 *                                 configured for Epic. **PlayFab has no LoginWithEpic**; OIDC is the route.
 *
 * Both paths land in the SAME HandleLoginResponse and yield the identical PlayFabId/SessionTicket contract,
 * so nothing downstream -- persistence, wallet, the ?PlayFabId= reconcile key -- knows or cares which ran.
 *
 * The shipping path is compiled into EVERY configuration, not just Shipping: a login path that only exists
 * in the config you cannot attach a debugger to is a path nobody can test. `afl.Online.ForceEosLogin 1`
 * exercises it from a dev build. What is NOT compiled into shipping is CustomID, which is the actual hazard.
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

	/**
	 * T2 identity-join SWAP-POINT: the reconcile key this client carries in its ?PlayFabId= connect option, matched
	 * server-side against the matchmaker roster's member.id (UAFLMatchmakerDataProvider). Returns the master
	 * PlayFabId today (== the title_player_account Entity.Id by PlayFab convention). If a real matchmaker ticket
	 * ever shows the roster keys on EntityToken.Entity.Id instead, change ONLY this accessor (the login response
	 * already carries Entity.Id) -- the connect wiring + the provider are unaffected.
	 */
	const FString& GetReconcileKey() const { return PlayFabId; }

	/** The connect-URL option carrying the reconcile key -- the client appends this to its travel URL at session
	 *  join (the live append lands with the online client-connect path, S12). Empty when not logged in. */
	FString GetConnectOptions() const
	{
		return PlayFabId.IsEmpty() ? FString() : FString::Printf(TEXT("?PlayFabId=%s"), *PlayFabId);
	}

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

	/** A1.3b: POST a signed earn to the server-authoritative /earn Lambda -- a SIBLING to PostClientApi, not a
	 *  reuse (PostClientApi is PlayFab-hardwired: base URL, X-Authorization, {code,status,data} envelope). Signs
	 *  the EXACT EarnJsonBody with the server-only earn HMAC key and sends that same body. Plain HTTP:
	 *  OnComplete(bOk = HTTP 200, RespBody = the raw {success,newBalance,nonce} or the error body for diagnosis).
	 *  SERVER-ONLY: if the earn key/URL are unset (not a dedicated server) it logs a skip and returns without signing. */
	void PostServerEarn(const FString& EarnJsonBody, TFunction<void(bool, const FString&)> OnComplete);

	/** A1.4: POST a signed body to the server-authoritative /resolve-identity Lambda (SessionTicket -> verified
	 *  PlayFabId). SAME signer + HMAC key + server gate as PostServerEarn (one game-server caller); only the URL
	 *  differs. Server-only. OnComplete(bOk = HTTP 200, RespBody = the raw {playFabId} or the error body). */
	void PostServerResolve(const FString& ResolveJsonBody, TFunction<void(bool, const FString&)> OnComplete);

	/**
	 * The three MATCH-LIFECYCLE endpoints. All three are the SAME signer, the SAME HMAC key and the SAME
	 * server-only gate as PostServerEarn -- one game-server caller, one inbound key. Only the URL differs,
	 * which is why these are three lines rather than three implementations.
	 *
	 *   Escrow  POST /escrow-entry   at match START, one call per participant. DEBITS the stake.
	 *   Settle  POST /settle-match   at match END. Pays the curve against the escrow rows.
	 *   Rating  POST /update-rating  at match END. Moves the ladder. **Carries NO stake field** -- the
	 *                                endpoint rejects one outright, because a rating that reads stake size
	 *                                would make rank buyable (matchmaking.md §10.1).
	 *
	 * All are idempotent server-side on their own keys, so a retry is safe and a duplicate is a no-op.
	 */
	void PostServerEscrow(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete);
	void PostServerSettle(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete);
	void PostServerRating(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete);

	/** CC-3.5: POST a signed body to /creator-builds (saved creator builds, load and save).
	 *  The SIXTH sibling of PostServerEarn -- same signer, same HMAC key, same server-only gate; only
	 *  the URL differs. The backend deliberately REUSES the earn inbound key rather than minting a
	 *  second, because the design intent recorded here is one game-server caller, one inbound key. */
	void PostServerCreatorBuilds(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete);

	/** True when the server signer is configured (key + all three match URLs present). Lets a caller log a
	 *  single clear "economy not wired" line instead of three identical per-endpoint skips. */
	bool IsMatchReportingConfigured() const;

	/**
	 * POST to one of OUR player-facing Lambdas as THE PLAYER -- the deliberate inverse of PostServerSigned.
	 *
	 * ⚠ IT SIGNS NOTHING, AND THAT IS THE POINT. The server endpoints authenticate a trusted caller with a
	 * shared HMAC key; a client cannot hold one, because a key shipped to every player is a key every player
	 * has. These endpoints instead authenticate the player AS THEMSELVES with their own PlayFab
	 * SessionTicket, which PlayFab vouches for and which names exactly one account.
	 *
	 * The distinction is not stylistic. /create-ticket takes a SessionTicket and lets the SERVER author the
	 * stake attributes; /match-status takes a SessionTicket and returns only the caller's own row. Neither
	 * accepts a player id, so neither can be aimed at somebody else.
	 *
	 * Needed because the two existing transports both refuse this job: PostClientApi is hardwired to
	 * PlayFab's base URL, auth header and {code,status,data} envelope, and the PostServer* family is
	 * HMAC-gated and returns early on any process that is not a dedicated server.
	 *
	 * EndpointPath is appended to the configured API base ("/match-status"). OnComplete(bOk = HTTP 200, raw body).
	 */
	void PostPlayerApi(const FString& EndpointPath, const FString& JsonBody,
		TFunction<void(bool, const FString&)> OnComplete);

	/**
	 * Base URL of our Lambda API for CLIENT calls.
	 *
	 * ⚠ CONFIG, NOT ENVIRONMENT. Every server URL here is read from an env var, which works because a
	 * dedicated server is launched by our own tooling. A shipping client is launched by the player and has no
	 * environment to read -- so a client that resolved its endpoint the same way would silently have none and
	 * fail to matchmake with no explanation. Read from DefaultGame.ini; the env var still wins when present so
	 * local server-side tooling keeps behaving identically.
	 */
	FString PlayerApiBaseUrl() const;

private:
	enum class EAFLLoginState : uint8 { NotStarted, InFlight, LoggedIn, Failed };
	EAFLLoginState LoginState = EAFLLoginState::NotStarted;

	FString PlayFabId;
	FString SessionTicket;
	FString EntityToken;

	// -- A1.3b earn signer (server-only) --
	/** HMAC-SHA256(Body, Key) as LOWERCASE hex (the backend compares lowercase, constant-time). UTF-8 bytes for
	 *  BOTH inputs; returns the digest length HMAC reports (32 for sha256), never a hardcoded 32. OpenSSL-backed;
	 *  returns empty if OpenSSL is unavailable on the platform. The Body signed MUST equal the body sent. */
	static FString SignHmacSha256Hex(const FString& Body, const FString& Key);

	/** The earn HMAC key + full /earn endpoint URL, read from the environment ONCE on init, only on a dedicated
	 *  server (production) or in the editor (dev canary) -- never in a cooked client process. Empty otherwise. */
	FString EarnHmacKey;
	FString EarnUrl;
	/** A1.4 /resolve-identity endpoint URL (env AFL_RESOLVE_URL), read once under the SAME gate as EarnUrl. */
	FString ResolveUrl;
	/** The match-lifecycle endpoints (env AFL_ESCROW_URL / AFL_SETTLE_URL / AFL_RATING_URL), read once under
	 *  the SAME gate as EarnUrl -- so they never exist in a cooked client process. */
	FString EscrowUrl;
	FString SettleUrl;
	FString RatingUrl;
	FString CreatorBuildsUrl;
	/** /purchase-bundle. Same HMAC key as earn. The bundle SSOT is the mint-ledger row, not this URL. */
	FString BundleUrl;
	/** CC-X30 /counted-entitlement. Same HMAC key. The counted set's SSOT is PlayFab ReadOnlyData behind
	 *  this endpoint -- NOT the local AFLEconomy SaveGame, which is now only a mirror. */
	FString CountedUrl;
	FString ConditionalUrl;

	/** Shared signed-POST transport for the server-authoritative endpoints (A1.3b earn + A1.4 resolve): sign the
	 *  EXACT Body with EarnHmacKey, POST it to Url with X-Signature, plain-HTTP-200 completion. Server-only
	 *  (empty key/URL -> logged skip). PostServerEarn/PostServerResolve are thin wrappers over this. */
	void PostServerSigned(const FString& Url, const FString& Body, TFunction<void(bool, const FString&)> OnComplete);

public:
	/**
	 * POST a signed purchase to /purchase-bundle. The body carries ONLY {playFabId, bundleId, nonce, ts}:
	 * price, children and cap are read server-side from the mint-ledger row and are never sent, so a
	 * tampered request cannot change what is charged or what is granted.
	 *
	 * Server-only by construction -- PostServerSigned refuses to sign without the HMAC key, which no
	 * client process holds. A client attempt fails closed rather than granting.
	 */
	void PostServerPurchaseBundle(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete);

	/** True when /purchase-bundle can actually be signed. Lets a caller say 'not configured' rather
	 *  than reporting a purchase failure for an unwired endpoint. */
	bool IsBundlePurchaseConfigured() const { return !BundleUrl.IsEmpty() && !EarnHmacKey.IsEmpty(); }

	/**
	 * POST a signed counted-entitlement op to /counted-entitlement. Body is {playFabId, op, key, ...}:
	 *   op:read   -> {counts:{key:n}}          the authoritative set
	 *   op:grant  -> {key, count}              count is the NEW authoritative total, post-increment
	 *   op:redeem -> {key, count, granted}     spends exactly one and grants targetId
	 *
	 * Same server-only construction as the bundle post: PostServerSigned refuses to sign without the
	 * HMAC key, which no client process holds, so a client attempt fails closed rather than granting.
	 */
	void PostServerCountedEntitlement(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete);

	/** True when /counted-entitlement can be signed. A caller that gets false is in bring-up with no
	 *  backend, which is a DIFFERENT state from a call that was made and refused -- and the two must
	 *  never collapse into one 'it didn't work'. */
	bool IsCountedEntitlementConfigured() const { return !CountedUrl.IsEmpty() && !EarnHmacKey.IsEmpty(); }

	/**
	 * POST a signed conditional-entitlement op to /conditional-entitlement.
	 * Body is {playFabId, op, conditionId, state, expiresAt, nonce, ts}: op=read|set.
	 */
	void PostServerConditionalEntitlement(const FString& JsonBody, TFunction<void(bool, const FString&)> OnComplete);

	/** True when /conditional-entitlement can be signed. A caller that gets false is in bring-up with
	 *  no backend, which is NOT a refusal and must not be reported as one. */
	bool IsConditionalEntitlementConfigured() const { return !ConditionalUrl.IsEmpty() && !EarnHmacKey.IsEmpty(); }

private:

	/** Queued one-shot login waiters (fired on resolve). */
	TArray<TFunction<void(bool)>> PendingLoginCallbacks;

	FString GetTitleId() const;
	FString BaseUrl() const;             // https://<TitleId>.playfabapi.com
	FString ResolveDevCustomId() const;  // dev id (cvar afl.Online.DevCustomId)

	/** PlayFab OpenID Connect connection id (Game Manager -> Add-ons -> OpenID Connect), naming the connection
	 *  configured against Epic Account Services. NON-SECRET (an identifier, not a credential) -- set via
	 *  cvar afl.Online.EosOidcConnectionId, which DefaultEngine.ini [ConsoleVariables] populates. Empty is a
	 *  hard failure, never a fallback: logging in against the wrong connection is worse than not logging in. */
	FString ResolveEosOidcConnectionId() const;

	/**
	 * Copy the local Epic account's ID TOKEN (a JWT) out of the live EOS platform.
	 *
	 * **The ID token, NOT the access token.** The EOS SDK is explicit that identity verification against a
	 * third party must use EOS_Auth_CopyIdToken; EOS_Auth_CopyUserAuthToken yields an ACCESS token, which is
	 * what OnlineSubsystemEOS's IOnlineIdentity::GetAuthToken returns and is the wrong artifact for PlayFab's
	 * LoginWithOpenIdConnect. Going to the SDK directly is therefore deliberate, not a shortcut around the OSS.
	 *
	 * Returns false with a human-readable OutFailReason on every failure path (no SDK, no platform, nobody
	 * signed in, token expired), because a shipping login failure must be diagnosable from the log alone.
	 */
	bool TryGetEosIdToken(FString& OutIdToken, FString& OutFailReason) const;

	void StartLoginWithCustomID();
	/** SHIPPING login: EOS ID token -> PlayFab Client/LoginWithOpenIdConnect. Compiled in ALL configurations
	 *  on purpose -- a path that only exists in shipping is a path nobody can test (cvar afl.Online.ForceEosLogin
	 *  exercises it from a dev build). Resolves through the SAME HandleLoginResponse as the dev path, so the
	 *  PlayFabId/SessionTicket contract downstream is byte-identical whichever way the player signed in. */
	void StartLoginWithEOS();
	void HandleLoginResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedOk);
	void ResolveLogin(bool bSuccess);

	/** Which login produced the current attempt -- for logging only, so the success line names the real path. */
	FString LoginMethod;

	/** Parse a PlayFab {code,status,data} envelope; OutData = the "data" object. */
	static bool ParseEnvelope(const FString& Body, TSharedPtr<FJsonObject>& OutData, int32& OutCode);
};
