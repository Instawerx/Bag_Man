// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h" // front-end market renders OUR tile as a ListView entry

#include "AFLW_LoadoutTileBase.generated.h"

class UButton;
class UTextBlock;
class URichTextBlock;
class UWidget;
class UBorder;
class UImage;
class UTexture2D;

/** The loadout axis a picker/tile drives -- decoupled from EAFLCosmeticType (which has NO WeaponSkin value:
 *  weapon-skins share Type==Weapon and are disambiguated by the AFL.WeaponSkin.* namespace). */
UENUM(BlueprintType)
enum class EAFLLoadoutAxis : uint8
{
	Weapon      UMETA(DisplayName = "Weapon"),      // AFL.Weapon.*      -> WeaponId       (type=Weapon, prefix-filtered)
	WeaponSkin  UMETA(DisplayName = "Weapon Skin"), // AFL.WeaponSkin.*  -> WeaponSkinId   (type=Weapon, skin-prefix)
	Beam        UMETA(DisplayName = "Beam"),        // AFL.Beam.*        -> BeamId          (type=Beam)
	Identity    UMETA(DisplayName = "Identity"),    // AFL.Team.* + AFL.Character.* -> IdentityType + Team/CharacterId (dual-type)
	BodyColor   UMETA(DisplayName = "Body Color"),  // AFL.Finish.*      -> BodyId          (BodyId resolves to a Finish; free base = 7 finishes)
	EdgeColor   UMETA(DisplayName = "Edge Color"),  // AFL.Edge.*        -> EdgeId          (type=SkinColor_Edge)
	Facemask    UMETA(DisplayName = "Facemask")     // AFL.Facemask.*    -> FacemaskId      (type=Facemask)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAFLLoadoutTileClicked, EAFLLoadoutAxis, Axis, FName, CosmeticId);
/** STORE rich card: BUY pressed. bWatts=false -> pay Volts, true -> pay Watts (the owner maps to EAFLPayCurrency). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAFLStoreTileBuy, FName, CosmeticId, bool, bWatts);
/** STORE rich card: EQUIP pressed on an owned item (owner previews/equips it). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAFLStoreTileEquip, FName, CosmeticId);

/**
 * UAFLMarketLoadoutItem -- a front-end LOADOUT list item. Carries a tile's full SetTileData payload so
 * UAFLW_LoadoutTileBase can render as a ListView entry. The front-end market (UAFLW_FrontEndMarket) points the
 * store's ListView at OUR tile via OnGetEntryClassForItem and feeds these items in LOADOUT mode; the store's own
 * BP tile is used only in STORE mode (so STORE stays byte-for-byte).
 */
UCLASS()
class AFLCOMBAT_API UAFLMarketLoadoutItem : public UObject
{
	GENERATED_BODY()

public:
	EAFLLoadoutAxis Axis = EAFLLoadoutAxis::BodyColor;
	FName CosmeticId;
	FText DisplayName;
	bool bEquipped = false;
	/** SUGGESTED zone: an unowned, buyable item (B2 renders a plain card; B3 adds price + BUY + the buy->owned hop). */
	bool bPurchasable = false;
	bool bIsSwatch = false;
	FLinearColor SwatchColor = FLinearColor::White;
	TSoftObjectPtr<UTexture2D> Thumbnail;
};

/**
 * UAFLMarketSectionHeader -- a zone-header row in the front-end loadout list (EQUIPPED / OWNED / SUGGESTED).
 * Rendered by UAFLW_LoadoutSectionHeader (NOT the tile); UAFLW_FrontEndMarket::GetLoadoutEntryClass dispatches on
 * the item type so ONE ShopListView carries both headers and cosmetic tiles (feed change, not a new grid).
 */
UCLASS()
class AFLCOMBAT_API UAFLMarketSectionHeader : public UObject
{
	GENERATED_BODY()

public:
	/** The zone caption ("EQUIPPED" / "OWNED" / "SUGGESTED"). */
	FText Label;
};

/**
 * UAFLW_LoadoutTileBase -- one OWNED-cosmetic tile.
 *
 * The C++ base owns the data + the click; the WBP child owns the layout (BindWidget: SelectButton +
 * NameText + optional EquippedBadge). Used two ways, both C++-driven: the in-match locker (UAFLW_LoadoutBase)
 * spawns these into a grid via CreateWidget + SetTileData; the front-end market feeds them through a ListView
 * via IUserObjectListEntry (NativeOnListItemObjectSet -> the same SetTileData). Its SelectButton broadcasts
 * OnTileClicked, so the click is a delegate WE own -- no dependence on the ListView's routing.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_LoadoutTileBase : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** Broadcast on click, carrying this tile's axis + CosmeticId (the owner binds this -> EquipForAxis). */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Loadout")
	FOnAFLLoadoutTileClicked OnTileClicked;

