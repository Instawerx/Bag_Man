// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h" // front-end market renders OUR tile as a ListView entry

#include "AFLW_LoadoutTileBase.generated.h"

class UButton;
class UTextBlock;
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
	bool bIsSwatch = false;
	FLinearColor SwatchColor = FLinearColor::White;
	TSoftObjectPtr<UTexture2D> Thumbnail;
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

	/** Populate: store the axis + id, set the name label + EQUIPPED badge, show the ShopThumbnail as the tile's
	 *  PRODUCT IMAGE (render or swatch), and tint the SwatchChip fallback for color axes with no thumbnail. */
	void SetTileData(EAFLLoadoutAxis InAxis, FName InCosmeticId, const FText& InDisplayName, bool bInEquipped, bool bInIsSwatch, FLinearColor InSwatchColor, const TSoftObjectPtr<UTexture2D>& InThumbnail);

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

	UFUNCTION()
	void HandleButtonClicked();

private:
	EAFLLoadoutAxis Axis = EAFLLoadoutAxis::Weapon;
	FName CosmeticId;
};
