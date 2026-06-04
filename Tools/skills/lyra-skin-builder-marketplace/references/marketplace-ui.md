# Marketplace UI Reference

Phase 7 of the pipeline. The in-game cosmetic shop: grid browsing, preview,
purchase, currency, and equipping. Built on Lyra's CommonUI + LyraHUDLayout
stack so it works across PC/Console/Mobile with input parity.

---

## Architecture Overview

The shop is built from three coordinated CommonUI widgets, all living on
Lyra's `UI.Layer.GameMenu` layer:

```
W_<Project>CosmeticShop_Root          (UCommonActivatableWidget)
├── W_<Project>CosmeticShop_Browser   (grid of catalog rows)
├── W_<Project>CosmeticShop_Preview   (3D preview viewport + skin detail)
└── W_<Project>CosmeticShop_Wallet    (currency display, persistent header)

Modal Stack (pushed on UI.Layer.Modal):
├── W_<Project>PurchaseConfirmDialog
├── W_<Project>InsufficientFundsDialog
└── W_<Project>PurchaseSuccessDialog
```

This separation matters: the root holds shared state, the browser handles
filtering/selection, the preview is the expensive component (3D viewport),
and the wallet refreshes independently of shop navigation.

---

## Root Widget — W_<Project>CosmeticShop_Root

The activatable widget the player pushes onto `UI.Layer.GameMenu`. Owns the
selected skin state and routes between sub-widgets.

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class <PROJECT>GAME_API U<Project>CosmeticShopRoot
    : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** Sub-widget bindings (set in BP defaults via meta) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<U<Project>CosmeticShopBrowser> Browser;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<U<Project>CosmeticShopPreview> Preview;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<U<Project>CosmeticShopWallet> Wallet;

    /** Currently focused skin id */
    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FName FocusedSkinId;

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;

    /** Input config — show "Back" and "Purchase" prompts on console/mobile */
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    UFUNCTION()
    void HandleSkinFocused(FName SkinId);

    UFUNCTION()
    void HandlePurchaseRequested(FName SkinId);

    UFUNCTION()
    void HandleEquipRequested(FName SkinId);

private:
    UPROPERTY()
    TObjectPtr<U<Project>CosmeticCatalogSubsystem> CatalogSubsystem;

    UPROPERTY()
    TObjectPtr<U<Project>EntitlementSubsystem> EntitlementSubsystem;
};
```

Input config — this is what makes the shop feel native on every platform:

```cpp
TOptional<FUIInputConfig> U<Project>CosmeticShopRoot::GetDesiredInputConfig() const
{
    FUIInputConfig Config;
    Config.InputMode = ECommonInputMode::Menu;       // disables game input
    Config.MouseCaptureMode = EMouseCaptureMode::NoCapture;
    Config.bHideCursorDuringViewportCapture = false;
    return Config;
}
```

The CommonUI subsystem picks up the input config and switches the platform-
appropriate prompts (Cross button on PS5, A on Xbox, Enter on PC, tap on mobile)
automatically.

---

## Browser Widget — Grid + Filters

The shop grid displays soft-loaded thumbnails. Filtering happens in-subsystem
(O(1) by indexed tags), not in the widget.

**Layout:**
```
┌──────────────────────────────────────────────────────────┐
│ [Featured] [All] [Common] [Rare] [Epic] [Legendary]      │  ← Filter tabs
├──────────────────────────────────────────────────────────┤
│ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ │
│ │ [TN] │ │ [TN] │ │ [TN] │ │ [TN] │ │ [TN] │ │ [TN] │ │  ← Tile grid
│ │ Name │ │ Name │ │ Name │ │ Name │ │ Name │ │ Name │ │
│ │ 500 ◇│ │ 800 ◇│ │ 200 ◇│ │1200 ◇│ │ 600 ◇│ │ 950 ◇│ │
│ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ │
│ ... (virtualized list — recycle tiles offscreen)         │
└──────────────────────────────────────────────────────────┘
```

**Virtualization is mandatory** for catalogs > 200 items. Use
`UCommonListView` (CommonUI's list widget) which recycles entry widgets.
A 2,000-item catalog with non-virtualized tiles eats 2GB of RAM on mobile
from soft texture loads alone.

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class <PROJECT>GAME_API U<Project>CosmeticShopBrowser
    : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonListView> SkinListView;

    /** Designer assigns the entry widget BP — W_<Project>SkinTile */
    UPROPERTY(EditDefaultsOnly, Category = "Shop")
    TSubclassOf<U<Project>SkinTileEntry> EntryWidgetClass;

    UPROPERTY(BlueprintAssignable)
    FOnSkinFocused OnSkinFocused;

    UPROPERTY(BlueprintAssignable)
    FOnSkinActivated OnSkinActivated;

    /** Refresh the grid with the given filter */
    UFUNCTION(BlueprintCallable)
    void ApplyFilter(FGameplayTag RarityFilter, FGameplayTag SeasonFilter);

protected:
    virtual void NativeOnInitialized() override;

    UFUNCTION()
    void HandleSelectionChanged(UObject* SelectedItem);

    UFUNCTION()
    void HandleItemActivated(UObject* ActivatedItem);
};
```

