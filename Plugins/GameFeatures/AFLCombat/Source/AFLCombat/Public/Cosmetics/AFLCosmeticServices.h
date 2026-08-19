// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Cosmetics/AFLCosmeticSelectionTypes.h"

#include "AFLCosmeticServices.generated.h"

class ALyraPlayerState;

/**
 * FAFLPlayerId -- OPAQUE cross-session player key for the persistence seam (#43, D8).
 *
 * Deliberately NOT `using FAFLPlayerId = FString;` and NOT a struct with a public string. The backing
 * (today: a net-id string) is PRIVATE. Call sites construct one explicitly and compare them; nothing
 * outside the persistence implementation can read the backing AS a string. When PlayFab (Phase 3)
 * swaps in its own player-id mapping, it changes what's INSIDE this wrapper and no call site knows or
 * cares -- that opacity, plus the async-shaped interface below, is what keeps the stub->PlayFab swap
 * confined to the one impl class. If this wrapper ever leaked its string, call sites would start
 * formatting/assuming its shape and the deferral would rot across every site.
 *
 * #43 does NOT solve cross-session identity (that's the account-system / S-ECON-WALLET dependency):
 * the stub keys on whatever string MakeFromBacking() captures locally; the real account id fills in later.
 */
USTRUCT(BlueprintType)
struct FAFLPlayerId
{
	GENERATED_BODY()

	FAFLPlayerId() = default;

	/** Explicit named construction from the current backing. Only the persistence layer + the component
	 *  that derives the key call this -- it is intentionally not a string-conversion ctor. */
	static FAFLPlayerId MakeFromBacking(const FString& InBacking)
	{
		FAFLPlayerId Id;
		Id.Backing = InBacking;
		return Id;
	}

	/** True once a real backing has been captured (vs a default-constructed empty key). */
	bool IsValid() const { return !Backing.IsEmpty(); }

	bool operator==(const FAFLPlayerId& Other) const { return Backing == Other.Backing; }
	bool operator!=(const FAFLPlayerId& Other) const { return !(*this == Other); }

	/** Map-key support (the owned-set / selection stores key on this). Hashes the opaque backing
	 *  WITHOUT exposing it -- callers get a hash, never the string. */
	friend uint32 GetTypeHash(const FAFLPlayerId& Id) { return GetTypeHash(Id.Backing); }

private:
	/** The opaque backing. PRIVATE on purpose -- see the class comment. Only this type's own members
	 *  (and, by extension, the persistence impl that constructs keys) touch it. */
	UPROPERTY()
	FString Backing;
};

/**
 * Result of an async selection load. The stub completes synchronously but presents the async surface
 * so PlayFab's network latency needs no signature change later.
 */
DECLARE_DELEGATE_TwoParams(FAFLOnSelectionLoaded, bool /*bFound*/, const FAFLCosmeticSelection& /*Selection*/);
DECLARE_DELEGATE_TwoParams(FAFLOnOwnedSetLoaded, bool /*bOk*/, const TArray<FName>& /*OwnedCosmeticIds*/);
/** CC-3.3 -- COUNTED entitlement. Distinct from the boolean owned-set above: this carries HOW MANY.
 *  A boolean set can only answer "does the player own X"; a slot ladder needs "how many X", and there
 *  was no shape on this seam that could hold it. */
/** Alias so the TMap comma does not split the delegate macro's argument list. */
using FAFLCountedEntitlementMap = TMap<FName, int32>;
DECLARE_DELEGATE_TwoParams(FAFLOnCountedSetLoaded, bool /*bOk*/, const FAFLCountedEntitlementMap& /*Counts*/);
// S-ECON-WALLET (Fork A): balance rides the SAME persistence seam (one interface for all of a player's
// economic state -- selection + owned-set + balance -- behind one PlayFab-ready store). Async-shaped like
// the others. Volts + Watts are INTEGER (peg discipline; IRONICS economy LOCKED). bFound=false on a new
// player -> the wallet seeds defaults.
DECLARE_DELEGATE_ThreeParams(FAFLOnBalanceLoaded, bool /*bFound*/, int32 /*Volts*/, int32 /*Watts*/);

