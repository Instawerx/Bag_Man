// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/PlayerStateComponent.h"

#include "Cosmetics/AFLCosmeticServices.h"   // IAFLEntitlementSource (this component IS the real impl), FAFLPlayerId, persistence seam
#include "Templates/Function.h"              // TFunction (A1.2 client-purchase completion callbacks)

#include "AFLWalletComponent.generated.h"

class ALyraPlayerState;
class UAFLCosmeticCatalogSubsystem;

/**
 * Which currency a purchase pays in. IRONICS LOCKED tier model: SPARK (accessible) is payable in EITHER
 * Volts OR Watts; SURGE/ARC/THUNDERBOLT are Volts-only. This lets the STORE offer the player the choice on
 * a dual-priced (SPARK) item -- the two buy paths pass Volts/Watts explicitly. Auto = let the server pick
 * for a dual-priced item (prefer Volts when affordable, else Watts) -- backward-compatible default for the
 * console cheat + any single-arg caller, and the right behavior for single-priced items (it just uses the
 * price that is set).
 */
UENUM(BlueprintType)
enum class EAFLPayCurrency : uint8
{
	Auto   UMETA(DisplayName = "Auto (prefer Volts)"),
	Volts  UMETA(DisplayName = "Volts"),
	Watts  UMETA(DisplayName = "Watts")
};

/** Broadcast whenever the wallet's balance OR owned-set changes (server commit + each client OnRep). The
 *  store wallet/grid widgets bind this -> event-driven UI refresh, NEVER tick (the marketplace-ui mandate).
 *  Carries the current Volts/Watts so a listener can update without a second read. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAFLOnWalletChanged, int32, Volts, int32, Watts);

/** A1.2 verify (afl.Online.VerifyA12): the server-side facts the harness asserts against. */
struct FAFLPurchaseVerifyResult
{
	bool    bLoginOk = false;
	int32   VoBefore = -1;
	int32   VoAfter = -1;                  // PlayFab balance after the legit buy (== VoBefore - price -> server deducted)
	bool    bLegitOwnedOnPlayFab = false;  // token count increased on PlayFab -> server granted
	bool    bMirrorDeducted = false;       // the wallet's LOCAL balance reflected the deduct (display)
	bool    bSpoofRejected = false;        // PurchaseItem(price=1) rejected -> can't cheat the price
	bool    bSpendSpoofRejected = false;   // over-balance PurchaseItem rejected -> faked LOCAL balance UNSPENDABLE
	FString FailNote;
};

