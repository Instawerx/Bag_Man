# Backend Entitlement Reference

Phase 9 of the pipeline. Server-authoritative ownership, platform store
receipt validation, currency, and save data integrity. Without this layer,
your cosmetic shop is a client-trust system anyone can cheat in 5 minutes.

---

## Core Principle: Server Is Source of Truth

The single most important rule of any monetized cosmetic system:

```
The CLIENT requests purchases and equips.
The SERVER decides what the player owns.

Save data is a cache. The backend is the database.
```

Why this matters:
- Client save data is trivially editable (a JSON file on PC)
- Player platform ID can persist across devices — entitlement must follow
- Refunds, fraud, and chargebacks require server-side revocation
- Cross-platform play means the same player owns the same cosmetics on
  PC, console, and mobile — only a backend can know this

If you're shipping a single-player offline-only game, you can relax this.
For anything with multiplayer or monetization, the server-authoritative
model is non-negotiable.

---

## Entitlement Subsystem Architecture

```
                    ┌───────────────────────┐
                    │  Backend Entitlement  │
                    │  Service              │
                    │  (your service or     │
                    │   platform store)     │
                    └────────────┬──────────┘
                                 │
                  GRPC/REST/Platform SDK
                                 │
                    ┌────────────▼──────────┐
                    │  Dedicated Server     │
                    │  EntitlementSubsystem │
                    │  (authoritative)      │
                    └────────────┬──────────┘
                                 │
                       Replicated state
                                 │
                    ┌────────────▼──────────┐
                    │  Client                │
                    │  EntitlementSubsystem  │
                    │  (cached, read-only)   │
                    └────────────┬──────────┘
                                 │
                                 │
            ┌────────────────────┼────────────────────┐
            ▼                    ▼                    ▼
       Shop UI            Cosmetic Equip         Save Data
   (display owned)    (validate equipped)     (offline cache)
```

The client subsystem is **read-only** — it caches entitlements for UI display
and offline play, but never grants or revokes. All mutations go through the
server.

---

## Entitlement Subsystem Interface

```cpp
UCLASS()
class <PROJECT>GAME_API U<Project>EntitlementSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Does the given player own this skin id? */
    UFUNCTION(BlueprintPure, Category = "Entitlement")
    bool IsOwned(const FUniqueNetIdRepl& PlayerId, FName SkinId) const;

    /** All skins owned by the player */
    UFUNCTION(BlueprintPure, Category = "Entitlement")
    TArray<FName> GetOwnedSkins(const FUniqueNetIdRepl& PlayerId) const;

    /** SERVER ONLY — grants entitlement and persists to backend */
    void GrantEntitlement(const FUniqueNetIdRepl& PlayerId, FName SkinId);

    /** SERVER ONLY — revokes entitlement (refund, fraud, etc.) */
    void RevokeEntitlement(const FUniqueNetIdRepl& PlayerId, FName SkinId);

    /** Sync entitlements from backend for the local player on login */
    void SyncLocalPlayerEntitlements();

    /** Multicast when entitlement set changes — UI binds to this */
    UPROPERTY(BlueprintAssignable, Category = "Entitlement")
    FOnEntitlementsChanged OnEntitlementsChanged;

protected:
    /** Calls the backend to fetch entitlements for a player */
    virtual void FetchFromBackend(
        const FUniqueNetIdRepl& PlayerId,
        TFunction<void(const TArray<FName>&)> OnComplete);

    /** Calls the backend to persist a granted entitlement */
    virtual void PersistGrant(
        const FUniqueNetIdRepl& PlayerId, FName SkinId);

    /** Calls the backend to revoke an entitlement */
    virtual void PersistRevoke(
        const FUniqueNetIdRepl& PlayerId, FName SkinId);

private:
    /** In-memory cache, replicated server → owning client */
    UPROPERTY()
    TMap<FString /*PlayerIdAsString*/, FEntitlementSet> EntitlementCache;
};

USTRUCT()
struct FEntitlementSet
{
    GENERATED_BODY()

    UPROPERTY()
    TSet<FName> OwnedSkinIds;

    UPROPERTY()
    FDateTime LastSyncUtc;
};
```

The interface lives in the base game (`<Project>Game` module). The actual
backend integration (HTTP / gRPC / platform SDK calls) goes in a separate
module so different backends can be swapped (your service vs PlayFab vs
Nakama vs raw platform stores).

---

## Server-Side Grant Implementation

```cpp
void U<Project>EntitlementSubsystem::GrantEntitlement(
    const FUniqueNetIdRepl& PlayerId, FName SkinId)
{
    // Server-only guard — clients can't call this directly
    if (!IsServer())
    {
        UE_LOG(LogEntitlement, Error,
            TEXT("GrantEntitlement called on client — rejected"));
        return;
    }

    if (!PlayerId.IsValid())
    {
        UE_LOG(LogEntitlement, Warning,
            TEXT("Invalid player id in GrantEntitlement"));
        return;
    }

    const FString PlayerIdStr = PlayerId.ToString();
    FEntitlementSet& Set = EntitlementCache.FindOrAdd(PlayerIdStr);

    if (Set.OwnedSkinIds.Contains(SkinId))
    {
        // Already granted — idempotent, no-op
        return;
    }

    Set.OwnedSkinIds.Add(SkinId);
    Set.LastSyncUtc = FDateTime::UtcNow();

    // Persist to backend before notifying client to avoid client thinking
    // they own something that's not yet committed
    PersistGrant(PlayerId, SkinId);

    // Notify (client receives via replicated PlayerState or direct RPC)
    NotifyEntitlementChange(PlayerId);
}
```

