// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AFLCosmeticCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "AFLCosmeticCatalogSubsystem.generated.h"

struct FAFLCatalogEntry;
class UAFLCosmeticCatalog;
class UPrimaryDataAsset;

/**
 * UAFLCosmeticCatalogSubsystem — the single access layer for the cosmetic catalog (S-ECON-CAT).
 *
 * Loads the one DA_AFL_CosmeticCatalog (resolved via AssetManager primary-asset lookup, type
 * "AFLCosmeticCatalog") once and serves every reader: the #43 selection resolve, the store, the wallet
 * entitlement, and the 3D showroom. GameInstance-scoped -> one instance for the whole session, available
 * on server and client. This is what replaces #43's ResolveEdgeById/BrandEdgeMap-scan stopgap.
 *
 * Lives in AFLCosmeticCore (always-loaded) -- both so the catalog class is loadable at the engine-startup
 * AssetManager scan AND so the registry is reachable from any module (AFLCombat depends on this for the
 * #43 swap). Resolution is by the immutable FName CosmeticId; cosmetic ASSETS are soft-referenced and
 * loaded on demand via ResolveAsset (synchronous load -> already-cooked, tiny per-cosmetic).
 */
UCLASS()
class AFLCOSMETICCORE_API UAFLCosmeticCatalogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End

	/** The manifest row for a cosmetic id, or null if absent. The cheap, no-load lookup. */
	const FAFLCatalogEntry* FindEntry(FName CosmeticId) const;

	/** Resolve + LOAD the cosmetic asset for an id (synchronous; the asset is cooked + small). Null on
	 *  miss or unset asset. Callers cast to the concrete type per Type (UAFLSkinColorAsset for skin axes). */
	UPrimaryDataAsset* ResolveAsset(FName CosmeticId) const;

	/** Gather every entry of a given kind (the store / showroom iterate this to render their rows). */
	void GetEntriesByType(EAFLCosmeticType Type, TArray<const FAFLCatalogEntry*>& OutEntries) const;

	/** True once the catalog asset is loaded + ready. */
	bool IsReady() const { return Catalog != nullptr; }

	/** Static convenience: resolve the subsystem from any WorldContext (null-safe). */
	static UAFLCosmeticCatalogSubsystem* Get(const UObject* WorldContext);

private:
	/** Find + load the one catalog asset via AssetManager (primary-asset type "AFLCosmeticCatalog"). */
	void LoadCatalog();

	/** The loaded manifest (hard ref -> stays resident for the session). */
	UPROPERTY()
	TObjectPtr<UAFLCosmeticCatalog> Catalog = nullptr;
};
