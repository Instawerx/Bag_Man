// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "UI/LyraActivatableWidget.h"    // ELyraWidgetInputMode (the enum only -- the class is private)
#include "UI/AFLW_LoadoutTileBase.h"     // EAFLLoadoutAxis + UAFLW_LoadoutTileBase (the LOADOUT entry widget)

#include "AFLW_FrontEndMarket.generated.h"

struct FUIInputConfig;
class UWidget;
class UUserWidget;
class UCommonButtonBase;
class UAFLCosmeticLoadoutComponent;
class AAFLLoadoutDisplayPawn;

/** STORE = purchasable + BUY (the store WBP graph drives it, UNCHANGED). LOADOUT = owned + EQUIP with OUR tile. */
UENUM(BlueprintType)
enum class EAFLMarketMode : uint8
{
	Store   UMETA(DisplayName = "Store"),
	Loadout UMETA(DisplayName = "Loadout")
};

/**
 * UAFLW_FrontEndMarket -- the shared front-end "Digital Market" chassis. The store WBP reparents here and renders
 * over the live armory (transparent center). TWO MODES, ONE WIDGET:
 *  - STORE: this class NO-OPS -> the store WBP's own graph + its BP tile drive list/tabs/buy, byte-for-byte.
 *  - LOADOUT: this class OVERRIDES at runtime -- it points the store's ListView at OUR readable C++ tile
 *    (UAFLW_LoadoutTileBase, via OnGetEntryClassForItem, WITHOUT touching the store's default EntryWidgetClass),
 *    feeds owned entries, and binds each tile's own OnTileClicked -> equip onto the armory display pawn.
 *    No graph surgery, no opaque-BP hooks -- the tile is ours end to end.
 * ZERO coupling to the in-match UAFLW_LoadoutBase (shared vocabulary only: the tile + the EAFLLoadoutAxis enum).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_FrontEndMarket : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Store vs Loadout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Market")
	EAFLMarketMode Mode = EAFLMarketMode::Store;

	/** Enter LOADOUT at runtime -- the pusher calls this AFTER push/construct (CommonUI's push init-hook runs
	 *  AFTER NativeConstruct, so setting Mode pre-construct doesn't take). BlueprintCallable so the hub's LOADOUT
	 *  button can call it on the pushed market widget (Cast -> EnterLoadout) for the post-push mode flip. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Market")
	void EnterLoadout();

protected:
	//~UCommonActivatableWidget -- ULyraActivatableWidget parity (re-implemented; that class is LyraGame-private)
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	/** Gamepad focus -> the product browser, resolved by NAME (never a BindWidget). */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~End

	//~UUserWidget
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~End

	/** Center hero slot -> live scene: dissolve the market backdrop when a display pawn is behind us (armory). */
	void ApplyShowroomMode();

	// --- STEP A: STORE mode -- make the category tabs FILTER (Path 1: filter the store's OWN BP items;
	// the store's tile + BUY/EQUIP + BP graph all stay intact). The tabs shipped as UBorders with NO click
	// binding -> we bind OnMouseButtonDownEvent (same mechanism as LOADOUT) and re-SetListItems a namespace-
	// filtered subset of the store's own BP_AFL_StoreEntryData items. Active tab -> cyan. ---
	void EnterStoreMode();
	void FilterStore(int32 TabIndex);
	void UpdateStoreTabVisuals(int32 ActiveIndex);
	/** Read the CosmeticId FName off a store item (BP_AFL_StoreEntryData) by its confirmed property name. */
	static FName ReadEntryCosmeticId(const UObject* Item);

	UFUNCTION() FEventReply OnStoreTabWeapons(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnStoreTabSkins(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnStoreTabHelmets(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnStoreTabVisors(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnStoreTabEmotes(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnStoreTabBundles(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	// --- STORE PREVIEW (front-end try-before-buy): selecting a store card shows it ON the display robot,
	// temporarily, WITHOUT committing (unowned-OK). Reverts on deselect / tab-change / market close. The
	// entitlement gate lives only in the commit, so preview (no commit) bypasses it for free. ---
	/** Bound (AddUObject) to the store ListView's NATIVE OnItemSelectionChanged (the BP one is private). Item ==
	 *  the newly selected entry, or nullptr on full deselect. */
	void OnStoreItemSelectionChanged(UObject* Item);
	/** CosmeticId namespace -> loadout axis (the reverse of the store-tab taxonomy). False if unclassifiable. */
	static bool ClassifyStoreAxis(FName CosmeticId, EAFLLoadoutAxis& OutAxis);
	/** Clear any active store preview -> the display robot returns to the player's real saved loadout. */
	void RevertStorePreview();

	// --- STORE's OWN TILE (reliable browsing) -- the store's BP tile's BUY/EQUIP buttons ate the row click, so no
	// second item could be selected. STORE now renders OUR readable tile (UAFLW_LoadoutTileBase) over the store's
	// SAME BP items; each tile's own click drives a real ListView selection -> the store's detail panel + BUY AND
	// our try-on preview fire. Additive per-mode: LOADOUT still equips; STORE never touches the buy DATA-spine. ---
	/** OnEntryWidgetGenerated (STORE): bind each generated tile's OnTileClicked -> HandleStoreTileClicked. */
	void OnStoreTileGenerated(UUserWidget& EntryWidget);
	/** Bound to each store tile's OnTileClicked -> select that id in the ListView (detail panel + BUY + preview). */
	UFUNCTION()
	void HandleStoreTileClicked(EAFLLoadoutAxis Axis, FName CosmeticId);

	// --- B2c/d: per-tile BUY + EQUIP wired to the PROVEN buy path (buy-spine, must not regress) ---
	/** Store tile BUY -> Wallet->ClientRequestPurchase(id, Volts|Watts). */
	UFUNCTION()
	void HandleStoreTileBuy(FName CosmeticId, bool bWatts);
	/** Store tile EQUIP (owned) -> preview/equip on the display hero. */
	UFUNCTION()
	void HandleStoreTileEquip(FName CosmeticId);
	/** Wallet OnWalletChanged -> refresh the store tiles so a bought item flips BUY -> OWNED/EQUIP live. */
	UFUNCTION()
	void OnStoreWalletChanged(int32 Volts, int32 Watts);

	// --- CHROME (top bar) -- WIRE + STYLE the existing scaffold by NAME (GetWidgetFromName; no BindWidget, no
	// reparent -> zero WBP-structure risk). Pills bind the live (replicated) wallet; styling is SSOT neon. ---
	/** Push live balances into VoltsValue / WattsValue (grouped) + hide the scaffold's duplicate WalletText readout.
	 *  Called on enter + every OnWalletChanged. */
	void RefreshWalletChrome(int32 Volts, int32 Watts);
	/** One-time SSOT styling of the whole chrome: neon-outline wallet pills + coin tints, profile chip (name real,
	 *  level STUB), bottom utility bar (dark-gloss band + tinted labels; only Close is wired, rest are stubs). */
	void StyleChrome();

	// --- STEP 5 LOADOUT mode (C++-runtime; STORE mode never reaches any of this) ---
	void EnterLoadoutMode();
	void PopulateForAxis(EAFLLoadoutAxis Axis);

	/** Drive the store's detail-panel widgets (DetailName/Series/Desc/Rarity) BY NAME from the catalog entry, so a
	 *  LOADOUT tile SELECT updates the panel the store's BP graph normally fills on ListView selection. */
	void PopulateDetailForLoadout(FName CosmeticId);

	/** Switch the active axis-group + repopulate. */
	void SelectAxis(EAFLLoadoutAxis Axis);

	// The store's tabs are UBorder (not buttons) -> their only click is OnMouseButtonDownEvent, a single-cast
	// dynamic delegate (no capture), so one handler per tab. Our BindDynamic REPLACES the store's binding on this
	// LOADOUT instance -> no STORE conflict.
	UFUNCTION() FEventReply OnTabWeapon(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnTabWeaponSkin(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnTabBeam(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnTabIdentity(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnTabColors(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() FEventReply OnTabFacemask(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	/** OnGetEntryClassForItem handler: LOADOUT generates OUR tile (WBP_AFL_LoadoutTile) per item, overriding the
	 *  store's default EntryWidgetClass WITHOUT modifying it -> STORE tile stays byte-for-byte. */
	TSubclassOf<UUserWidget> GetLoadoutEntryClass(UObject* Item) const;

	/** OnEntryWidgetGenerated handler: bind each generated tile's OnTileClicked -> equip. */
	void OnLoadoutEntryGenerated(UUserWidget& EntryWidget);

	/** Bound to each tile's OnTileClicked (the tile-body SelectButton) -> SELECT: preview on the display robot +
	 *  fill the detail panel. The COMMIT lives on the tile's EQUIP button (HandleLoadoutTileEquip). */
	UFUNCTION()
	void HandleLoadoutTileClicked(EAFLLoadoutAxis Axis, FName CosmeticId);

	/** Bound to each tile's OnEquipClicked (the EQUIP button) -> COMMIT: equip the previewed cosmetic for real
	 *  (data RPC + fan-out onto the display pawn). Uses the active axis (the loadout shows one axis at a time). */
	UFUNCTION()
	void HandleLoadoutTileEquip(FName CosmeticId);

	/** Equip one axis (data RPC) + fan it out onto the armory display pawn (visual). */
	void EquipSelected(FName CosmeticId, EAFLLoadoutAxis Axis);

	UAFLCosmeticLoadoutComponent* GetLocalLoadout() const;
	AAFLLoadoutDisplayPawn* GetDisplayPawn() const;

	/** Which axis-group the loadout list is showing (5a: fixed to Colors/BodyColor; tabs switch it in 5b). */
	EAFLLoadoutAxis CurrentAxis = EAFLLoadoutAxis::BodyColor;

	/** Guard so EnterLoadoutMode runs exactly once. */
	bool bLoadoutActive = false;

	/** STEP A: the store's FULL item set (cached lazily on first tab click) to filter category subsets from. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> StoreFullItems;

	/** Active store category tab (-1 = show all / no filter applied yet). */
	int32 ActiveStoreTab = -1;

	/** OUR tile class (WBP_AFL_LoadoutTile) the ListView uses in LOADOUT mode. */
	UPROPERTY(Transient)
	TSubclassOf<UAFLW_LoadoutTileBase> LoadoutTileClass;

	UPROPERTY(EditDefaultsOnly, Category = Input)
	ELyraWidgetInputMode InputConfig = ELyraWidgetInputMode::Default;

	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	// NOTE: NO BindWidgets. A base-typed BindWidget on a graph-driven store widget re-types its graph variable and
	// broke the store on reparent. Every widget this chassis touches -- focus target, backdrop, tabs, list, buttons
	// -- is fetched by NAME (GetWidgetFromName), which creates no property. The LOADOUT tiles are OUR own C++ tile.
};