/**
 * UAFLWalletComponent -- the server-authoritative player WALLET + entitlement source (S-ECON-WALLET).
 *
 * The economy stops being catalog METADATA (prices on FAFLCatalogEntry) and becomes a live, interactive
 * BALANCE. Three coupled layers, one component:
 *   (a) BALANCE  -- replicated integer Volts/Watts (peg discipline; IRONICS economy LOCKED -> NEVER float).
 *   (b) GATE     -- this component IS the real IAFLEntitlementSource (replaces the #43 permissive null stub):
 *                   IsEntitled = "is this CosmeticId in the player's owned set (or GrantedFree)?".
 *   (c) EARN/SPEND -- server-validated mutation: EarnWatts adds; PurchaseCosmetic reads the catalog price,
 *                     checks the balance + change-timing, deducts, and grants ownership (owned-set += id).
 *
 * WITHIN LYRA (not beside it): same shape as the proven #43 UAFLCosmeticLoadoutComponent --
 * UPlayerStateComponent (replicates to EVERY client; balances/ownership must be readable on the owner's HUD
 * and server-auth), replication enabled in the ctor (no replicated base), authority-guarded mutations that
 * also apply on the listen-host, OnRep for remote clients. Persistence rides the EXISTING (now balance-
 * extended) IAFLCosmeticPersistence seam behind the opaque FAFLPlayerId -> PlayFab drops in at Phase 3 with
 * no call-site change. "Server-authoritative-against-stubbed-backing" -- real-money/bank/top-up stays GATED
 * to Phase 3 (legal). The client NEVER mutates the balance; it requests, the server decides, the value
 * replicates down.
 *
 * INSTRUMENTED PER LAYER (afl.WalletDiag cvar, OFF by default) so a composite failure is diagnosable per
 * seam without bisecting -- the SkinDiag / resolveVia= / SetParam pattern (instrument = infrastructure):
 *   (a) balance replication logs; (b) the gate logs entitled=Y/N + why; (c) earn/spend log the server-side
 *       validation + before/after balance.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLWalletComponent : public UPlayerStateComponent, public IAFLEntitlementSource
{
	GENERATED_BODY()

public:
	// UPlayerStateComponent has no default ctor (only the FObjectInitializer overload) -> forward to Super.
	UAFLWalletComponent(const FObjectInitializer& ObjectInitializer);

	/** Balance/ownership changed -- the store wallet + grid bind this for event-driven refresh (not tick).
	 *  Fires on the server commit AND on each client's OnRep. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Wallet")
	FAFLOnWalletChanged OnWalletChanged;

	/** Diagnostic (client-wallet-refresh splitting check): how many listeners are bound to OnWalletChanged on
	 *  THIS instance. The store's Construct binds it; if a client's OnRep fires with a 0 count, the store never
	 *  bound to this wallet instance (UI-refresh gap, the #43 late-resolve case) -- distinct from a replication
	 *  gap (OnRep never fires on the client). 0 if unbound; -1 only on a null self. */
	int32 GetOnWalletChangedBoundCount() const { return OnWalletChanged.IsBound() ? OnWalletChanged.GetAllObjects().Num() : 0; }

	//~ Balance reads (any client; the owner's HUD reads these) ---------------------------------------
	UFUNCTION(BlueprintPure, Category = "AFL|Wallet")
	int32 GetVolts() const { return Volts; }

	UFUNCTION(BlueprintPure, Category = "AFL|Wallet")
	int32 GetWatts() const { return Watts; }

	/** True if the player owns CosmeticId (in the replicated owned set). Identity/GrantedFree handled by
	 *  the catalog Acquisition check inside the gate, not here. */
	UFUNCTION(BlueprintPure, Category = "AFL|Wallet")
	bool OwnsCosmetic(FName CosmeticId) const { return OwnedCosmeticIds.Contains(CosmeticId); }

	//~ IAFLEntitlementSource (the REAL impl; the loadout's gate resolves this instead of the null stub) -
	virtual bool IsEntitled(const ALyraPlayerState* Player, FName CosmeticId) const override;
	virtual bool OwnsIdentity(const ALyraPlayerState* Player, EAFLIdentityType Type, FName Id) const override;

	//~ Server-authoritative MUTATIONS (client requests; server validates + decides) -------------------

	/** (c) EARN: add Watts to the balance (e.g. match earn / QUANTUM trickle). Authority-only; replicates.
	 *  Server-validated (clamps negatives). The earn SOURCES (match results) wire in later; this is the sink. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, BlueprintAuthorityOnly, Category = "AFL|Wallet")
	void ServerEarnWatts(int32 Amount);

	/** Symmetric Volts grant (pack purchase at real-money enable, Phase 3-gated; here for completeness +
	 *  the dev cheat). Authority-only. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, BlueprintAuthorityOnly, Category = "AFL|Wallet")
	void ServerEarnVolts(int32 Amount);

	/**
	 * (c) SPEND + (b) GRANT: the ONE purchase path. Server-authoritative flow:
	 *   1. Resolve FAFLCatalogEntry for CosmeticId (price + tier + acquisition).
	 *   2. Reject GrantedFree (already owned by all) + already-owned (no double charge).
	 *   3. Compute cost from the entry (Volts ladder, or Watts only for SPARK; Watts-discount later).
	 *   4. Check the balance covers it; reject if not (NO partial / NO negative).
	 *   5. Deduct, add CosmeticId to the owned set (the gate now resolves it) -> both replicate.
	 * The wallet UI calls THIS; there is no client-side balance write.
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, BlueprintAuthorityOnly, Category = "AFL|Wallet")
	void ServerPurchaseCosmetic(FName CosmeticId, EAFLPayCurrency PayWith = EAFLPayCurrency::Auto);

	//~ A1.2 -- PlayFab-native purchase (the anti-spoof path; replaces ServerPurchaseCosmetic for shipping) --

	/**
	 * The REAL purchase path. CLIENT-side: resolve the FAFLCatalogEntry price -> PlayFab Client/PurchaseItem
	 * (server-enforced catalog price + ATOMIC server-side deduct+grant, the player's OWN token) -> reflect
	 * (Option A): re-read OWNERSHIP from PlayFab (authoritative) + mirror-deduct the BALANCE locally (display;
	 * NOT a PlayFab balance overwrite -> preserves the proven local earn loop). A faked LOCAL balance is
	 * UNSPENDABLE (PlayFab spends only PlayFab-held currency). Authority context (front-end / standalone);
	 * purchases happen between matches, never on the untrusted in-match listen-host.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Wallet")
	void ClientRequestPurchase(FName CosmeticId, EAFLPayCurrency PayWith = EAFLPayCurrency::Auto);

	/** Native overload with a completion callback (the verify harness awaits it; bSuccess = PlayFab committed). */
	void ClientRequestPurchase(FName CosmeticId, EAFLPayCurrency PayWith, TFunction<void(bool bSuccess)> OnComplete);

	/** A1.2 verify driver (afl.Online.VerifyA12): legit buy (server deduct+grant) + fake-price-reject +
	 *  over-balance-reject (faked local UNSPENDABLE). Reports the server-side facts; the cheat asserts. */
	void DebugVerifyA12(FName TokenId, FName PremiumId, TFunction<void(const FAFLPurchaseVerifyResult&)> OnDone);

	/** Gameplay earn (extraction cash-out etc.): authority-only NATIVE call into the CommitMutation
	 *  funnel with a caller-named Reason for the diag line. Not an RPC -- gameplay sources already
	 *  run on the server (the Server RPCs above are the client->server cheat/UI hop). Clamps
	 *  negatives like ServerEarnWatts. Extraction cycle 1 (S8 AFL-0805: off-GAS, the wallet rail). */
	void EarnWattsAuthority(int32 Amount, const TCHAR* Reason);

	/** Dev/seed setter (cheat-driven): authority sets the balance directly. Bypasses earn/spend validation
	 *  -- for the (a) balance watch + test seeding, NOT a gameplay path. */
	void DebugSetBalance(int32 InVolts, int32 InWatts);

	/** Dev grant (cheat-driven): authority adds a CosmeticId to the owned set directly (test the gate
	 *  without spending). */
	void DebugGrantOwnership(FName CosmeticId);

