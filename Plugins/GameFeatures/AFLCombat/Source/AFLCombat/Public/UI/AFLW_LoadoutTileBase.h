// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "AFLW_LoadoutTileBase.generated.h"

class UButton;
class UTextBlock;
class UWidget;

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
 * UAFLW_LoadoutTileBase -- one OWNED-cosmetic tile in the locker's AxisPicker grid.
 *
 * The C++ base owns the data + the click; the WBP child owns the layout (BindWidget: SelectButton +
 * NameText + optional EquippedBadge). The locker (UAFLW_LoadoutBase) spawns these into its TileContainer,
 * binds OnTileClicked, and calls SetTileData -- the proven UAFLW_MatchScoreboard row-spawn pattern (C++
 * owns bindings, WBP owns layout), so the WBP carries zero graph.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_LoadoutTileBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Broadcast on click, carrying this tile's CosmeticId (the locker binds this -> EquipForAxis). */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Loadout")
	FOnAFLLoadoutTileClicked OnTileClicked;

	/** Populate: store the axis + id, set the name label, show/hide the EQUIPPED badge. */
	void SetTileData(EAFLLoadoutAxis InAxis, FName InCosmeticId, const FText& InDisplayName, bool bInEquipped);

	FName GetCosmeticId() const { return CosmeticId; }

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UButton> SelectButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UWidget> EquippedBadge;

	UFUNCTION()
	void HandleButtonClicked();

private:
	EAFLLoadoutAxis Axis = EAFLLoadoutAxis::Weapon;
	FName CosmeticId;
};
