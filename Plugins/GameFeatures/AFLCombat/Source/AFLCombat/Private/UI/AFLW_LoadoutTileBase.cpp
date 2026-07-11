// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_LoadoutTileBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/Border.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_LoadoutTileBase)

void UAFLW_LoadoutTileBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutTileBase::HandleButtonClicked);
	}
}

void UAFLW_LoadoutTileBase::SetTileData(EAFLLoadoutAxis InAxis, FName InCosmeticId, const FText& InDisplayName, bool bInEquipped, bool bInIsSwatch, FLinearColor InSwatchColor)
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