Idempotency is critical — `GrantEntitlement` must be safe to call twice for
the same skin. Network retries, server crash recovery, and double-clicks
will all cause duplicate calls in production.

---

## Platform Store Integration

For premium cosmetic purchases, the platform store handles real-money
transactions and provides a receipt. The server validates the receipt
before granting entitlement.

| Platform | SDK / API | Receipt Flow |
|---|---|---|
| Steam | Steamworks SDK — `ISteamMicroTxn` | Steam → game server → ISteamUser::RequestEncryptedAppTicket |
| PSN | PS5 Commerce SDK | PS5 SDK → game server → PSN service validation |
| Xbox | Microsoft Store Services | MS Store → game server → Xbox Live commerce REST |
| iOS | StoreKit 2 | App Store → game server → App Store Server API (verify JWS) |
| Android | Google Play Billing | Play → game server → Google Play Developer API (verify token) |

Lyra ships an `OnlineSubsystem` integration. Use the OSS purchase interface
where available to abstract platform differences:

```cpp
void U<Project>PurchaseSubsystem::RequestPurchase(
    FName SkinId, FOnPurchaseComplete Callback)
{
    const IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) { Callback.ExecuteIfBound(false); return; }

    const IOnlinePurchasePtr Purchase = OSS->GetPurchaseInterface();
    if (!Purchase.IsValid()) { Callback.ExecuteIfBound(false); return; }

    F<Project>SkinCatalogRow Row;
    if (!Catalog->GetCatalogRow(SkinId, Row))
    {
        Callback.ExecuteIfBound(false);
        return;
    }

    // Map our SkinId → the platform's product id (set in catalog row or
    // via a side table specific to each platform's store)
    const FString PlatformProductId = GetPlatformProductId(Row, SkinId);

    FPurchaseCheckoutRequest Request;
    Request.AddPurchaseOffer(
        FOfferNamespace(), PlatformProductId, /*Quantity=*/1, /*bIsConsumable=*/false);

    const FUniqueNetIdRef UserId = GetLocalPlayerUniqueId();
    Purchase->Checkout(*UserId, Request,
        FOnPurchaseCheckoutComplete::CreateLambda(
            [this, SkinId, Callback](
                const FOnlineError& Error,
                const TSharedRef<FPurchaseReceipt>& Receipt)
            {
                if (Error.WasSuccessful())
                {
                    // Send receipt to OUR backend for validation
                    ValidateReceiptOnBackend(Receipt, SkinId, Callback);
                }
                else
                {
                    Callback.ExecuteIfBound(false);
                }
            }));
}
```

**Never trust the client-side `WasSuccessful`** — a tampered client can
fake it. The receipt validation must happen on your server, which queries
the platform's authoritative validation endpoint with the receipt token.

---

## Receipt Validation on Backend

The pattern (pseudocode for a Node/Python/Go backend):

```
POST /entitlements/grant
Headers: Authorization: Bearer <player session token>
Body: {
  skinId: "AlphaHelmet",
  platform: "iOS",
  receipt: "<base64 JWS receipt blob>"
}

Backend:
1. Authenticate session token → resolve playerId
2. Switch on platform:
   - iOS: POST to App Store Server API verifyReceipt endpoint
   - Android: GET to Google Play Developer API purchases.products.get
   - Steam: query ISteamMicroTxn::GetUserInfo
   - etc.
3. Verify receipt:
   - Signature matches platform's public key
   - Product id matches the requested skinId's platform product id
   - Receipt is not already consumed (check receipt id against a used-receipts table)
   - Player id in receipt matches authenticated player
4. Atomically:
   - Insert into entitlements table (playerId, skinId, sourceReceiptId, grantedAt)
   - Insert into used-receipts table (receiptId, playerId, grantedAt)
5. Return 200 OK with new entitlement record
6. Game server picks up entitlement on next sync (or via push)
```

The used-receipts table is critical — replaying a receipt should grant the
skin once. Platform stores re-deliver receipts on app reinstall, so the
backend must handle "I'm seeing this receipt id again, player already owns
the skin → return success but don't duplicate".

---

## Cross-Platform Account Linking

If you support cross-progression, the player's entitlements must follow
their account across platforms. The standard approach:

```
1. Player creates a <Studio> account on first launch
2. Player links their platform identities (PSN, Xbox, Steam, App Store, Play)
   to that <Studio> account
3. Backend stores entitlements keyed by <Studio> account id, NOT platform id
4. Per-platform receipt validation grants entitlement to <Studio> account
5. On login from any platform, the player's <Studio> account loads with
   all entitlements regardless of which platform purchased each one
```

