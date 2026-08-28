// AFL kit (C2, I-4/I-9/I-25, AFL-3221): the catalog-filtered part list. EXTENDS the proven market
// surfaces -- the tiles ARE UAFLW_LoadoutTileBase (WBP_AFL_LoadoutTile), the list is the same plain
// UListView the market drives, the items are the market's UAFLMarketLoadoutItem payloads.
// Selecting a tile only broadcasts -- the shell routes it to preview (BeginPreview-only law);
// nothing here commits, buys, or writes a selection.

#pragma once

#include "CommonUserWidget.h"
#include "AFLCosmeticCoreTypes.h"       // EAFLCosmeticType (catalog taxonomy)
#include "UI/AFLW_LoadoutTileBase.h"    // EAFLLoadoutAxis + the tile + UAFLMarketLoadoutItem
#include "AFLW_PartPicker.generated.h"

class UListView;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAFLPartSelected, EAFLLoadoutAxis, Axis, FName, CosmeticId);

UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_PartPicker : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** A tile was clicked. The shell binds this to the preview seam -- preview only, no commit. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|PartPicker")
	FOnAFLPartSelected OnPartSelected;

	/**
	 * Populate from the catalog SSOT: every entry of CatalogType, badged owned/priced against the
	 * local wallet (I-9: everyone SEES everything; ownership gates elsewhere). EquippedId marks the
	 * current pick. ListAxis is the loadout axis the ids resolve into on click.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|PartPicker")
	void SetCatalogFilter(EAFLCosmeticType CatalogType, EAFLLoadoutAxis ListAxis, FName EquippedId);

protected:
	virtual void NativeOnInitialized() override;

	/** The proven tile (extend-not-rebuild). The kit WBP points this at WBP_AFL_LoadoutTile or a
	 *  restyled child of it -- never a new tile family. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|PartPicker")
	TSubclassOf<UAFLW_LoadoutTileBase> TileClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UListView> PartsListView;

private:
	UFUNCTION()
	void HandleTileClicked(EAFLLoadoutAxis Axis, FName CosmeticId);

	TSubclassOf<UUserWidget> GetEntryClassForItem(UObject* Item) const;
	void HandleEntryGenerated(UUserWidget& EntryWidget);
};
