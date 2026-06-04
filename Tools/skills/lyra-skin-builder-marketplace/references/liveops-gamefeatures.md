# Live Ops via GameFeature Plugins Reference

Phase 8 of the pipeline. How to ship skins **without a full game patch** by
packaging each drop as a GameFeature plugin that activates / deactivates at
runtime. This is the Lyra-native pattern for live ops content.

---

## Why GameFeature Plugins for Skins?

Lyra's GameFeatures system was built for exactly this use case. The benefits
over shipping skins in `/Content/`:

```
✓ Activate / deactivate at runtime — seasonal vaulting just works
✓ Per-plugin asset registry — plugin's skins appear only when activated
✓ Plugin content can be downloaded after install (DLC chunk pattern)
✓ Modular: each plugin owns its data tables, assets, gameplay tags
✓ Cooks separately — patch a single plugin without recooking the base game
✓ Hot-reloadable in editor for fast iteration

✗ Putting everything in /Content/ means every cosmetic ships in the base
  pak file. 2,000 skins = 12GB+ base game.
```

Every seasonal drop, premium pack, collaboration event, or limited-time
cosmetic should ship as a GameFeature plugin.

---

## Plugin Anatomy

A cosmetic GameFeature plugin layout:

```
Plugins/GameFeatures/<Project>Cosmetics_<DropName>/
├── <Project>Cosmetics_<DropName>.uplugin
├── Content/
│   ├── Skins/                          → U<Project>SkinAsset instances
│   ├── Parts/                          → U<Project>CharacterPartDefinition assets
│   ├── Meshes/                         → SK_*, SM_* assets
│   ├── Materials/                      → MI_* assets (master M_ lives in base game)
│   ├── Textures/                       → T_* assets
│   └── Data/
│       └── DT_<Project>SkinCatalog_<DropName>  → DataTable rows for this drop
├── Source/
│   └── <Project>Cosmetics_<DropName>/
│       ├── <Project>Cosmetics_<DropName>.Build.cs
│       ├── Public/
│       │   └── <Project>CosmeticsModule_<DropName>.h
│       └── Private/
│           └── <Project>CosmeticsModule_<DropName>.cpp
└── <Project>Cosmetics_<DropName>_GameFeatureData.uasset
```

Most cosmetic plugins are **content-only** (no Source/ folder needed). Only
add a Source module if you need custom C++ logic for the drop (e.g. a
specially scripted unlock mechanic).

---

## The .uplugin File

```json
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0",
  "FriendlyName": "<Project> Cosmetics — <DropName>",
  "Description": "Seasonal cosmetic content drop: <DropName>",
  "Category": "Cosmetics",
  "CreatedBy": "<Studio>",
  "CanContainContent": true,
  "IsBetaVersion": false,
  "Installed": false,
  "ExplicitlyLoaded": true,
  "EnabledByDefault": false,
  "GameFeatureType": "GameFeature",

  "Plugins": [
    { "Name": "GameFeatures",     "Enabled": true },
    { "Name": "ModularGameplay",  "Enabled": true }
  ]
}
```

Critical settings:
```
ExplicitlyLoaded: true  → plugin only loads when LoadAndActivate is called
EnabledByDefault: false → does NOT auto-activate on game start
GameFeatureType: GameFeature → makes this a GameFeature plugin, not regular
```

If you set `EnabledByDefault: true`, the plugin's content activates on every
game launch — which defeats the purpose of seasonal vaulting.

---

## The GameFeatureData Asset

Each plugin needs a `UGameFeatureData` asset (created via right-click → 
GameFeature → Game Feature Data). This is the activation manifest.

```
<Project>Cosmetics_<DropName>_GameFeatureData:
  Default State: Initial
  Initial Game Feature Data Class: GameFeatureData

  Actions:
    [0] UGameFeatureAction_AddDataRegistrySource
        - Adds catalog DataTable rows to the global registry
    [1] UGameFeatureAction_AddGameplayCuePath  (optional)
        - Registers GameplayCue search paths for the drop's VFX
    [2] U<Project>GameFeatureAction_RegisterCatalog  (custom — see below)
        - Notifies CosmeticCatalogSubsystem to pick up these rows

  Asset Manager Rules:
    - Set the plugin's content to register as <Project>Skin / <Project>CharacterPart
      primary asset types so AssetManager picks them up on activate
```