This requires a backend account system. Off-the-shelf options:
- PlayFab (Microsoft)
- AccelByte
- Nakama
- Custom (Auth0 / Firebase Auth + your own service)

Don't build your own from scratch unless you have specific compliance
needs — account systems have non-obvious requirements (GDPR deletion,
ban appeals, password recovery, MFA) that take months to get right.

---

## Wallet Subsystem (Soft + Premium Currency)

Currencies are tracked server-side with the same authority model as
entitlements.

```cpp
UCLASS()
class U<Project>WalletSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Get current balance for a currency type */
    UFUNCTION(BlueprintPure, Category = "Wallet")
    int32 GetBalance(
        const FUniqueNetIdRepl& PlayerId,
        FGameplayTag CurrencyTag) const;

    /** SERVER — atomically deduct currency. Returns false if insufficient. */
    bool TrySpend(
        const FUniqueNetIdRepl& PlayerId,
        FGameplayTag CurrencyTag,
        int32 Amount);

    /** SERVER — grant currency (rewards, refunds, etc.) */
    void Grant(
        const FUniqueNetIdRepl& PlayerId,
        FGameplayTag CurrencyTag,
        int32 Amount,
        const FString& SourceTag);

    UPROPERTY(BlueprintAssignable, Category = "Wallet")
    FOnBalanceChanged OnBalanceChanged;

private:
    /** Persist balance change to backend, atomically */
    bool PersistBalanceMutation(
        const FUniqueNetIdRepl& PlayerId,
        FGameplayTag CurrencyTag,
        int32 Delta,
        const FString& Source);
};
```

`TrySpend` must call into a single atomic database operation on the backend
— typically `UPDATE wallets SET balance = balance - $amount WHERE
playerId = $player AND currencyType = $currency AND balance >= $amount`.
If the row count is 1, the spend succeeded. If 0, insufficient funds.

Splitting "check balance → deduct" into two operations is the textbook
race condition that lets a player double-spend with two simultaneous
purchases.

---

## Save Data — Cache Only, Never Source of Truth

The local `ULyraSaveGame` extension stores entitlements as a cache for two
purposes:

```
1. Offline play — player can equip owned skins without connecting
2. Instant UI display — show owned skins on shop open without server roundtrip
```

But on login:

```
1. Load local save with cached entitlements
2. Immediately fire SyncLocalPlayerEntitlements
3. Backend response is authoritative — overwrite local cache
4. Notify UI of any diff between cached and authoritative
```

If the backend says the player owns skin X and the local cache disagrees,
the backend wins. Never the other way.

---

## Currency Tag Hierarchy

```ini
+GameplayTagList=(Tag="Currency.SoftCoin",      DevComment="Earned in-game, no real money")
+GameplayTagList=(Tag="Currency.PremiumGem",    DevComment="Purchased with real money or earned slowly")
+GameplayTagList=(Tag="Currency.EventToken",    DevComment="Limited-time event currency, expires")
+GameplayTagList=(Tag="Currency.BattlePassXP",  DevComment="Not spendable — progression resource")
```

Each currency tag has rules:
- Can it be earned? (soft yes, premium usually only via purchase)
- Does it expire? (event tokens yes, soft/premium no)
- Can it be refunded? (premium yes via platform, soft never)
- Cross-platform shared? (premium typically yes, event tokens often no)

---

## Anti-Cheat Considerations

For multiplayer games with cosmetic visibility:

```
Threat: Player modifies client save to "own" skins they didn't buy
Defense: Server filters which skins are visible to other players. If
         server doesn't have an entitlement record, the client sees the
         cheater as wearing the default skin regardless of what they
         locally equipped.

Threat: Player intercepts purchase RPC and grants themselves currency
Defense: Currency mutations only on server, never via client RPC. Client
         requests purchases via PlatformPurchase flow which requires a
         valid platform receipt the client cannot forge.

Threat: Player crafts a fake entitlement and replicates it
Defense: Entitlement replication is server → owning client only. Clients
         can't replicate entitlements TO the server. Server periodically
         re-syncs from backend to catch any drift.

Threat: Player uses a refunded purchase to keep the skin
Defense: Platform store webhooks notify backend of refunds. Backend
         revokes entitlement. Client picks up revocation on next sync.
```

Compromised clients are inevitable — the goal is to prevent compromised
clients from affecting other players' experience or the studio's revenue.
A player cheating themselves cosmetics they only see locally is annoying
but not catastrophic.

---

## Verification Checklist — End of Phase 9

```
□ EntitlementSubsystem implemented; server-only mutation guards in place
□ Receipt validation works end-to-end on at least one platform (iOS or Android first)
□ Cross-platform account linking works if cross-progression is shipped
□ TrySpend is atomic at the database level (verified with concurrent purchase test)
□ Refund webhook from at least one platform handled correctly
□ Save data treated as cache; backend is source of truth on login
□ Replicated entitlements only server → owning client (no client → server replication)
□ Cosmetic display in multiplayer filtered by server-side entitlement check
□ "Replay receipt" test passes — same receipt twice grants only one entitlement
□ Used-receipt table on backend prevents replay
□ Player ban / unban flow can revoke / restore entitlements
```
