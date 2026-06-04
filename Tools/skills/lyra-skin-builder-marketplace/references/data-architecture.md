# Data Architecture Reference

Phase 6 of the pipeline. How to define, catalog, store, and stream the skin
catalog so it scales from 10 cosmetics to 10,000 without breaking memory
budgets or cook times.

---

## The Core Tradeoff: Data Asset vs DataTable

UE5 offers two patterns for catalog data. Choose based on volume and editing
needs.

```
PATTERN                        | When to use                              | Scales to
───────────────────────────────┼──────────────────────────────────────────┼──────────
Primary Data Asset per item    | < 500 items, designer-friendly per-asset | ~1,000
DataTable with row struct      | > 500 items, bulk editing in spreadsheet | 10,000+
Both (hybrid)                  | Mix — DataTable references DataAssets    | unlimited
```

For a typical Lyra cosmetic shop with 50–500 items at launch growing to
2,000+ over live ops, **the hybrid pattern is correct**: each cosmetic is a
PrimaryDataAsset (designer-editable, soft-ref friendly), and a DataTable
indexes them all for fast queries.

---

## Skin Definition Struct — F<Project>SkinDefinition

The runtime-facing struct that the rest of the cosmetic system consumes.
Lives in a USTRUCT for use in C++, BP, replication, and DataTables.

```cpp
USTRUCT(BlueprintType)
struct <PROJECT>GAME_API F<Project>SkinDefinition
{
    GENERATED_BODY()

    /** Stable identifier used as the asset key — never localized */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FName SkinId;

    /** Designer-facing display name (localized) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (MultiLine = true))
    FText Description;

    /** Shop thumbnail */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
    TSoftObjectPtr<UTexture2D> ShopThumbnail;

    /** Hero card / preview render */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
    TSoftObjectPtr<UTexture2D> HeroImage;

    /** The body mesh for this skin (L2 mesh-swap skin uses this) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    /** Mobile variant — null falls back to SkeletalMesh */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TSoftObjectPtr<USkeletalMesh> SkeletalMeshMobile;

    /** Material instances per slot (parallel to mesh material slots) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TArray<TSoftObjectPtr<UMaterialInterface>> MaterialInstances;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TArray<TSoftObjectPtr<UMaterialInterface>> MaterialInstancesMobile;

    /** L4 — modular parts that make up this skin (if any) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Modular")
    TArray<TSoftObjectPtr<U<Project>CharacterPartDefinition>> Parts;

    /** Rarity tier — drives shop visuals + base pricing */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
    FGameplayTag RarityTag;

    /** Currency type and cost — soft ref so different currencies plug in */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
    FGameplayTag CurrencyTag; // Currency.SoftCoin / Currency.PremiumGem / etc.

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
    int32 BaseCost = 0;

    /** Tags this skin applies (cosmetic.outfit.heavy, etc.) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTagContainer SkinTags;

    /** Owning GameFeature plugin — empty for base game skins */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LiveOps")
    FString OwningGameFeaturePlugin;

    /** Season identifier — drives "available this season" filters */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LiveOps")
    FGameplayTag SeasonTag;

    /** ISO date in UTC after which this skin can't be purchased (vault) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LiveOps")
    FString AvailableUntilUtc;
};
```

This struct is what `ApplySkin` consumes (see materials-variants.md). The
hybrid pattern wraps it in a PrimaryDataAsset:

```cpp
UCLASS(BlueprintType)
class <PROJECT>GAME_API U<Project>SkinAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
    F<Project>SkinDefinition Definition;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(
            FPrimaryAssetType(TEXT("<Project>Skin")),
            Definition.SkinId);
    }
};
```

---

## DataTable Index — DT_<Project>SkinCatalog

For the shop UI to display 500+ skins without instantiating every PrimaryDataAsset,
ship a DataTable that lists them by soft ref. The DataTable rows are tiny;
actual skin data loads only when previewed/equipped.

```cpp
USTRUCT(BlueprintType)
struct <PROJECT>GAME_API F<Project>SkinCatalogRow : public FTableRowBase
{
    GENERATED_BODY()

    /** Soft ref to the actual skin asset */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSoftObjectPtr<U<Project>SkinAsset> SkinAsset;

    /** Duplicated for fast filtering without loading the asset */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FName SkinId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTag RarityTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTag CurrencyTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 BaseCost = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTag SeasonTag;

    /** Soft ref to thumbnail — small, cheap to load for grid display */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> ShopThumbnail;

    /** GameFeature plugin owning this skin (empty for base game) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FString OwningGameFeaturePlugin;
};
```

The shop UI loads the DataTable once, displays the grid with thumbnails (soft-
loaded), and only loads the full PrimaryDataAsset when the player previews or
purchases.

---

## Catalog Subsystem — U<Project>CosmeticCatalogSubsystem

A `UGameInstanceSubsystem` centralizing catalog queries. Single entry point for
the shop UI, the loadout screen, and the entitlement system.

