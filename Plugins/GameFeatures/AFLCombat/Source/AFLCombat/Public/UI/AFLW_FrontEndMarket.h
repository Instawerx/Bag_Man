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
	 *  AFTER NativeConstruct, so setting Mode pre-construct doesn't take). */
	void EnterLoadout();

protected:
	//~UCommonActivatableWidget -- ULyraActivatableWidget parity (re-implemented; that class is LyraGame-private)
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	/** Gamepad focus -> the product browser, resolved by NAME (never a BindWidget). */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~End

	//~UUserWidget
	virtual void NativeConstruct() override;
	//~End

	/** Center hero slot -> live scene: dissolve the market backdrop when a display pawn is behind us (armory). */
	void ApplyShowroomMode();

	// --- STEP 5 LOADOUT mode (C++-runtime; STORE mode never reaches any of this) ---
	void EnterLoadoutMode();
	void PopulateForAxis(EAFLLoadoutAxis Axis);

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

	/** Bound to each tile's OnTileClicked (the tile's own SelectButton) -> equip the cosmetic onto the display robot. */
	UFUNCTION()
	void HandleLoadoutTileClicked(EAFLLoadoutAxis Axis, FName CosmeticId);

	/** Equip one axis (data RPC) + fan it out onto the armory display pawn (visual). */
	void EquipSelected(FName CosmeticId, EAFLLoadoutAxis Axis);

	UAFLCosmeticLoadoutComponent* GetLocalLoadout() const;
	AAFLLoadoutDisplayPawn* GetDisplayPawn() const;

	/** Which axis-group the loadout list is showing (5a: fixed to Colors/BodyColor; tabs switch it in 5b). */
	EAFLLoadoutAxis CurrentAxis = EAFLLoadoutAxis::BodyColor;

	/** Guard so EnterLoadoutMode runs exactly once. */
	bool bLoadoutActive = false;

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
