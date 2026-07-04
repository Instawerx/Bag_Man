// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"

#include "Cosmetics/AFLCosmeticServices.h"        // IAFLCosmeticPersistence, FAFLPlayerId, the load delegates
#include "Cosmetics/AFLCosmeticSelectionTypes.h"  // FAFLCosmeticSelection

#include "AFLEconomyPersistenceSubsystem.generated.h"

/**
 * FAFLEconomyRecord -- ONE player's persisted economic state on the seam: balance (Volts/Watts) +
 * the owned-cosmetic set + the last cosmetic selection. The bHas* flags carry the async contract's
 * bFound/bOk discrimination (a brand-new player has a record only once something is saved -> the
 * consumers seed defaults on a miss). INTEGER Volts/Watts (peg discipline; IRONICS economy LOCKED).
 */
USTRUCT()
struct FAFLEconomyRecord
{
	GENERATED_BODY()

	UPROPERTY() int32 Volts = 0;
	UPROPERTY() int32 Watts = 0;
	UPROPERTY() bool  bHasBalance = false;

	UPROPERTY() TArray<FName> OwnedCosmeticIds;

	UPROPERTY() FAFLCosmeticSelection Selection;
	UPROPERTY() bool  bHasSelection = false;
};

/**
 * UAFLEconomySaveGame -- the on-disk container written to / read from a single SaveGame slot. Keyed by
 * the opaque FAFLPlayerId so multiple local accounts (future) never collide; A0 in practice holds one
 * local record (see the ForceLocalSlot cvar). The FAFLPlayerId's backing is a private UPROPERTY -> it
 * serializes through reflection without exposing the string to any call site (the opacity holds).
 */
UCLASS()
class UAFLEconomySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Bumped if the record layout changes; a load can migrate/ignore an older version. */
	UPROPERTY() int32 SaveVersion = 1;

	/** Per-player economic state. */
	UPROPERTY() TMap<FAFLPlayerId, FAFLEconomyRecord> Records;
};

/**
 * UAFLEconomyPersistenceSubsystem -- PHASE A0: the FIRST real implementation of IAFLCosmeticPersistence.
 *
 * Backing = a LOCAL SaveGame (SaveGameToSlot / LoadGameFromSlot). This makes ownership + wallet + the
 * cosmetic selection SURVIVE A SESSION BOUNDARY (buy -> stop PIE -> start PIE -> still owned), which the
 * nullptr stub cannot do. It is DELIBERATELY NOT anti-spoof -- a local .sav is a file the player can edit.
 * Server-authoritative anti-spoof is PHASE A1 (the Bag_Man_Backend Lambda tier: client calls REST with
 * its login token, the Lambda validates funds + grants via PlayFab server-side, the secret stays in
 * Secrets Manager -- never on the client). A1 swaps THIS backing behind the SAME interface with no
 * call-site change, and A0 becomes A1's OFFLINE / last-known-good cache.
 *
 * WITHIN LYRA: a UGameInstanceSubsystem -- one per game instance, GC-rooted, resolved the same way the
 * wallet resolves the catalog (a static Get). It lives in AFLCombat alongside its only consumers (the
 * wallet + loadout components); because those consumers are in this same module, they can only run AFTER
 * this module loads, by which point FSubsystemModuleWatcher has already created this subsystem -> the
 * GetPersistence() swap point never sees a half-loaded module.
 */
UCLASS()
class AFLCOMBAT_API UAFLEconomyPersistenceSubsystem : public UGameInstanceSubsystem, public IAFLCosmeticPersistence
{
	GENERATED_BODY()

public:
	/** Resolve the subsystem from any world-context object (mirrors UAFLCosmeticCatalogSubsystem::Get).
	 *  Returns null before the game instance exists -> callers fall back to the no-op path. */
	static UAFLEconomyPersistenceSubsystem* Get(const UObject* WorldContext);

	//~ USubsystem ------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ IAFLCosmeticPersistence (the 6 seam methods; stub contract fires the load delegates synchronously)
	virtual void LoadSelection(const FAFLPlayerId& Player, FAFLOnSelectionLoaded OnLoaded) override;
	virtual void SaveSelection(const FAFLPlayerId& Player, const FAFLCosmeticSelection& Selection) override;
	virtual void LoadOwnedSet(const FAFLPlayerId& Player, FAFLOnOwnedSetLoaded OnLoaded) override;
	virtual void SaveOwnedSet(const FAFLPlayerId& Player, const TArray<FName>& OwnedCosmeticIds) override;
	virtual void LoadBalance(const FAFLPlayerId& Player, FAFLOnBalanceLoaded OnLoaded) override;
	virtual void SaveBalance(const FAFLPlayerId& Player, int32 Volts, int32 Watts) override;

private:
	/** In-memory mirror of the on-disk slot. UPROPERTY -> GC-rooted by the subsystem. */
	UPROPERTY()
	TObjectPtr<UAFLEconomySaveGame> SaveData;

	/** Load the slot if present, else create a fresh SaveGame object. Idempotent. */
	void EnsureLoaded();

	/** Write the current SaveData to the disk slot (synchronous; the data is tiny; earn/buy are rare). */
	void Flush() const;

	/** A0 key normalization. PIE net-ids are ephemeral, which would break a deterministic logout/login
	 *  proof -> afl.Econ.ForceLocalSlot (default 1) collapses every key to one stable local slot. A1 sets
	 *  it 0 once PlayFab login provides a real, stable account id (then keying is per-account). Also maps an
	 *  invalid id to the local key regardless, so a null net-id never keys an unreachable record. */
	FAFLPlayerId ResolveKey(const FAFLPlayerId& In) const;

	/** Find-or-add the record for the normalized player key (ensures SaveData first). */
	FAFLEconomyRecord& RecordFor(const FAFLPlayerId& Player);
};