```cpp
UCLASS()
class <PROJECT>GAME_API U<Project>CosmeticCatalogSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Lookup a catalog row by SkinId (fast, no asset load) */
    UFUNCTION(BlueprintCallable, Category = "Cosmetics")
    bool GetCatalogRow(FName SkinId, F<Project>SkinCatalogRow& OutRow) const;

    /** All skins of a given rarity */
    UFUNCTION(BlueprintCallable, Category = "Cosmetics")
    TArray<F<Project>SkinCatalogRow> GetSkinsByRarity(FGameplayTag RarityTag) const;

    /** All skins in a season */
    UFUNCTION(BlueprintCallable, Category = "Cosmetics")
    TArray<F<Project>SkinCatalogRow> GetSkinsBySeason(FGameplayTag SeasonTag) const;

    /** All skins originating from a specific GameFeature plugin */
    UFUNCTION(BlueprintCallable, Category = "Cosmetics")
    TArray<F<Project>SkinCatalogRow> GetSkinsByPlugin(const FString& PluginName) const;

    /** Async load a full skin asset (for preview / equip) */
    void LoadSkinAssetAsync(
        FName SkinId,
        TFunction<void(U<Project>SkinAsset*)> OnLoaded);

    /** Get currently loaded skin asset (sync — returns null if not loaded) */
    UFUNCTION(BlueprintCallable, Category = "Cosmetics")
    U<Project>SkinAsset* GetLoadedSkinAsset(FName SkinId) const;

    /** Register a GameFeature plugin's catalog rows (called during plugin activation) */
    void RegisterPluginCatalogRows(
        const FString& PluginName,
        UDataTable* CatalogChunk);

    /** Unregister when a GameFeature plugin deactivates */
    void UnregisterPluginCatalogRows(const FString& PluginName);

private:
    /** Base game catalog DataTable — loaded once */
    UPROPERTY()
    TObjectPtr<UDataTable> BaseCatalog;

    /** Live runtime view of all catalog rows (base + active plugins) */
    UPROPERTY()
    TMap<FName, F<Project>SkinCatalogRow> AllRows;

    /** Currently loaded full skin assets, keyed by SkinId */
    UPROPERTY()
    TMap<FName, TObjectPtr<U<Project>SkinAsset>> LoadedSkinAssets;

    /** Active streamable handles so loads don't get GC'd */
    TMap<FName, TSharedPtr<FStreamableHandle>> ActiveLoadHandles;

    void RebuildRowMap();
};
```

Implementation highlights:

```cpp
void U<Project>CosmeticCatalogSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Load the base catalog from a known path defined in DefaultGame.ini
    const FSoftObjectPath BaseCatalogPath = FSoftObjectPath(
        TEXT("/Game/Cosmetics/Data/DT_<Project>SkinCatalog.DT_<Project>SkinCatalog"));

    BaseCatalog = Cast<UDataTable>(BaseCatalogPath.TryLoad());
    if (IsValid(BaseCatalog))
    {
        RebuildRowMap();
    }

    // Subscribe to GameFeature plugin state changes to refresh catalog when
    // a plugin activates / deactivates
    if (UGameFeaturesSubsystem* GFSubsystem =
        UGameFeaturesSubsystem::GetForWorld(GetWorld()))
    {
        // Wire up plugin state observer here (Phase 8 reference)
    }
}

void U<Project>CosmeticCatalogSubsystem::LoadSkinAssetAsync(
    FName SkinId,
    TFunction<void(U<Project>SkinAsset*)> OnLoaded)
{
    // Already loaded?
    if (U<Project>SkinAsset* Cached = GetLoadedSkinAsset(SkinId))
    {
        OnLoaded(Cached);
        return;
    }

    F<Project>SkinCatalogRow Row;
    if (!GetCatalogRow(SkinId, Row))
    {
        UE_LOG(LogCosmetics, Warning,
            TEXT("LoadSkinAssetAsync: unknown skin %s"), *SkinId.ToString());
        OnLoaded(nullptr);
        return;
    }

    FStreamableManager& StreamMgr = UAssetManager::GetStreamableManager();
    TSharedPtr<FStreamableHandle> Handle = StreamMgr.RequestAsyncLoad(
        Row.SkinAsset.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda(
            [this, SkinId, Row, OnLoaded]()
            {
                U<Project>SkinAsset* Asset = Row.SkinAsset.Get();
                if (IsValid(Asset))
                {
                    LoadedSkinAssets.Add(SkinId, Asset);
                }
                OnLoaded(Asset);
            }));

    if (Handle.IsValid())
    {
        ActiveLoadHandles.Add(SkinId, Handle);
    }
}
```

---

## AssetManager Configuration

For PrimaryDataAssets to stream correctly, register them in
`DefaultGame.ini`. Without this, cooked builds won't include the skin assets
or won't be able to find them at runtime.