protected:
	//~UActorComponent
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End

	/** (a) Replicated balances -- INTEGER (peg). Single OnRep; balance changes are purchase/earn-rare. */
	UPROPERTY(ReplicatedUsing = OnRep_Balance)
	int32 Volts = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Balance)
	int32 Watts = 0;

	/** (b) The replicated owned-cosmetic set -- the entitlement source of truth. FName ids match the
	 *  catalog CosmeticIds (AFL.Edge.*, AFL.Facemask.*, AFL.Ability.*). Replicated so the store UI can grey
	 *  out owned items on the owner's client. */
	UPROPERTY(ReplicatedUsing = OnRep_OwnedSet)
	TArray<FName> OwnedCosmeticIds;

	UFUNCTION()
	void OnRep_Balance();

	UFUNCTION()
	void OnRep_OwnedSet();

private:
	//~ Server RPC validation bodies (the _Implementation live in the .cpp) -----------------------------
	bool ServerEarnWatts_Validate(int32 Amount) { return true; }
	bool ServerEarnVolts_Validate(int32 Amount) { return true; }
	bool ServerPurchaseCosmetic_Validate(FName CosmeticId, EAFLPayCurrency PayWith) { return true; }

	ALyraPlayerState* GetLyraPlayerState() const;

	/** The persistence backend (stub now; PlayFab Phase 3). Resolved the same way the loadout does -- one
	 *  place to swap. Null-tolerant: a missing backend means no load/save (in-bring-up). */
	IAFLCosmeticPersistence* GetPersistence() const;

	/** Derive the opaque player key (mirrors the loadout's MakePlayerId). */
	FAFLPlayerId MakePlayerId() const;

	/** Load balance + owned-set from persistence on the authority at BeginPlay; seed defaults if new. */
	void LoadFromPersistence();

	/** Persist the current balance + owned set (after any authority mutation). */
	void PersistState() const;

	/** Catalog accessor (price lookup for the purchase path). */
	UAFLCosmeticCatalogSubsystem* GetCatalog() const;

	/** Apply an authority balance delta + (optionally) an ownership grant, then replicate + persist + diag.
	 *  The single commit point all server mutations funnel through. A1.2: this + its callers (Purchase/Earn/
	 *  Debug) are DEV/ADVISORY for the BALANCE -- PlayFab is the sole SPEND + OWNERSHIP truth. */
	void CommitMutation(int32 DeltaVolts, int32 DeltaWatts, FName GrantId, const TCHAR* Reason);

	/** A1.2 -- reflect a committed PlayFab purchase (Option A): re-read OWNERSHIP from PlayFab (authoritative;
	 *  REQ-2 bounded-retry, display lags-then-reconciles on failure, NEVER a local patch) + mirror-deduct the
	 *  balance locally (display). Authority-context. */
	void ApplyPurchaseResult(FName CosmeticId, int32 CostVolts, int32 CostWatts, TFunction<void(bool)> OnComplete);
};
