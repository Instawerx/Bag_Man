// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "AFLW_LoadoutTileBase.generated.h"

class UButton;
class UTextBlock;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAFLLoadoutTileClicked, FName, CosmeticId);

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

	/** Populate: store the id, set the name label, show/hide the EQUIPPED badge. */
	void SetTileData(FName InCosmeticId, const FText& InDisplayName, bool bInEquipped);

	FName GetCosmeticId() const { return CosmeticId; }

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UButton> SelectButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UWidget> EquippedBadge;

	UFUNCTION()
	void HandleButtonClicked();

private:
	FName CosmeticId;
};