Entry widgets implement `IUserObjectListEntry` (CommonUI requires it):

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class <PROJECT>GAME_API U<Project>SkinTileEntry
    : public UCommonUserWidget
    , public IUserObjectListEntry
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> ThumbnailImage;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> NameText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> PriceText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> RarityFrameImage;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> OwnedBadge;

protected:
    virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

private:
    UPROPERTY()
    TWeakObjectPtr<U<Project>SkinRowWrapper> CurrentRow;

    void PopulateFromRow(const F<Project>SkinCatalogRow& Row);
    void OnThumbnailLoaded(UTexture2D* Texture);
};
```

When the list view sets a row on an entry, the entry async-loads its
thumbnail. Off-screen entries get released (their soft handle drops) so
memory stays bounded.

---

## Preview Widget — 3D Viewport + Detail Panel

The expensive component. Renders the focused skin in a 3D viewport with
rotation, lighting, and (optionally) animation. Detail panel shows skin
description, rarity, parts breakdown, and the purchase / equip CTA.

**Layout:**
```
┌─────────────────────────────────────────────────────────┐
│  ┌─────────────────────────┐  ┌────────────────────┐    │
│  │                         │  │ Skin Name [Legend.] │    │
│  │                         │  │ ──────────────────  │    │
│  │       [3D PREVIEW]      │  │ Description text...  │    │
│  │       (drag to rotate)  │  │                      │    │
│  │                         │  │ ⊕ Helmet Variant     │    │
│  │                         │  │ ⊕ Cape Variant       │    │
│  │                         │  │                      │    │
│  │  [Idle ▾]  [↻ Reset]    │  │ [   PURCHASE 1200 ◇  ] │    │
│  └─────────────────────────┘  │ [      EQUIP        ] │    │
│                                └────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

The 3D preview uses a render-target-driven viewport. There are two valid
approaches:

```
APPROACH A — UMG SceneCaptureComponent2D
  ✓ Simple to set up; renders into a UMG Image widget
  ✓ Independent lighting / camera control
  ✗ Performance: full scene capture, expensive
  → Use on PC/Console only

APPROACH B — Spawned Preview Actor + Camera
  ✓ Cheap — only the preview pawn is rendered into the target
  ✓ Allows interactive rotation, animation playback
  ✓ Works on mobile
  → Use for cross-platform — RECOMMENDED
```

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class <PROJECT>GAME_API U<Project>CosmeticShopPreview
    : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UImage> PreviewViewport;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> SkinNameText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<URichTextBlock> DescriptionText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonButtonBase> PurchaseButton;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UCommonButtonBase> EquipButton;

    /** Render target driving the viewport image */
    UPROPERTY(EditDefaultsOnly, Category = "Preview")
    TObjectPtr<UTextureRenderTarget2D> PreviewRenderTarget;

    /** Update the preview to show the given skin (async-loads as needed) */
    UFUNCTION(BlueprintCallable, Category = "Preview")
    void ShowSkin(FName SkinId);

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;

    UFUNCTION()
    void HandlePurchaseClicked();

    UFUNCTION()
    void HandleEquipClicked();

private:
    /** The actor spawned in a hidden preview level to render the skin */
    UPROPERTY()
    TWeakObjectPtr<A<Project>CosmeticPreviewActor> PreviewActor;

    /** Currently displayed skin */
    FName CurrentSkinId;

    void EnsurePreviewActorSpawned();
    void OnSkinAssetLoaded(U<Project>SkinAsset* SkinAsset);
};
```

The preview actor is a stripped-down character spawned in a dedicated
preview level loaded as a sub-level on shop entry, then unloaded on exit.
This keeps the gameplay world untouched and gives the preview its own
isolated rendering environment.

---

## Wallet Widget — Currency Display

Persistent header showing the player's currency balances. Updates via
event delegate from the wallet subsystem, not via tick.

```cpp
UCLASS(Abstract, BlueprintType, Blueprintable)
class <PROJECT>GAME_API U<Project>CosmeticShopWallet
    : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> SoftCurrencyText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> PremiumCurrencyText;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleBalanceChanged(FGameplayTag CurrencyTag, int32 NewBalance);

private:
    UPROPERTY()
    TObjectPtr<U<Project>WalletSubsystem> WalletSubsystem;
};
```

---

## Purchase Flow

The purchase sequence touches UI, the wallet subsystem, the entitlement
subsystem, and a server RPC. Always treat the client-side purchase as a
**request**, not a commit — the server is the source of truth.

```
1. Player clicks PURCHASE in preview widget
2. Show W_<Project>PurchaseConfirmDialog as modal on UI.Layer.Modal
3. On confirm:
   a. Client sends ServerRequestPurchase RPC to PlayerState
   b. Server validates:
      - Catalog entry exists for SkinId
      - Skin is currently available (season/event window)
      - Player has sufficient balance
      - Player doesn't already own this skin
   c. Server deducts currency atomically
   d. Server grants entitlement (writes to authoritative store)
   e. Server replicates new entitlement + balance to client