// S-ECON WRITE-SIDE (Phase 1): the authoritative PlayFab TRANSACTION completions -- async-shaped like the loads
// above (single-cast delegate). EarnComplete carries the raw /earn response (the caller parses newBalance +
// logs the AFL_A13S3 grant); PurchaseComplete is accept/reject (PlayFab spends+grants server-side; false = rejected).
DECLARE_DELEGATE_TwoParams(FAFLOnEarnComplete, bool /*bOk*/, const FString& /*Resp*/);
DECLARE_DELEGATE_OneParam(FAFLOnPurchaseComplete, bool /*bAccepted*/);

// ---------------------------------------------------------------------------------------------------
// Entitlement seam -- "does this player own this cosmetic?" The gate ASKS this; it does not implement
// policy. Permissive impl now (everyone owns the basics); S-ECON-WALLET implements it against the
// owned-set later. ALWAYS CALLED, so the call site is proven from day one.
// ---------------------------------------------------------------------------------------------------
/**
 * CC-4.1 -- CONDITIONAL ENTITLEMENT. The THIRD entitlement shape, and the first with a lifetime.
 *
 * The two that existed are both PERMANENT once granted: a boolean owned-set answers "do you own it"
 * and a counted entitlement answers "how many". Neither can express "you have this WHILE something
 * else is true" -- a subscription perk, a season pass, a trial. There is no revoke, expire, or
 * subscription-derived grant anywhere in the codebase to conform to; this is new construction.
 *
 * THREE STATES, NOT A BOOLEAN, AND THE THIRD IS THE POINT.
 *   Held    -- the condition is true; the grants apply.
 *   Lapsed  -- the condition WAS true and is not now. The player had this and lost it.
 *   Unknown -- we have not established either. NOT the same as Lapsed.
 *
 * A boolean would collapse Unknown into Lapsed, and that collapse is the dangerous one: a server that
 * has not yet reached the entitlement source would treat every player as freshly lapsed and start
 * locking their builds. The same absent-versus-negative ambiguity that has cost this programme
 * repeatedly -- a parameter reading (0,0,0) could not say "absent", a Type reading SkinColor_Edge
 * could not say "authored". Unknown exists so the system can say "I do not know yet" out loud.
 *
 * THE TWO FAILURE DIRECTIONS ARE DELIBERATELY ASYMMETRIC:
 *   * GRANTS fail CLOSED on Unknown -- never hand out a perk we cannot prove is held.
 *   * PENALTIES fail OPEN on Unknown -- never apply the lapse rule to someone we simply have not
 *     checked. Locking a paying subscriber's builds because a lookup was slow is worse than briefly
 *     withholding a cosmetic, and it is the failure a player would actually notice and resent.
 */
UENUM(BlueprintType)
enum class EAFLConditionState : uint8
{
	/** Never established. Fail CLOSED for grants, fail OPEN for penalties. */
	Unknown  UMETA(DisplayName = "Unknown / not yet checked"),
	/** Condition currently true -- grants apply. */
	Held     UMETA(DisplayName = "Held"),
	/** Condition was true and is no longer -- grants revoked, CC-4.2 lapse rule applies. */
	Lapsed   UMETA(DisplayName = "Lapsed")
};

USTRUCT(BlueprintType)
struct FAFLConditionalGrant
{
	GENERATED_BODY()

	/** What holds the grant, e.g. AFL.Sub.League. NOT a cosmetic id -- a condition can confer many. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Entitlement")
	FName ConditionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Entitlement")
	EAFLConditionState State = EAFLConditionState::Unknown;

	/** Unix seconds when State was last ESTABLISHED -- not when it was last read. Lets a caller decide
	 *  a state is too stale to act on, which a bare bool could never support. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Entitlement")
	int64 StateAsOfUnix = 0;

	/** What this condition confers WHILE Held. Empty is legal: a condition can gate capability
	 *  (creator editing, slot count) rather than confer specific cosmetic ids. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Entitlement")
	TArray<FName> GrantedCosmeticIds;

	bool IsHeld() const { return State == EAFLConditionState::Held; }

	/** True only for a CONFIRMED lapse. Unknown deliberately returns false -- see the asymmetry above. */
	bool IsConfirmedLapsed() const { return State == EAFLConditionState::Lapsed; }
};

