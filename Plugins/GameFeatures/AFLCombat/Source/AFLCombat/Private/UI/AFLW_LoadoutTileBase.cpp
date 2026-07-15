// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_LoadoutTileBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "AFLCosmeticCatalogSubsystem.h" // STORE tile: resolve CosmeticId -> DisplayName + ShopThumbnail (SSOT)
#include "AFLCosmeticCoreTypes.h"        // FAFLCatalogEntry
#include "UObject/UnrealType.h"          // FNameProperty / CastField -- read CosmeticId off the store's BP item

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_LoadoutTileBase)

void UAFLW_LoadoutTileBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutTileBase::HandleButtonClicked);
	}
}

void UAFLW_LoadoutTileBase::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// LOADOUT path: the ListView hands us a UAFLMarketLoadoutItem -> drive the SAME SetTileData the in-match locker
	// calls directly. Both routes share one render; the SelectButton still owns the click.
	if (const UAFLMarketLoadoutItem* It = Cast<UAFLMarketLoadoutItem>(ListItemObject))
	{
		SetTileData(It->Axis, It->CosmeticId, It->DisplayName, It->bEquipped, It->bIsSwatch, It->SwatchColor, It->Thumbnail);
		return;
	}

	// STORE path: the ListView hands us the store's BP data object (BP_AFL_StoreEntryData). C++ knows only its
	// 'CosmeticId' FName (reflected -- mirrors UAFLW_FrontEndMarket::ReadEntryCosmeticId); resolve the display
	// (name + ShopThumbnail) from the catalog (the SSOT) so OUR tile renders it. The click stays our SelectButton
	// -> OnTileClicked, so the market can drive a reliable selection (the reason we swap in this tile: the store's
	// BP tile's BUY/EQUIP buttons ate the row click, so no second item could ever be browsed).
	FName StoreId = NAME_None;
	if (ListItemObject)
	{
		if (const FNameProperty* Prop = CastField<FNameProperty>(ListItemObject->GetClass()->FindPropertyByName(TEXT("CosmeticId"))))
		{
			StoreId = Prop->GetPropertyValue_InContainer(ListItemObject);
		}
	}
	if (StoreId.IsNone())
	{
		return;
	}
	FText Name = FText::FromName(StoreId);
	TSoftObjectPtr<UTexture2D> Thumb;
	if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
	{
		if (const FAFLCatalogEntry* Entry = Catalog->FindEntry(StoreId))
		{
			if (!Entry->DisplayName.IsEmpty()) { Name = Entry->DisplayName; }
			Thumb = Entry->ShopThumbnail;
		}
	}
	// Axis is irrelevant for a store tile (the market re-derives it from the CosmeticId on selection); pass a
	// placeholder. bEquipped=false: the store's own detail panel owns the OWNED/BUY state in this increment.
	SetTileData(EAFLLoadoutAxis::Weapon, StoreId, Name, /*bEquipped*/ false, /*bIsSwatch*/ false, FLinearColor::White, Thumb);
}

void UAFLW_LoadoutTileBase::SetTileData(EAFLLoadoutAxis InAxis, FName InCosmeticId, const FText& InDisplayName, bool bInEquipped, bool bInIsSwatch, FLinearColor InSwatchColor, const TSoftObjectPtr<UTexture2D>& InThumbnail)
{
	Axis = InAxis;
	CosmeticId = InCosmeticId;

	if (NameText)
	{
		NameText->SetText(InDisplayName);
	}
	if (EquippedBadge)
	{
		// HitTestInvisible so the badge never eats the tile's click.
		EquippedBadge->SetVisibility(bInEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (SwatchChip)
	{
		// Color axes (body/edge/beam) render as a tinted chip; other axes hide it (name/thumbnail tile).
		SwatchChip->SetVisibility(bInIsSwatch ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bInIsSwatch)
		{
			SwatchChip->SetBrushColor(InSwatchColor);
		}
	}
	if (ProductImage)
	{
		// PRODUCT IMAGE: the entry's ShopThumbnail (render or swatch) IS the tile visual. Sync-load -- small UI
		// textures; a one-time menu-open hitch is acceptable for Inc 1 (async-upgrade if it bites). Supersedes the
		// SwatchChip. 261/261 SKUs carry a thumbnail, so this is the live path; a no-thumb tile falls back to swatch/name.
		UTexture2D* Thumb = InThumbnail.IsNull() ? nullptr : InThumbnail.LoadSynchronous();
		if (Thumb)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Thumb);
			Brush.ImageSize = FVector2D(112.f, 112.f); // uniform product-card tile size (a fresh UImage brush is 32x32)
			ProductImage->SetBrush(Brush);
			ProductImage->SetVisibility(ESlateVisibility::HitTestInvisible); // never eats the tile click
			if (SwatchChip)
			{
				SwatchChip->SetVisibility(ESlateVisibility::Collapsed); // the image supersedes the chip
			}
		}
		else
		{
			ProductImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (SelectButton)
	{
		// Selected-fill: electric-blue #1E5AFF on the EQUIPPED tile, dark glass otherwise (UI Style SSOT).
		SelectButton->SetBackgroundColor(bInEquipped
			? FLinearColor(0.013f, 0.102f, 1.0f, 0.92f)
			: FLinearColor(0.02f, 0.05f, 0.14f, 0.88f));
	}
}

void UAFLW_LoadoutTileBase::HandleButtonClicked()
{
	OnTileClicked.Broadcast(Axis, CosmeticId);
}
