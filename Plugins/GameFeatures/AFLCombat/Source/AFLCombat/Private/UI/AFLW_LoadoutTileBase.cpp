// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_LoadoutTileBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h" // dual-COLOR price -> tagged runs styled by DT_AFL_PriceStyles
#include "Components/Widget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/DataTable.h"          // LoadObject<UDataTable> -> the RichText style set
#include "AFLCosmeticCatalogSubsystem.h" // STORE tile: resolve CosmeticId -> DisplayName + ShopThumbnail (SSOT)
#include "AFLCosmeticCoreTypes.h"        // FAFLCatalogEntry
#include "UObject/UnrealType.h"          // FNameProperty / CastField -- read CosmeticId off the store's BP item
#include "Cosmetics/AFLWalletComponent.h" // OwnsCosmetic -> OWNED vs price (B2b)
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_LoadoutTileBase)

namespace
{
	// The RichText style set (Default=white, volts=#1E5AFF, watts=#FF2D9E, sep=grey), authored + verified in the
	// editor. Loaded once and pushed onto PriceRichText so the tagged runs colorize.
	const TCHAR* GAFLPriceStyleSetPath = TEXT("/Game/BagMan/UI/Store/DT_AFL_PriceStyles.DT_AFL_PriceStyles");

	// Build the color-tagged price markup from the raw catalog numbers (grouped thousands via FText::AsNumber):
	//   dual  -> "3,990 <volts>V</>  <sep>/</>  39,900 <watts>W</>"   (amounts untagged -> Default/white)
	//   volts -> "16,000 <volts>V</>"      watts-only -> "100,000 <watts>W</>"      no price -> "Free".
	FText AFLBuildPriceMarkup(const FAFLCatalogEntry& Entry)
	{
		const bool bHasV = (Entry.PriceVolts > 0);
		const bool bHasW = (Entry.PriceWatts > 0);
		if (!bHasV && !bHasW) { return NSLOCTEXT("AFLStore", "Price_Free", "Free"); }
		FString M;
		if (bHasV) { M += FString::Printf(TEXT("%s <volts>V</>"), *FText::AsNumber(Entry.PriceVolts).ToString()); }
		if (bHasV && bHasW) { M += TEXT("  <sep>/</>  "); }
		if (bHasW) { M += FString::Printf(TEXT("%s <watts>W</>"), *FText::AsNumber(Entry.PriceWatts).ToString()); }
		return FText::FromString(M);
	}
}