```ini
[/Script/Engine.AssetManagerSettings]

+PrimaryAssetTypesToScan=(PrimaryAssetType="<Project>Skin",
    AssetBaseClass=/Script/<Project>Game.<Project>SkinAsset,
    bHasBlueprintClasses=False,
    bIsEditorOnly=False,
    Directories=((Path="/Game/Cosmetics/Skins")),
    SpecificAssets=,
    Rules=(Priority=-1, ChunkId=-1, bApplyRecursively=True, CookRule=AlwaysCook))

+PrimaryAssetTypesToScan=(PrimaryAssetType="<Project>CharacterPart",
    AssetBaseClass=/Script/<Project>Game.<Project>CharacterPartDefinition,
    bHasBlueprintClasses=False,
    bIsEditorOnly=False,
    Directories=((Path="/Game/Cosmetics/Parts")),
    SpecificAssets=,
    Rules=(Priority=-1, ChunkId=-1, bApplyRecursively=True, CookRule=AlwaysCook))
```

**Critical mobile / chunked-download note**: if shipping live-ops chunks via
the PSO / pak chunk system, set `ChunkId` per asset type and configure
`AssetManager` chunking rules. For day-one base catalog, `ChunkId=-1` (always
in main pak) is correct.

---

## Soft vs Hard References — Discipline

Every reference inside the catalog flows through `TSoftObjectPtr` or
`TSoftClassPtr`. Hard refs in DataTable rows or skin definitions defeat the
async loading model and blow up memory.

```
✓ TSoftObjectPtr<USkeletalMesh>  in the row / skin asset
✓ TSoftClassPtr<AActor>          for part actor classes
✓ TSoftObjectPtr<UTexture2D>     for shop thumbnails
✓ Async load via FStreamableManager when actually needed

✗ TObjectPtr<USkeletalMesh>      — hard ref, asset loaded at row load
✗ Direct asset reference in Blueprints to skin assets — hard ref via BP
✗ "Load all skins at boot for fast shop UI" — uses gigabytes of RAM
```

For thumbnails specifically: keep them in a separate `T_<SkinName>_Thumb`
texture, low resolution (256–512), ASTC for mobile. Soft-load on grid
display. The hero card / large preview image loads when the player taps in.

---

## Save Data Schema

Player's equipped loadout and entitlements persist via a `ULyraSaveGame`
extension or your own save subsystem. Use stable `FName SkinId`s, not asset
references — survives asset path changes.

```cpp
USTRUCT(BlueprintType)
struct F<Project>CosmeticLoadoutSave
{
    GENERATED_BODY()

    /** Active full-body skin (L2/L5 systems) */
    UPROPERTY()
    FName ActiveSkinId;

    /** Active modular parts per slot (L4 systems) */
    UPROPERTY()
    TMap<FGameplayTag, FName> EquippedPartsBySlot;

    /** All owned cosmetics — fallback if server entitlement check is slow */
    UPROPERTY()
    TArray<FName> OwnedCosmeticIds;

    /** Schema version for forward-compat migration */
    UPROPERTY()
    int32 SchemaVersion = 1;
};
```

Never store the full skin asset path or asset reference in save data —
asset paths change, IDs shouldn't.

---

## Catalog Filtering for Shop UI

Common query patterns the shop UI needs:

```cpp
// "Show me featured legendary skins in the current season"
TArray<F<Project>SkinCatalogRow> Featured =
    CatalogSubsystem->GetSkinsBySeason(CurrentSeasonTag)
    .FilterByPredicate([](const F<Project>SkinCatalogRow& Row)
    {
        return Row.RarityTag == FGameplayTag::RequestGameplayTag(
            TEXT("Cosmetic.Rarity.Legendary"));
    });

// "Show me everything I can afford with my current premium currency balance"
const int32 Balance = WalletSubsystem->GetBalance(PremiumCurrencyTag);
TArray<F<Project>SkinCatalogRow> Affordable =
    CatalogSubsystem->GetAllRows()
    .FilterByPredicate([Balance, PremiumCurrencyTag](
        const F<Project>SkinCatalogRow& Row)
    {
        return Row.CurrencyTag == PremiumCurrencyTag &&
               Row.BaseCost <= Balance;
    });
```

For shops with thousands of items, index the rows by rarity / season / cost
brackets at subsystem init so filters are O(1) lookups, not O(n) scans.

---

## Verification Checklist — End of Phase 6

```
□ F<Project>SkinDefinition struct authored with soft refs
□ U<Project>SkinAsset class compiles, PrimaryAssetId returned correctly
□ DT_<Project>SkinCatalog DataTable created with F<Project>SkinCatalogRow
□ U<Project>CosmeticCatalogSubsystem builds and rebuilds row map on init
□ Async load round-trip works: request by id → asset loaded → callback fires
□ AssetManager settings in DefaultGame.ini register both PrimaryAsset types
□ Cooked build includes all skin assets (verify via Project Launcher cook log)
□ Soft thumbnails load on shop grid scroll without hitches (mobile tested)
□ Save / load roundtrip preserves equipped loadout by SkinId
□ Filtering by rarity / season / cost works in editor preview
```
