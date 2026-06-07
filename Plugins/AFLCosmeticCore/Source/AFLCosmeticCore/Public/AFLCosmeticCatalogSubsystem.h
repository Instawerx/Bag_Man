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

	/** BP-callable resolve (S-ECON-CAT): a Blueprint cosmetic consumer calls this to resolve a CosmeticId to
	 *  its cosmetic asset (then casts to the concrete type it expects). Same body as ResolveAsset, exposed to
	 *  BP. (Originally added for the now-retired helmet part-path BP; kept as a generic BP resolve entry.) */
	UFUNCTION(BlueprintCallable, Category = "AFL|Cosmetics", meta = (DisplayName = "Resolve Cosmetic Asset"))
	UPrimaryDataAsset* ResolveCosmeticAsset(FName CosmeticId) const { return ResolveAsset(CosmeticId); }

	/** Gather every entry of a given kind (the store / showroom iterate this to render their rows). */
	void GetEntriesByType(EAFLCosmeticType Type, TArray<const FAFLCatalogEntry*>& OutEntries) const;

	/**
	 * S-ECON-STORE (a) feed: every PURCHASABLE catalog entry (Acquisition != GrantedFree -> identity/free base
	 * excluded; those aren't shop items). Returned BY VALUE (FAFLCatalogEntry is a small BlueprintType struct:
	 * id + price + tier + displayname) so the store ListView/grid can read id/price/tier/name with no UObject-
	 * wrapper ceremony for the MVP. BlueprintCallable -- the store widget's grid feed. Pure read, no economy logic. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Cosmetics")
	void GetPurchasableEntries(TArray<FAFLCatalogEntry>& OutEntries) const;

	/** BP-callable single-entry lookup (the store reads price/tier/owned-affordable per id). Returns false on miss. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Cosmetics")
	bool GetEntry(FName CosmeticId, FAFLCatalogEntry& OutEntry) const;

	/**
	 * S-ECON-STORE (Fix 1) price-display formatter: the player-facing price string for a catalog entry, in the
	 * ACTUAL currency/currencies the entry charges -- so a SPARK (pay-either) item reads "10,000 V / 100,000 W",
	 * a Volts-only tier reads "16,000 V", a Watts-only item reads "100,000 W". Grouped with thousands separators.
	 * GrantedFree/zero-price reads "Free". Static + BlueprintPure so the tile formats with one node off the Entry
	 * struct -- the display SSOT for currency, so a tile can never show the wrong currency again (the Fix-1 bug).
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	static FText GetEntryPriceText(const FAFLCatalogEntry& Entry);

	/**
	 * The entry's rarity as a gameplay tag (the skill's canonical rarity signal). Returns Entry.RarityTag if
	 * the designer set it; otherwise FALLS BACK to mapping the EAFLCosmeticRarity enum -> Cosmetic.Rarity.<X>
	 * so a card always has a rarity tag without authoring both. One source for the frame-color logic.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	static FGameplayTag ResolveRarityTag(const FAFLCatalogEntry& Entry);

	/**
	 * The IRONICS cyber-palette frame color for an entry's rarity (the skill: rarity drives the shop card
	 * frame). Common=steel, Uncommon=green, Rare=cyan, Epic=magenta, Legendary=gold. The tile binds its
	 * RarityFrame tint to this; the details panel tints its rarity label with it. One color SSOT.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	static FLinearColor GetRarityColor(const FAFLCatalogEntry& Entry);

	/** Rarity as a short display string ("LEGENDARY") for the details-panel rarity row + the tile badge. */
	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	static FText GetRarityText(const FAFLCatalogEntry& Entry);

	/** True once the catalog asset is loaded + ready. */
	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	bool IsReady() const { return Catalog != nullptr; }

	/** Static convenience: resolve the subsystem from any WorldContext (null-safe). BlueprintPure so the
	 *  store widget can grab the catalog with one node (WorldContext auto-filled). */
	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics", meta = (WorldContext = "WorldContext"))
	static UAFLCosmeticCatalogSubsystem* Get(const UObject* WorldContext);

private:
	/** Find + load the one catalog asset via AssetManager (primary-asset type "AFLCosmeticCatalog"). */
	void LoadCatalog();

	/** The loaded manifest (hard ref -> stays resident for the session). */
	UPROPERTY()
	TObjectPtr<UAFLCosmeticCatalog> Catalog = nullptr;
};
