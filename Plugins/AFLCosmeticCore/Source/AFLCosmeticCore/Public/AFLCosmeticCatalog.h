// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AFLCosmeticCoreTypes.h"
#include "Engine/DataAsset.h"

#include "AFLCosmeticCatalog.generated.h"

/**
 * UAFLCosmeticCatalog — the ONE curated cosmetic manifest (S-ECON-CAT). One global asset
 * (DA_AFL_CosmeticCatalog); every economy system reads it (the #43 selection resolve, the store, the
 * wallet entitlement, the 3D showroom). "One source, many readers": adding a cosmetic = adding one
 * Entries row + the asset in a scanned dir -> it appears everywhere with zero code/level edits.
 *
 * Lives in the always-loaded AFLCosmeticCore module (NOT the AFLCombat GameFeature): it registers as an
 * AssetManager primary asset, and AssetManager scans PrimaryAssetTypesToScan at engine startup (frame 0)
 * before any GameFeature loads -- a catalog class in a GameFeature fails to load at scan time and the
 * type is rejected. Its TSoftObjectPtr entry refs keep the referenced cosmetics in the cook set. Access
 * is through UAFLCosmeticCatalogSubsystem (which builds the CosmeticId->entry accelerator once).
 */
UCLASS(BlueprintType)
class AFLCOSMETICCORE_API UAFLCosmeticCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The curated manifest, authored in the details panel (DA_AFL_CosmeticCatalog). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog", meta = (TitleProperty = "{Type} {CosmeticId}"))
	TArray<FAFLCatalogEntry> Entries;

	/** Direct typed lookup -- ZERO reflection at the read site. Null if the id isn't in the manifest. */
	const FAFLCatalogEntry* FindEntry(FName CosmeticId) const;

	/** (Re)build the CosmeticId -> entry accelerator from Entries. Idempotent; called after load + on
	 *  PostLoad so an editor edit to Entries stays consistent. */
	void BuildLookup();

	/** AssetManager primary-asset id -- fixed type "AFLCosmeticCatalog" so the subsystem can resolve it. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("AFLCosmeticCatalog"), GetFName());
	}

	//~UObject
	virtual void PostLoad() override;
	//~End

private:
	/** Transient accelerator built from Entries (not serialized). Points into the Entries array. */
	TMap<FName, const FAFLCatalogEntry*> EntryById;
};
