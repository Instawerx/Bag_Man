// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"

#include "AFLWeaponSpawnRegistry.generated.h"

class ULyraWeaponPickupDefinition;

/** Fired when the registry's mappings change (installed on activation / removed on deactivation). */
DECLARE_MULTICAST_DELEGATE(FAFLWeaponSpawnRegistryEvent);

/**
 * Per-world tag -> weapon-pickup-definition registry for the AFL GameFeature-injected spawner system.
 *
 * AFL-content-AGNOSTIC on purpose: it lives in AFLGameCore (always-loaded) and holds only engine/LyraGame
 * types (FGameplayTag -> ULyraWeaponPickupDefinition). The AFL GameFeature (AFLCombat's
 * UAFLGFA_WeaponSpawns) builds the mapping from its own AFL table and hands it here via RegisterMappings on
 * activation; markers (AAFLWeaponSpawner) read it via Resolve. This keeps the boundary clean -- the
 * always-loaded layer never names an AFLBagMan/AFLCombat asset type; the GameFeature pushes DOWN into it.
 *
 * One instance per UWorld (server and each client), so resolution is deterministic-local on every endpoint.
 */
UCLASS()
class AFLGAMECORE_API UAFLWeaponSpawnRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Broadcast when mappings become available (GameFeature activated). Markers that were waiting resolve. */
	FAFLWeaponSpawnRegistryEvent OnReady;

	/** Broadcast when mappings are removed (GameFeature deactivated). Markers revert to inert. */
	FAFLWeaponSpawnRegistryEvent OnCleared;

	/** Install the tag->def table (GameFeature activation). Sets active + broadcasts OnReady. */
	void RegisterMappings(const TMap<FGameplayTag, TObjectPtr<ULyraWeaponPickupDefinition>>& InMappings);

	/** Remove the table (GameFeature deactivation). Sets inactive + broadcasts OnCleared. */
	void Clear();

	/** True once a table is installed. */
	bool IsActive() const { return bActive; }

	/** Resolve a designer intent tag to a weapon-pickup definition, or null if unmapped. */
	ULyraWeaponPickupDefinition* Resolve(const FGameplayTag& IntentTag) const;

	//~ Begin USubsystem interface -- only game/PIE worlds carry weapon spawners.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End USubsystem interface

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<ULyraWeaponPickupDefinition>> Mappings;

	bool bActive = false;
};