4. Client UI:
   - Wallet updates via delegate
   - Skin tile updates to show "OWNED" badge
   - Show W_<Project>PurchaseSuccessDialog as modal
   - Optionally auto-equip the skin
```

Server RPC sketch:

```cpp
UFUNCTION(Server, Reliable)
void ServerRequestPurchase(FName SkinId);

void A<Project>PlayerState::ServerRequestPurchase_Implementation(FName SkinId)
{
    U<Project>CosmeticCatalogSubsystem* Catalog =
        UGameplayStatics::GetGameInstance(GetWorld())
            ->GetSubsystem<U<Project>CosmeticCatalogSubsystem>();

    F<Project>SkinCatalogRow Row;
    if (!Catalog->GetCatalogRow(SkinId, Row))
    {
        ClientPurchaseFailed(SkinId, EPurchaseFailureReason::UnknownSkin);
        return;
    }

    U<Project>EntitlementSubsystem* Entitlements =
        UGameplayStatics::GetGameInstance(GetWorld())
            ->GetSubsystem<U<Project>EntitlementSubsystem>();

    if (Entitlements->IsOwned(GetUniqueId(), SkinId))
    {
        ClientPurchaseFailed(SkinId, EPurchaseFailureReason::AlreadyOwned);
        return;
    }

    if (!IsSkinAvailableForPurchase(Row))
    {
        ClientPurchaseFailed(SkinId, EPurchaseFailureReason::NotAvailable);
        return;
    }

    U<Project>WalletSubsystem* Wallet =
        UGameplayStatics::GetGameInstance(GetWorld())
            ->GetSubsystem<U<Project>WalletSubsystem>();

    if (!Wallet->TrySpend(GetUniqueId(), Row.CurrencyTag, Row.BaseCost))
    {
        ClientPurchaseFailed(SkinId, EPurchaseFailureReason::InsufficientFunds);
        return;
    }

    // Grant entitlement — this is the durable commit
    Entitlements->GrantEntitlement(GetUniqueId(), SkinId);

    // Notify client of success
    ClientPurchaseSucceeded(SkinId);
}
```

The wallet's `TrySpend` must be atomic — never split into "check then
deduct" because that creates a race condition where two simultaneous
purchases each pass the check and overdraw the balance.

---

## CommonUI Layer Pushing

The shop pushes itself onto `UI.Layer.GameMenu`. Modals push onto
`UI.Layer.Modal`. The HUD layer is suppressed automatically when GameMenu
is active.

```cpp
// From a button click in the pause menu or main menu:
void U<Project>PauseMenu::HandleOpenShopClicked()
{
    if (UCommonUIExtensions* Extensions = GetCommonUIExtensions())
    {
        Extensions->PushContentToLayer_ForPlayer(
            GetOwningLocalPlayer(),
            FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.GameMenu")),
            CosmeticShopRootClass);
    }
}
```

`CosmeticShopRootClass` is a `TSoftClassPtr<U<Project>CosmeticShopRoot>` set
in defaults. Soft class so the shop UI class itself isn't loaded until the
shop is opened.

---

## Mobile-Specific Adjustments

```
✗ No gamepad cursor — touch input maps to direct widget activation
✓ Larger tap targets: minimum 44pt (iOS) / 48dp (Android)
✓ Skin tile size: ≥120pt to support thumb tapping
✓ Preview viewport: reduced render target resolution (512×768 vs 1024×1536)
✓ Scrolling: native momentum scroll on CommonListView
✓ Modal dialogs: full-screen sheets on mobile, popovers on tablet/desktop
```

Use `UCommonInputSubsystem` to detect the current input mode and adjust
layout at runtime:

```cpp
const ECommonInputType InputType =
    UCommonInputSubsystem::Get(GetOwningLocalPlayer())->GetCurrentInputType();

const bool bIsTouch = (InputType == ECommonInputType::Touch);
SkinTileGrid->SetEntryWidgetMinSize(bIsTouch ? FVector2D(140, 180) :
                                                FVector2D(100, 140));
```

---

## Verification Checklist — End of Phase 7

```
□ W_<Project>CosmeticShop_Root activates on UI.Layer.GameMenu
□ CommonListView virtualizes 1000+ items without OOM on mobile
□ Soft thumbnail load + release cycle measured (no leaks, no hitches)
□ 3D preview spawns/despawns in dedicated sub-level
□ Purchase flow round-trips: client request → server validate → entitlement granted
□ Wallet updates via delegate, not tick
□ "Insufficient funds" / "already owned" / "unavailable" all show correct modal
□ Gamepad navigation works in browser grid and preview detail
□ Touch input works on mobile with appropriate tile sizes
□ Cosmetic.Rarity tag drives tile frame color (visual rarity hierarchy)
□ Soft-loaded preview assets release when shop closes
```