	/** STORE rich card: BUY pressed (bWatts picks currency). Owner -> Wallet->ClientRequestPurchase. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Store")
	FOnAFLStoreTileBuy OnBuyClicked;
	/** STORE rich card: EQUIP pressed (owned item). Owner -> preview/equip on the hero. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Store")
	FOnAFLStoreTileEquip OnEquipClicked;

	/** Populate: store the axis + id, set the name label + EQUIPPED badge, show the ShopThumbnail as the tile's
	 *  PRODUCT IMAGE (render or swatch), and tint the SwatchChip fallback for color axes with no thumbnail. */
	void SetTileData(EAFLLoadoutAxis InAxis, FName InCosmeticId, const FText& InDisplayName, bool bInEquipped, bool bInIsSwatch, FLinearColor InSwatchColor, const TSoftObjectPtr<UTexture2D>& InThumbnail);

	/** FRONT-END LOADOUT card style (store parity): reveal the rarity frame + the EQUIP button on an owned tile.
	 *  Call AFTER SetTileData. bEquipViaTileClick=true (in-match locker) routes the EQUIP button through
	 *  OnTileClicked (the locker's tile-click handler equips it); false (front-end market) routes it through
	 *  OnEquipClicked so the market can split SELECT (tile-body click = preview) from COMMIT (EQUIP button). */
	void ApplyLoadoutCardStyle(bool bEquipped, bool bEquipViaTileClick = true);

	/** FRONT-END SUGGESTED card style: reveal the rarity frame + dual-color price + BUY (no EQUIP) on an UNOWNED,
	 *  buyable tile -- reuses the store card's widgets. Call AFTER SetTileData (which collapsed them). */
	void ApplyPurchasableCardStyle(FName InCosmeticId);

	FName GetCosmeticId() const { return CosmeticId; }

protected:
	virtual void NativeOnInitialized() override;

	//~IUserObjectListEntry -- render OUR tile inside the front-end market's ListView (reads a UAFLMarketLoadoutItem).
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UButton> SelectButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UWidget> EquippedBadge;

	/** PRODUCT IMAGE -- the entry's ShopThumbnail (render or swatch); the image IS the tile visual. Optional so
	 *  the Inc-1 WBP still binds; supersedes SwatchChip when a thumbnail resolves (261/261 SKUs have one). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UImage> ProductImage;

	/** Color-swatch chip (color axes: body/edge/beam) -- FALLBACK tint when no ShopThumbnail resolves. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UBorder> SwatchChip;

	// --- STORE rich-card widgets (B2) -- BindWidgetOptional so the loadout/locker WBP omits them (they stay
	// collapsed via SetTileData); the STORE render path binds + shows + fills them from the catalog. ---
	/** Rarity-colored frame (GetRarityColor). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UBorder> RarityFrame;
	/** Dual price "990 V / 9,900 W" (GetEntryPriceText) or "OWNED" when the player already owns it. Plain fallback:
	 *  used only when the WBP has no PriceRichText (below). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PriceText;
	/** Dual-COLOR price (RichText): amounts white, "V" electric-blue #1E5AFF, "W" electric-magenta #FF2D9E, "/"
	 *  grey -- styled by DT_AFL_PriceStyles (set in NativeOnInitialized). The store path builds the tagged markup
	 *  from PriceVolts/PriceWatts + hides the plain PriceText; loadout/locker keep both collapsed. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<URichTextBlock> PriceRichText;

	// --- STORE buy/equip actions (B2c/d). BindWidgetOptional -> only the store WBP binds them; on the loadout/
	// locker WBP they are null (SetTileData collapses them). The STORE render path shows/labels/flips them. ---
	/** Volts BUY (MI_AFL_Button_Volts brush). Label -> "BUY  {volts} V". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> BuyButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BuyLabel;
	/** Watts BUY (MI_AFL_Button_Watts brush) -- shown only on SPARK (dual-priced) items. Label -> "BUY  {watts} W". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> BuyAltButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BuyAltLabel;
	/** EQUIP -- shown only when the item is OWNED (arc-violet treatment). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> EquipButton;

	UFUNCTION()
	void HandleButtonClicked();
	UFUNCTION() void HandleBuyClicked();
	UFUNCTION() void HandleBuyAltClicked();
	UFUNCTION() void HandleEquipClicked();

private:
	EAFLLoadoutAxis Axis = EAFLLoadoutAxis::Weapon;
	FName CosmeticId;

	/** Locker card mode (set by ApplyLoadoutCardStyle): the EQUIP button equips via the axis-carrying OnTileClicked
	 *  (the locker's existing handler) instead of the store's OnEquipClicked -> no new locker handler needed. */
	bool bEquipUsesTileClick = false;
};

/**
 * UAFLW_LoadoutSectionHeader -- the thin electric-glass header row between loadout zones. The C++ base reads the
 * UAFLMarketSectionHeader label; the WBP child (WBP_AFL_LoadoutSectionHeader) owns the layout (BindWidget:
 * HeaderLabel). Non-interactive -- it only segments the list into EQUIPPED / OWNED / SUGGESTED.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_LoadoutSectionHeader : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	//~IUserObjectListEntry -- read the UAFLMarketSectionHeader label into the caption.
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> HeaderLabel;
};
