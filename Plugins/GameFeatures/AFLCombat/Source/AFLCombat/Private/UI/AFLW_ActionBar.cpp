#include "UI/AFLW_ActionBar.h"

#include "CommonButtonBase.h"
#include "Components/TextBlock.h"

void UAFLW_ActionBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// UCommonButtonBase click idiom: the non-dynamic OnClicked() with AddUObject -- never
	// AddDynamic (house rule, AFLW_Lobby_DetailPanel precedent).
	if (Btn_Save)
	{
		// Guarded on state, not only on button enablement -- a stray input path must not commit.
		Btn_Save->OnClicked().AddWeakLambda(this, [this]() { if (bSaveEnabled) { OnSaveClicked.Broadcast(); } });
	}
	if (Btn_Revert)
	{
		Btn_Revert->OnClicked().AddWeakLambda(this, [this]() { OnRevertClicked.Broadcast(); });
	}
	if (Btn_Equip)
	{
		Btn_Equip->OnClicked().AddWeakLambda(this, [this]() { OnEquipClicked.Broadcast(); });
	}
	if (Btn_Buy)
	{
		Btn_Buy->OnClicked().AddWeakLambda(this, [this]() { OnBuyClicked.Broadcast(); });
	}

	ApplyContext();
}

void UAFLW_ActionBar::SetContext(const EAFLActionBarContext InContext)
{
	Context = InContext;
	ApplyContext();
}

void UAFLW_ActionBar::SetSaveEnabled(const bool bEnabled, const FText& DisabledReason)
{
	bSaveEnabled = bEnabled;
	SaveDisabledReason = DisabledReason;
	ApplyContext();
}

void UAFLW_ActionBar::SetBuyPrice(const FText& PriceText)
{
	if (Txt_BuyPrice)
	{
		Txt_BuyPrice->SetText(PriceText);
	}
}

void UAFLW_ActionBar::ApplyContext()
{
	const bool bCreator = (Context == EAFLActionBarContext::CreatorClean
		|| Context == EAFLActionBarContext::CreatorDirty
		|| Context == EAFLActionBarContext::SavedBuildSelected);
	const bool bDirty = (Context == EAFLActionBarContext::CreatorDirty);
	const bool bSaved = (Context == EAFLActionBarContext::SavedBuildSelected);
	const bool bStore = (Context == EAFLActionBarContext::StoreItem);

	if (Btn_Save)
	{
		Btn_Save->SetVisibility(bCreator ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		// Enabled = state AND context: a clean creator has nothing to save even for an entitled player.
		Btn_Save->SetIsEnabled(bSaveEnabled && bDirty);
	}
	if (Btn_Revert)
	{
		// I-7: revert-to-saved, always available WHILE DIRTY -- hidden otherwise, never disabled-grey.
		Btn_Revert->SetVisibility(bDirty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Btn_Equip)
	{
		Btn_Equip->SetVisibility(bSaved ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Btn_Buy)
	{
		Btn_Buy->SetVisibility(bStore ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Txt_Reason)
	{
		// The reason shows exactly when Save is visible-but-gated (I-14's "save/carry gates").
		const bool bShowReason = bCreator && bDirty && !bSaveEnabled && !SaveDisabledReason.IsEmpty();
		Txt_Reason->SetText(SaveDisabledReason);
		Txt_Reason->SetVisibility(bShowReason ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Txt_BuyPrice)
	{
		Txt_BuyPrice->SetVisibility(bStore ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