/** Alias so the TMap comma cannot split a delegate macro's argument list (the CC-3.3 lesson). */
using FAFLConditionalGrantMap = TMap<FName, FAFLConditionalGrant>;
DECLARE_DELEGATE_TwoParams(FAFLOnConditionalSetLoaded, bool /*bOk*/, const FAFLConditionalGrantMap& /*Grants*/);
/** CC-3.5: the saved-build blob as raw JSON. Kept opaque at this seam on purpose -- the persistence
 *  layer moves the blob, it does not interpret it; only the loadout component knows the shape. */
DECLARE_DELEGATE_TwoParams(FAFLOnCreatorBuildsLoaded, bool /*bFound*/, const FString& /*BuildsJson*/);

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAFLEntitlementSource : public UInterface
{
	GENERATED_BODY()
};

class IAFLEntitlementSource
{
	GENERATED_BODY()

public:
	/** True if Player is entitled to the cosmetic CosmeticId (axis cosmetic). */
	virtual bool IsEntitled(const ALyraPlayerState* Player, FName CosmeticId) const = 0;

	/** True if Player owns the identity (Team/Character) keyed by Id. */
	virtual bool OwnsIdentity(const ALyraPlayerState* Player, EAFLIdentityType Type, FName Id) const = 0;
};

// ---------------------------------------------------------------------------------------------------
// Persistence seam (D8) -- the backing store hides ENTIRELY behind this. Real interface + real data
// model now; stub backing (in-memory / SaveGame) now; PlayFab behind the SAME interface at Phase 3.
// Async-shaped (load returns via delegate) so latency needs no later signature change.
// ---------------------------------------------------------------------------------------------------
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAFLCosmeticPersistence : public UInterface
{
	GENERATED_BODY()
};

class IAFLCosmeticPersistence
{
	GENERATED_BODY()

public:
	/** Load the player's saved selection (async-shaped; stub fires the delegate synchronously). */
	virtual void LoadSelection(const FAFLPlayerId& Player, FAFLOnSelectionLoaded OnLoaded) = 0;

	/** Persist the player's selection (fire-and-forget; the stub writes in-memory / to a SaveGame). */
	virtual void SaveSelection(const FAFLPlayerId& Player, const FAFLCosmeticSelection& Selection) = 0;

	/** Load the player's owned cosmetic-id set (feeds the entitlement gate once wallet lands). */
	virtual void LoadOwnedSet(const FAFLPlayerId& Player, FAFLOnOwnedSetLoaded OnLoaded) = 0;

	// --- S-ECON-WALLET (Fork A): the player's economic state on the SAME seam -------------------------
	/** Persist the player's owned cosmetic-id set (after a purchase grants ownership). */
	virtual void SaveOwnedSet(const FAFLPlayerId& Player, const TArray<FName>& OwnedCosmeticIds) = 0;

	// --- CC-3.3 COUNTED ENTITLEMENT ------------------------------------------------------------------
	// The THIRD entitlement shape. Before this the seam could express exactly two things: a boolean
	// owned-set (TArray<FName> -- do you own it) and counted CURRENCY (int32 Volts/Watts -- a balance,
	// not an entitlement). Neither can say "this player is entitled to N of X". Health packs are NOT a
	// precedent: they ride Lyra inventory, are match-scoped, and never touch this interface.
	//
	// The motivating case is save slots -- a $3 purchase increments a slot count, packs increment by 3
	// and 8 -- but the shape is deliberately generic (FName -> int32), because a counted entitlement is
	// a general capability and hard-coding "slots" here would force a fourth shape for the next one.
	//
	// NOT WIRED TO PLAYFAB. The cache/SaveGame path is complete and authoritative locally; the backend
	// blob is CC-3.4 in the SEPARATE Bag_Man_Backend repo. This is honest scoping, not a stub: the data
	// is real and round-trips, only the remote transport is staged.
	// --- CC-4.1 CONDITIONAL ENTITLEMENT ---------------------------------------------------------
	// Keyed by ConditionId, NOT by cosmetic id: one condition confers many grants, and the state
	// belongs to the condition. Keying by cosmetic would duplicate the state per grant and let two
	// copies of the same subscription's status disagree.
	// --- CC-3.5 SAVED BUILDS, THROUGH THE BACKEND ------------------------------------------------
	// The FIRST cosmetic data on this seam with a real remote store: selection/owned/balance ride
	// PlayFab, and the counted/conditional sets are cache-only pending their own backend work. Builds
	// go to POST /creator-builds (deployed and live-verified, tag cc-3-4-done).
	// TWO IDS, DELIBERATELY. Player keys the local cache; PlayFabId names the REMOTE target and is the
	// caller's server-VERIFIED id (AFLPlayerIdentityComponent::GetResolvedPlayFabId, A1.4) -- exactly as
	// EarnThroughBackend takes it. FAFLPlayerId's backing string is private ON PURPOSE, so deriving the
	// remote target from it would both crack that wrapper open and lose the anti-spoof property that
	// naming the player explicitly provides.
	/** Load the player's saved-build blob. bFound=false for a new player -- NOT an error. */
	virtual void LoadCreatorBuilds(const FAFLPlayerId& Player, const FString& PlayFabId, FAFLOnCreatorBuildsLoaded OnLoaded) = 0;