---

## Custom GameFeatureAction for Catalog Registration

The stock GameFeature actions handle data registry sources, components,
and spawned actors. For the cosmetic catalog to know about plugin rows,
ship a custom action.

```cpp
// <Project>GameFeatureAction_RegisterCatalog.h
UCLASS(MinimalAPI, meta = (DisplayName = "Register <Project> Cosmetic Catalog"))
class U<Project>GameFeatureAction_RegisterCatalog : public UGameFeatureAction
{
    GENERATED_BODY()

public:
    /** The catalog DataTable from this plugin to register */
    UPROPERTY(EditAnywhere, Category = "Cosmetics")
    TSoftObjectPtr<UDataTable> CatalogChunk;

    /** Display name of the season — used for filter UI */
    UPROPERTY(EditAnywhere, Category = "Cosmetics")
    FGameplayTag SeasonTag;

    //~ UGameFeatureAction
    virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
    virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
    //~ End UGameFeatureAction
};
```

```cpp
// <Project>GameFeatureAction_RegisterCatalog.cpp
void U<Project>GameFeatureAction_RegisterCatalog::OnGameFeatureActivating(
    FGameFeatureActivatingContext& Context)
{
    UDataTable* Table = CatalogChunk.LoadSynchronous();
    if (!IsValid(Table))
    {
        UE_LOG(LogCosmetics, Error,
            TEXT("CatalogChunk failed to load for activating plugin"));
        return;
    }

    // Find a world to get the subsystem from — GameFeature actions don't
    // have a world directly. Use the engine's first valid world.
    UWorld* World = nullptr;
    for (const FWorldContext& WorldCtx : GEngine->GetWorldContexts())
    {
        if (WorldCtx.World() && WorldCtx.WorldType == EWorldType::Game)
        {
            World = WorldCtx.World();
            break;
        }
    }

    if (!IsValid(World))
    {
        // Editor-time or PIE early — fall back to deferred registration
        // via UGameInstance->OnStart delegate (omitted for brevity)
        return;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (!IsValid(GameInstance)) { return; }

    U<Project>CosmeticCatalogSubsystem* Catalog =
        GameInstance->GetSubsystem<U<Project>CosmeticCatalogSubsystem>();

    if (IsValid(Catalog))
    {
        // Extract plugin name from the activating context
        const FString PluginName =
            Context.GetPluginURL().ToString();

        Catalog->RegisterPluginCatalogRows(PluginName, Table);
    }
}

void U<Project>GameFeatureAction_RegisterCatalog::OnGameFeatureDeactivating(
    FGameFeatureDeactivatingContext& Context)
{
    UWorld* World = nullptr;
    for (const FWorldContext& WorldCtx : GEngine->GetWorldContexts())
    {
        if (WorldCtx.World() && WorldCtx.WorldType == EWorldType::Game)
        {
            World = WorldCtx.World();
            break;
        }
    }
    if (!IsValid(World)) { return; }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (!IsValid(GameInstance)) { return; }

    U<Project>CosmeticCatalogSubsystem* Catalog =
        GameInstance->GetSubsystem<U<Project>CosmeticCatalogSubsystem>();

    if (IsValid(Catalog))
    {
        const FString PluginName = Context.GetPluginURL().ToString();
        Catalog->UnregisterPluginCatalogRows(PluginName);
    }
}
```

The catalog subsystem's `RegisterPluginCatalogRows` reads every row from the
plugin's chunk and inserts them into its global row map. When the plugin
deactivates, the subsystem removes them — which automatically removes them
from shop UI filters.

---

## Activation Lifecycle

A typical plugin lifecycle from cold install to runtime active:

```
1. Player has the plugin's pak file installed (shipped with base game, or
   downloaded via platform store as DLC, or chunked installation)

2. Game launches. Base game registered the plugin's URL with the
   UGameFeaturesSubsystem but the plugin is in the "Registered" state.

3. Backend / live ops service tells the client "Season X is active".

4. Client calls:
     UGameFeaturesSubsystem::LoadAndActivateGameFeaturePlugin(PluginURL, Delegate)

5. UGameFeaturesSubsystem progresses the plugin through:
     Registered → Loaded → Activated

6. On Activated, every action in the plugin's GameFeatureData runs:
     - AssetManager scans the plugin's content for primary assets
     - Catalog DataTable rows registered into CosmeticCatalogSubsystem
     - GameplayCue paths added
     - Custom actions run

7. Shop UI's next refresh shows the new skins. Entitlement system can now
   grant ownership for these skin ids.

8. When the season ends (or backend tells us to deactivate):
     UGameFeaturesSubsystem::DeactivateGameFeaturePlugin(PluginURL)

   - Custom actions run their deactivate paths
   - Catalog rows removed from CosmeticCatalogSubsystem
   - Plugin returns to Loaded (or Registered) state

9. Players who already OWN a vaulted skin can still equip it — entitlement
   persists. They just can't see it in the shop anymore.
```

