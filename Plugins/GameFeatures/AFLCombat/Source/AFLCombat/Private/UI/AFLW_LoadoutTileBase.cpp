// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_LoadoutTileBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

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
	// Front-end market path: the ListView hands us a UAFLMarketLoadoutItem -> drive the SAME SetTileData the
	// in-match locker calls directly. Both routes share one render; the SelectButton still owns the click.
	if (const UAFLMarketLoadoutItem* It = Cast<UAFLMarketLoadoutItem>(ListItemObject))
	{
		SetTileData(It->Axis, It->CosmeticId, It->DisplayName, It->bEquipped, It->bIsSwatch, It->SwatchColor, It->Thumbnail);
	}
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