	/** Persist the player's saved-build blob. Fire-and-forget. */
	virtual void SaveCreatorBuilds(const FAFLPlayerId& Player, const FString& PlayFabId, const FString& BuildsJson) = 0;

	/** Load the player's conditional grants. bOk=false for a new player. */
	virtual void LoadConditionalSet(const FAFLPlayerId& Player, FAFLOnConditionalSetLoaded OnLoaded) = 0;

	/** Persist the player's conditional grants. Fire-and-forget, mirroring SaveOwnedSet. */
	virtual void SaveConditionalSet(const FAFLPlayerId& Player, const FAFLConditionalGrantMap& Grants) = 0;

	/** Load the player's counted entitlements (id -> count). bOk=false for a new player. */
	virtual void LoadCountedSet(const FAFLPlayerId& Player, FAFLOnCountedSetLoaded OnLoaded) = 0;

	/** Persist the player's counted entitlements. Fire-and-forget, mirroring SaveOwnedSet. */
	virtual void SaveCountedSet(const FAFLPlayerId& Player, const FAFLCountedEntitlementMap& Counts) = 0;

	/** Load the player's Volts/Watts balance (async-shaped; stub fires synchronously). bFound=false for a
	 *  new player -> the wallet seeds starting balances. */
	virtual void LoadBalance(const FAFLPlayerId& Player, FAFLOnBalanceLoaded OnLoaded) = 0;

	/** Persist the player's Volts/Watts balance (fire-and-forget; stub writes in-memory / SaveGame). */
	virtual void SaveBalance(const FAFLPlayerId& Player, int32 Volts, int32 Watts) = 0;

	// --- S-ECON WRITE-SIDE (Phase 1): the two authoritative PlayFab TRANSACTIONS on the SAME seam ----------
	// These carry the SERVER-AUTHORITATIVE writes that were formerly inline in the wallet (parity with the
	// already-seamed load side). The impl relocates the transport verbatim; behaviour is unchanged.

	/** Server-authoritative EARN: mirror the committed delta to the player's PlayFab wallet via /earn (A1.3b).
	 *  PlayFabId = the earner's server-VERIFIED id (A1.4 GetResolvedPlayFabId) -- NOT the login key (which is the
	 *  server's OWN id on a dedicated host); naming the target player is what preserves the A1.4 anti-spoof.
	 *  Async; OnComplete carries (bOk, the raw /earn response body). */
	virtual void EarnThroughBackend(const FString& PlayFabId, const FString& CurrencyCode, int32 Amount,
		const FString& Reason, const FString& MatchId, FAFLOnEarnComplete OnComplete) = 0;

	/** Server-authoritative PURCHASE: PlayFab Client/PurchaseItem spends+grants (the anti-spoof wall; can REJECT).
	 *  NO player id -- PurchaseItem is auth-token'd, so the client's login token IS the identity. Async;
	 *  OnComplete is accept(true)/reject(false). */
	virtual void PurchaseThroughBackend(FName CosmeticId, const FString& CurrencyCode, int32 Price,
		FAFLOnPurchaseComplete OnComplete) = 0;
};
