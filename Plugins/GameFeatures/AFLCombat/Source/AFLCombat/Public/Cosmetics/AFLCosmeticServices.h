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
// S-ECON-WALLET (Fork A): balance rides the SAME persistence seam (one interface for all of a player's
// economic state -- selection + owned-set + balance -- behind one PlayFab-ready store). Async-shaped like
// the others. Volts + Watts are INTEGER (peg discipline; IRONICS economy LOCKED). bFound=false on a new
// player -> the wallet seeds defaults.
DECLARE_DELEGATE_ThreeParams(FAFLOnBalanceLoaded, bool /*bFound*/, int32 /*Volts*/, int32 /*Watts*/);

// ---------------------------------------------------------------------------------------------------
// Entitlement seam -- "does this player own this cosmetic?" The gate ASKS this; it does not implement
// policy. Permissive impl now (everyone owns the basics); S-ECON-WALLET implements it against the
// owned-set later. ALWAYS CALLED, so the call site is proven from day one.
// ---------------------------------------------------------------------------------------------------
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

	/** Load the player's Volts/Watts balance (async-shaped; stub fires synchronously). bFound=false for a
	 *  new player -> the wallet seeds starting balances. */
	virtual void LoadBalance(const FAFLPlayerId& Player, FAFLOnBalanceLoaded OnLoaded) = 0;

	/** Persist the player's Volts/Watts balance (fire-and-forget; stub writes in-memory / SaveGame). */
	virtual void SaveBalance(const FAFLPlayerId& Player, int32 Volts, int32 Watts) = 0;
};
