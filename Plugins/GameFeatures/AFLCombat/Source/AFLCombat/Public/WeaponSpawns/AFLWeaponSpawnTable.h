// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "AFLWeaponSpawnTable.generated.h"

class ULyraWeaponPickupDefinition;

/**
 * One designer-intent tag -> the weapon pickup it spawns.
 */
USTRUCT(BlueprintType)
struct FAFLWeaponSpawnEntry
{
	GENERATED_BODY()

	/** The tag an AAFLWeaponSpawner carries in its WeaponIntent (e.g. AFL.Spawner.Weapon.Railgun). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Spawner", meta = (Categories = "AFL.Spawner"))
	FGameplayTag IntentTag;

	/** The weapon pickup granted + displayed when a marker carries IntentTag (bundles item/mesh/cooldown/FX). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Spawner")
	TObjectPtr<ULyraWeaponPickupDefinition> PickupData;
};

/**
 * The AFL weapon-spawn mapping table -- DATA, not code.
 *
 * Add or rebalance which weapon each intent tag grants by editing this asset; no C++ change. The table
 * ASSET lives in AFLBagMan content (AFL->AFL: it references AFLBagMan's own WeaponPickupData) while the
 * TYPE lives here in AFLCombat so the GameFeature action (UAFLGFA_WeaponSpawns) can read it.
 *
 * Tier/role POOLS are a planned extension (a second array of tier-tag -> weighted list, letting a marker
 * request a balance tier instead of a specific weapon). Intentionally NOT built yet -- the precise-entry
 * shape below is forward-compatible with adding that array alongside it.
 */
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLWeaponSpawnTable : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Precise intent-tag -> weapon mappings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Spawner", meta = (TitleProperty = "IntentTag"))
	TArray<FAFLWeaponSpawnEntry> Entries;
};