The vaulting behavior (own but can't buy) is critical for player goodwill —
never delete entitlements when vaulting content.

---

## Driving Activation from Live Ops Backend

The decision of which plugins to activate is server-driven. The client polls
or receives a push from the live ops service on login.

```cpp
USTRUCT()
struct F<Project>LiveOpsManifest
{
    GENERATED_BODY()

    /** Plugin URLs to activate this session */
    UPROPERTY()
    TArray<FString> ActivePluginUrls;

    /** Backend timestamp this manifest was generated */
    UPROPERTY()
    FString GeneratedAtUtc;
};

UCLASS()
class U<Project>LiveOpsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Fetch the active plugin manifest and apply it */
    UFUNCTION(BlueprintCallable, Category = "LiveOps")
    void ApplyManifestFromBackend();

private:
    void OnManifestReceived(const F<Project>LiveOpsManifest& Manifest);
    void ActivatePlugin(const FString& PluginUrl);
    void DeactivatePlugin(const FString& PluginUrl);

    UPROPERTY()
    TSet<FString> CurrentlyActivePlugins;
};
```

```cpp
void U<Project>LiveOpsSubsystem::OnManifestReceived(
    const F<Project>LiveOpsManifest& Manifest)
{
    UGameFeaturesSubsystem& GFSubsystem = UGameFeaturesSubsystem::Get();

    // Activate any plugins in the manifest that aren't already active
    for (const FString& PluginUrl : Manifest.ActivePluginUrls)
    {
        if (!CurrentlyActivePlugins.Contains(PluginUrl))
        {
            ActivatePlugin(PluginUrl);
        }
    }

    // Deactivate any currently-active plugins not in the new manifest
    TArray<FString> ToDeactivate;
    for (const FString& Active : CurrentlyActivePlugins)
    {
        if (!Manifest.ActivePluginUrls.Contains(Active))
        {
            ToDeactivate.Add(Active);
        }
    }
    for (const FString& PluginUrl : ToDeactivate)
    {
        DeactivatePlugin(PluginUrl);
    }
}

void U<Project>LiveOpsSubsystem::ActivatePlugin(const FString& PluginUrl)
{
    UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(
        PluginUrl,
        FGameFeaturePluginLoadComplete::CreateLambda(
            [this, PluginUrl](const UE::GameFeatures::FResult& Result)
            {
                if (Result.HasValue())
                {
                    CurrentlyActivePlugins.Add(PluginUrl);
                    UE_LOG(LogLiveOps, Display,
                        TEXT("Activated cosmetic plugin: %s"), *PluginUrl);
                }
                else
                {
                    UE_LOG(LogLiveOps, Error,
                        TEXT("Failed to activate %s: %s"),
                        *PluginUrl, *Result.GetError());
                }
            }));
}
```

---

## Authoring a New Cosmetic Drop — Studio Workflow

Step-by-step for the art / design team to ship a new drop:

```
1. Generate plugin scaffold:
   Editor → Edit menu → Plugins → New Plugin → Game Feature (Content Only)
   Name: <Project>Cosmetics_<DropName>
   Location: Plugins/GameFeatures/

2. Author assets in the plugin's Content/ folder:
   - SK_, SM_, T_, MI_ assets per skin
   - U<Project>SkinAsset for each new skin
   - U<Project>CharacterPartDefinition for any modular parts

3. Build DT_<Project>SkinCatalog_<DropName>:
   - One row per new skin
   - Soft-ref the skin assets in the plugin

4. Configure the GameFeatureData:
   - Add UGameFeatureAction_AddDataRegistrySource referencing the DataTable
   - Add U<Project>GameFeatureAction_RegisterCatalog referencing the DT
   - Set Season tag and any rarity / availability metadata

5. Test activation in editor:
   - Project Settings → GameFeature Plugins → set plugin to Active
   - Verify catalog subsystem picks up new rows
   - Verify shop UI shows the new skins
   - Equip a skin and verify it renders correctly

6. Cook the plugin:
   - Project Launcher → Cook by the Book
   - Or via UAT: -CookPlugin=<Project>Cosmetics_<DropName>

7. Deploy:
   - Ship plugin pak file via your delivery channel (base install / CDN / store)
   - Update backend manifest to include the new plugin URL when season starts
```

---

## Per-Platform Cosmetic Plugin Cooking

GameFeature plugin pak files can be cooked per platform — useful when a skin
has mobile-specific lower-quality assets.

```bash
# Cook a single plugin for Android only
UE5Editor-Cmd.exe <Project>.uproject \
  -run=Cook \
  -TargetPlatform=Android_ASTC \
  -Plugin=<Project>Cosmetics_<DropName> \
  -unattended -stdout
```

The plugin's content cooks with platform-appropriate texture compression
(ASTC for Android, DXT/BC for Win64, etc.) automatically — no per-asset
overrides needed if your texture group settings are correct.

---

## Common GameFeature Plugin Failures

| Symptom | Cause | Fix |
|---|---|---|
| Plugin activates but new skins don't appear in shop | Custom Register action not in GameFeatureData | Add U<Project>GameFeatureAction_RegisterCatalog |
| `LoadAndActivateGameFeaturePlugin` returns error | Plugin pak file not present at runtime | Check pak chunking; verify plugin shipped in build |
| Skins show but can't be purchased | Catalog rows registered, but entitlement system doesn't know about plugin assets | EntitlementSubsystem also needs to be notified — wire up the same action to call it |
| Shop empties on deactivate but client crashes | Asset references still held by shop UI | Force-release soft handles in shop browser on deactivate event |
| Editor PIE doesn't pick up plugin assets | Plugin set to ExplicitlyLoaded but not auto-activated in editor | Project Settings → GameFeature Plugins → enable for PIE |
| Cooked plugin missing assets | Plugin's content not in AssetManager primary asset rules | Add Directories entry in DefaultGame.ini for plugin path |
| Two plugins with same SkinId collide | DataTable row keys overlap | Prefix every drop's SkinIds with drop name: `SeasonOne_AlphaHelmet` |

---

## Verification Checklist — End of Phase 8

```
□ At least one cosmetic GameFeature plugin authored and built
□ Plugin's .uplugin has ExplicitlyLoaded=true, EnabledByDefault=false
□ GameFeatureData asset includes RegisterCatalog action
□ LoadAndActivateGameFeaturePlugin returns success at runtime
□ CosmeticCatalogSubsystem picks up new rows on activate
□ Shop UI shows new skins; equipping works end-to-end
□ DeactivateGameFeaturePlugin removes rows but entitlement persists
□ LiveOps subsystem diffs current vs target manifest and activates/deactivates
□ Plugin cooks per platform (Android_ASTC, IOS, Windows, PS5, XSX)
□ Plugin pak file ships via your delivery channel (CDN, base install, or store DLC)
□ No SkinId collisions between plugins (every drop prefixes ids uniquely)
```