void UAFLW_LoadoutTileBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutTileBase::HandleButtonClicked);
	}
	if (PriceRichText)
	{
		// Style set drives the per-run colors; set it before any SetText so the markup colorizes on first fill.
		if (UDataTable* Styles = LoadObject<UDataTable>(nullptr, GAFLPriceStyleSetPath))
		{
			PriceRichText->SetTextStyleSet(Styles);
		}
	}
	if (BuyButton)    { BuyButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutTileBase::HandleBuyClicked); }
	if (BuyAltButton) { BuyAltButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutTileBase::HandleBuyAltClicked); }
	if (EquipButton)  { EquipButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutTileBase::HandleEquipClicked); }
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
	const FAFLCatalogEntry* Entry = nullptr;
	if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
	{
		Entry = Catalog->FindEntry(StoreId); // pointer into the resident catalog -> stays valid
		if (Entry)
		{
			if (!Entry->DisplayName.IsEmpty()) { Name = Entry->DisplayName; }
			Thumb = Entry->ShopThumbnail;
		}
	}
	// Axis is irrelevant for a store tile (the market re-derives it from the CosmeticId on selection); pass a
	// placeholder. SetTileData collapses the rich-card widgets -> the store path (below) fills + shows them.
	SetTileData(EAFLLoadoutAxis::Weapon, StoreId, Name, /*bEquipped*/ false, /*bIsSwatch*/ false, FLinearColor::White, Thumb);

	// Rich store card (B2a rarity frame + B2b dual price / OWNED). Only the store WBP binds RarityFrame/PriceText;
	// on the loadout/locker WBP they're null -> no-op. HitTestInvisible so they never eat the tile's SelectButton click.
	if (Entry)
	{
		// OWNED? -> drives BUY-vs-EQUIP + the "OWNED" tag.
		bool bOwned = false;
		if (const APlayerController* PC = GetOwningPlayer())
		{
			if (const APlayerState* PS = PC->PlayerState)
			{
				if (const UAFLWalletComponent* Wallet = PS->FindComponentByClass<UAFLWalletComponent>())
				{
					bOwned = Wallet->OwnsCosmetic(StoreId);
				}
			}
		}

		if (RarityFrame)
		{
			RarityFrame->SetBrushColor(UAFLCosmeticCatalogSubsystem::GetRarityColor(*Entry));
			RarityFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		// Reference card layout: the dual-price sits on its own line; the row shows BUY + EQUIP, both ALWAYS present
		// (BUY purchases, EQUIP previews on the hero). One BUY on the card (Volts); the Watts choice is the detail panel.
		// Dual-COLOR price via RichText (amounts white, V blue, W magenta, / grey). Plain PriceText is the fallback
		// only when the WBP has no PriceRichText bound.
		if (PriceRichText)
		{
			PriceRichText->SetText(AFLBuildPriceMarkup(*Entry));
			PriceRichText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (PriceText) { PriceText->SetVisibility(ESlateVisibility::Collapsed); }
		}
		else if (PriceText)
		{
			PriceText->SetText(UAFLCosmeticCatalogSubsystem::GetEntryPriceText(*Entry));
			PriceText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (BuyButton)
		{
			BuyButton->SetVisibility(ESlateVisibility::Visible);
			BuyButton->SetIsEnabled(!bOwned); // owned -> BUY greys out
		}
		if (BuyLabel) { BuyLabel->SetText(bOwned ? NSLOCTEXT("AFLStore", "Owned", "OWNED") : NSLOCTEXT("AFLStore", "Buy", "BUY")); }
		if (BuyAltButton) { BuyAltButton->SetVisibility(ESlateVisibility::Collapsed); } // one BUY on the card
		if (EquipButton)  { EquipButton->SetVisibility(ESlateVisibility::Visible); }
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
			Brush.ImageSize = FVector2D(64.f, 64.f); // ~64px thumbnail -> drives the ~72-80px tight card-row height
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
		// Card fill: near-black DARK GLASS (#05080F @ 0.90 -- SSOT UIPanel.Glass) so the neon pipes + prices IGNITE
		// against it and the bright armory doesn't bleed through. Equipped tile (loadout) keeps the electric-blue cue.
		SelectButton->SetBackgroundColor(bInEquipped
			? FLinearColor(0.013f, 0.102f, 1.0f, 0.92f)
			: FLinearColor(0.0196f, 0.0314f, 0.0588f, 0.90f));
	}
	// STORE rich-card widgets default HIDDEN: only the STORE render path (NativeOnListItemObjectSet) re-shows + fills
	// them. Loadout/locker tiles call SetTileData directly and leave them collapsed.
	if (RarityFrame)   { RarityFrame->SetVisibility(ESlateVisibility::Collapsed); }
	if (PriceText)     { PriceText->SetVisibility(ESlateVisibility::Collapsed); }
	if (PriceRichText) { PriceRichText->SetVisibility(ESlateVisibility::Collapsed); }
	if (BuyButton)    { BuyButton->SetVisibility(ESlateVisibility::Collapsed); }
	if (BuyAltButton) { BuyAltButton->SetVisibility(ESlateVisibility::Collapsed); }
	if (EquipButton)  { EquipButton->SetVisibility(ESlateVisibility::Collapsed); }
}

void UAFLW_LoadoutTileBase::HandleBuyClicked()
{
	OnBuyClicked.Broadcast(CosmeticId, /*bWatts*/ false);
}

void UAFLW_LoadoutTileBase::HandleBuyAltClicked()
{
	OnBuyClicked.Broadcast(CosmeticId, /*bWatts*/ true);
}

void UAFLW_LoadoutTileBase::HandleEquipClicked()
{
	OnEquipClicked.Broadcast(CosmeticId);
}

void UAFLW_LoadoutTileBase::HandleButtonClicked()
{
	OnTileClicked.Broadcast(Axis, CosmeticId);
}
