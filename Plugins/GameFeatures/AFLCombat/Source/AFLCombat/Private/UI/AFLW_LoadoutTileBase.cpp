// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_LoadoutTileBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_LoadoutTileBase)

void UAFLW_LoadoutTileBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &UAFLW_LoadoutTileBase::HandleButtonClicked);
	}
}

void UAFLW_LoadoutTileBase::SetTileData(FName InCosmeticId, const FText& InDisplayName, bool bInEquipped)
{
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
}

void UAFLW_LoadoutTileBase::HandleButtonClicked()
{
	OnTileClicked.Broadcast(CosmeticId);
}
