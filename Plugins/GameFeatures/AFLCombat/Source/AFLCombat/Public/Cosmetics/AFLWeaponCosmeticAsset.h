// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "Equipment/LyraEquipmentDefinition.h"   // FULL type: TSoftClassPtr + LoadSynchronous need the complete class

#include "AFLWeaponCosmeticAsset.generated.h"

/**
 * UAFLWeaponCosmeticAsset -- the catalog target for a Weapon SKU (S-ECON-CAT, weapon-equip path). #43.
 *
 * A Weapon catalog entry's Asset (TSoftObjectPtr<UPrimaryDataAsset>) resolves to one of these. It names the
 * Lyra EQUIPMENT DEFINITION class the SKU equips (e.g. WID_AFL_Arclight, a ULyraEquipmentDefinition subclass).
 * Why a tiny carrier rather than the catalog pointing at the equipment def directly: a ULyraEquipmentDefinition
 * is a UClass, NOT a UPrimaryDataAsset, so the uniform catalog Asset field (TSoftObjectPtr<UPrimaryDataAsset>)
 * can't reference it -- the exact reason UAFLAbilityCosmeticAsset exists for the ability-grant path (D1: mirror
 * that proven carrier). This keeps a catalog row meaning "a cosmetic", and keeps catalog resolution UNIFORM
 * (ResolveAsset -> a UPrimaryDataAsset, cast to the concrete carrier -- NO per-type branch in the resolver).
 *
 * HOMED IN AFLCombat (not AFLCosmeticCore, where the ability carrier lives): it references a LyraGame type
 * (ULyraEquipmentDefinition), and AFLCosmeticCore is a deliberately-thin early-load module that must NOT depend
 * on the game modules (it loads at the AssetManager PrimaryAsset scan on frame 0). AFLCombat already depends on
 * LyraGame and already owns the other game-typed cosmetic asset (UAFLSkinColorAsset), so this carrier sits with
 * its kin + its consumer. The catalog stays decoupled: it resolves this via a SOFT UPrimaryDataAsset ref (the
 * same load-on-demand contract the .uplugin documents), so no early-load dependency is introduced.
 */
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLWeaponCosmeticAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The Lyra equipment definition this weapon SKU equips (SOFT -> loads on demand). For Arclight this is
	 *  WID_AFL_Arclight. The WeaponId consumer resolves THIS -> EquipItem(EquipmentDefinition) on the pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|WeaponCosmetic")
	TSoftClassPtr<ULyraEquipmentDefinition> EquipmentDefinition;
};
