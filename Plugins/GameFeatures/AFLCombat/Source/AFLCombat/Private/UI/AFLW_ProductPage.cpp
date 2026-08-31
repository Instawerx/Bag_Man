// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_ProductPage.h"

#include "AFLCombat.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Font.h"
#include "CommonUIExtensions.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/LyraPlayerState.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "UI/AFLW_FrontEndMarket.h"
#include "UI/AFLW_PurchaseConfirm.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_ProductPage)

namespace AFLProductPage
{
	static const FLinearColor Surface(0.004f, 0.006f, 0.02f, 0.94f);   // glass over the world
	static const FLinearColor Accent(0.013f, 0.102f, 1.0f, 1.0f);      // UI.House.Electric
	static const FLinearColor Good(0.16f, 0.85f, 0.35f, 1.0f);
	static const FLinearColor Bad(1.0f, 0.16f, 0.22f, 1.0f);
	static const FLinearColor Dim(1.0f, 1.0f, 1.0f, 0.55f);

	static UFont* Orbitron() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/Orbitron.Orbitron")); }
	static UFont* NotoSans() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/NotoSans.NotoSans")); }

	static void Style(UTextBlock* T, UFont* F, float Size, const FLinearColor& C)
	{
		if (!T) return;
		FSlateFontInfo Info(F, static_cast<int32>(Size));
		T->SetFont(Info);
		T->SetColorAndOpacity(FSlateColor(C));
	}
}

UAFLW_ProductPage::UAFLW_ProductPage()
{
	// LAP-2 FIX (operator: "could not close or back out... had to stop PIE"): the back action existed
	// but this flag was never set, so CommonUI never ROUTED ESC/B here -- the page was a trap. The
	// HubGateCards lesson, finally applied.
	bIsBackHandler = true;
}

TSharedRef<SWidget> UAFLW_ProductPage::RebuildWidget()
{
	using namespace AFLProductPage;
	UFont* Display = Orbitron();
	UFont* Body = NotoSans();

	// [ spacer 40% (the live world) | glass panel 60% ] -- the ruled non-obtrusive overlay.
	UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PageRoot"));
	WidgetTree->RootWidget = Root;

	USpacer* WorldGap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("WorldGap"));
	if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(WorldGap))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->Size.Value = 0.4f;
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(Surface);
	Panel->SetPadding(FMargin(28.f, 22.f));
	if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(Panel))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->Size.Value = 0.6f;
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	Panel->SetContent(Col);

	auto AddText = [&](const TCHAR* Name, UFont* F, float Size, const FLinearColor& C, float TopPad) -> UTextBlock*
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Style(T, F, Size, C);
		T->SetAutoWrapText(true);
		if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(T))
		{
			VS->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		}
		return T;
	};

	// Explicit ✕ exit for the mouse (ESC works too now, but a visible way out is non-negotiable).
	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	CloseButton->SetBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f));
	UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
	Style(CloseLabel, Display, 12.f, FLinearColor(1.f, 1.f, 1.f, 0.8f));
	CloseLabel->SetText(NSLOCTEXT("AFLRetail", "PageClose", "✕  CLOSE"));
	CloseButton->AddChild(CloseLabel);
	CloseButton->OnClicked.AddDynamic(this, &UAFLW_ProductPage::HandleCloseClicked);
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(CloseButton))
	{
		VS->SetHorizontalAlignment(HAlign_Right);
	}

	NameText  = AddText(TEXT("NameText"), Display, 22.f, FLinearColor::White, 0.f);
	TierText  = AddText(TEXT("TierText"), Display, 11.f, Accent, 6.f);
	PriceText = AddText(TEXT("PriceText"), Display, 16.f, Accent, 14.f);
	DescText  = AddText(TEXT("DescText"), Body, 13.f, Dim, 14.f);
	StatusText = AddText(TEXT("StatusText"), Body, 12.f, Dim, 14.f);

	USpacer* Flex = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("Flex"));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Flex))
	{
		VS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	auto AddButton = [&](const TCHAR* Name, const TCHAR* LabelName, const FLinearColor& Fill,
	                     TObjectPtr<UTextBlock>& OutLabel, float TopPad) -> UButton*
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		B->SetBackgroundColor(Fill);
		OutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		Style(OutLabel, Display, 15.f, FLinearColor::White);
		B->AddChild(OutLabel);
		if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(B))
		{
			VS->SetPadding(FMargin(0.f, TopPad, 0.f, 0.f));
		}
		return B;
	};

	BuyButton = AddButton(TEXT("BuyButton"), TEXT("BuyLabel"), Accent, BuyLabel, 10.f);
	BuyButton->OnClicked.AddDynamic(this, &UAFLW_ProductPage::HandleBuyClicked);
	EquipButton = AddButton(TEXT("EquipButton"), TEXT("EquipLabel"), FLinearColor(1.f, 1.f, 1.f, 0.10f), EquipLabel, 8.f);
	EquipButton->OnClicked.AddDynamic(this, &UAFLW_ProductPage::HandleEquipClicked);

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
	Style(Hint, Body, 10.f, FLinearColor(1.f, 1.f, 1.f, 0.35f));
	Hint->SetText(NSLOCTEXT("AFLRetail", "PageHint", "ESC · back to the floor"));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Hint))
	{
		VS->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	}

	return Super::RebuildWidget();
}

void UAFLW_ProductPage::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshFromCatalog();
}

bool UAFLW_ProductPage::NativeOnHandleBackAction()
{
	DeactivateWidget();
	return true;
}

void UAFLW_ProductPage::HandleCloseClicked()
{
	DeactivateWidget();
}

void UAFLW_ProductPage::FocusCosmeticId(FName InCosmeticId)
{
	CosmeticId = InCosmeticId;
	RefreshFromCatalog();
}

void UAFLW_ProductPage::RefreshFromCatalog()
{
	using namespace AFLProductPage;
	if (!NameText || CosmeticId.IsNone())
	{
		return;
	}
	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetWorld());
	const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(CosmeticId) : nullptr;
	if (!Entry)
	{
		NameText->SetText(FText::FromName(CosmeticId));
		SetStatus(NSLOCTEXT("AFLRetail", "NoRow", "Row not in the catalog."), Bad);
		BuyButton->SetIsEnabled(false);
		EquipButton->SetIsEnabled(false);
		return;
	}
	NameText->SetText(Entry->DisplayName.IsEmpty() ? FText::FromName(CosmeticId) : Entry->DisplayName);
	TierText->SetText(FText::Format(NSLOCTEXT("AFLRetail", "TierFmt", "{0}  ·  {1}"),
		FText::FromString(StaticEnum<EAFLCosmeticTier>()->GetNameStringByValue((int64)Entry->Tier)),
		FText::FromString(StaticEnum<EAFLCosmeticType>()->GetNameStringByValue((int64)Entry->Type))));
	PriceText->SetText(Catalog->GetEntryPriceText(*Entry));
	DescText->SetText(Entry->Description);

	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	const ALyraPlayerState* LyraPS = Cast<ALyraPlayerState>(PS);
	const bool bEntitled = Wallet && LyraPS && Wallet->IsEntitled(LyraPS, CosmeticId);
	const bool bCounted = !Entry->CountedKey.IsNone(); // counted SKUs stay re-buyable (D-7)

	EAFLLoadoutAxis Axis;
	const bool bEquippable = UAFLW_FrontEndMarket::ClassifyStoreAxis(CosmeticId, Axis);

	if (bEntitled && !bCounted)
	{
		BuyButton->SetIsEnabled(false); // D-7 grey-out
		BuyLabel->SetText(NSLOCTEXT("AFLRetail", "Owned", "OWNED ✓"));
		SetStatus(NSLOCTEXT("AFLRetail", "InLocker", "In your locker."), Good);
	}
	else if (!Entry->bTransactable)
	{
		BuyButton->SetIsEnabled(false);
		BuyLabel->SetText(NSLOCTEXT("AFLRetail", "NotForSale", "GRANTED · NOT FOR SALE"));
		SetStatus(FText::GetEmpty(), Dim);
	}
	else
	{
		BuyButton->SetIsEnabled(true);
		BuyLabel->SetText(FText::Format(NSLOCTEXT("AFLRetail", "BuyFmt", "BUY · {0}"), Catalog->GetEntryPriceText(*Entry)));
		SetStatus(FText::GetEmpty(), Dim);
	}
	EquipButton->SetIsEnabled(bEntitled && bEquippable);
	EquipLabel->SetText(bEquippable
		? NSLOCTEXT("AFLRetail", "Equip", "EQUIP")
		: NSLOCTEXT("AFLRetail", "NoEquip", "USED FROM THE CREATOR"));
}

void UAFLW_ProductPage::SetStatus(const FText& Text, const FLinearColor& Color)
{
	if (StatusText)
	{
		StatusText->SetText(Text);
		StatusText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UAFLW_ProductPage::HandleBuyClicked()
{
	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetWorld());
	const FAFLCatalogEntry* Entry = Catalog ? Catalog->FindEntry(CosmeticId) : nullptr;
	if (!Entry)
	{
		return;
	}
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		return;
	}
	UCommonActivatableWidget* Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
		FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Modal")),
		TSubclassOf<UCommonActivatableWidget>(UAFLW_PurchaseConfirm::StaticClass()));
	if (UAFLW_PurchaseConfirm* Confirm = Cast<UAFLW_PurchaseConfirm>(Pushed))
	{
		Confirm->Configure(NameText->GetText(), Catalog->GetEntryPriceText(*Entry));
		Confirm->Resolved.AddUniqueDynamic(this, &UAFLW_ProductPage::HandleConfirmResolved);
	}
}

void UAFLW_ProductPage::HandleConfirmResolved(bool bConfirmed)
{
	using namespace AFLProductPage;
	if (!bConfirmed)
	{
		SetStatus(NSLOCTEXT("AFLRetail", "Cancelled", "Cancelled — wallet untouched."), Dim);
		return;
	}
	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	if (!Wallet)
	{
		SetStatus(NSLOCTEXT("AFLRetail", "NoWallet", "Wallet unavailable — nothing charged."), Bad);
		return;
	}
	// Same split the market chassis ships: PlayFab in shipping, the dev grant path in editor/PIE.
#if UE_BUILD_SHIPPING
	Wallet->ClientRequestPurchase(CosmeticId);
#else
	Wallet->ServerPurchaseCosmetic(CosmeticId);
#endif
	SetStatus(NSLOCTEXT("AFLRetail", "Purchasing", "Purchasing…"), Dim);
	bAwaitingGrant = true;
	GrantPolls = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(GrantPollTimer, this, &UAFLW_ProductPage::PollGrant, 0.5f, true);
	}
}

void UAFLW_ProductPage::PollGrant()
{
	using namespace AFLProductPage;
	++GrantPolls;
	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	const ALyraPlayerState* LyraPS = Cast<ALyraPlayerState>(PS);
	const bool bOwned = Wallet && LyraPS && Wallet->IsEntitled(LyraPS, CosmeticId);
	if (bOwned || GrantPolls > 12)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(GrantPollTimer);
		}
		bAwaitingGrant = false;
		if (bOwned)
		{
			SetStatus(NSLOCTEXT("AFLRetail", "Granted", "GRANTED — in your locker. EQUIP is live."), Good);
		}
		else
		{
			SetStatus(NSLOCTEXT("AFLRetail", "Refused", "Purchase refused — wallet untouched (funds or availability)."), Bad);
		}
		RefreshFromCatalog();
	}
}

void UAFLW_ProductPage::HandleEquipClicked()
{
	using namespace AFLProductPage;
	EAFLLoadoutAxis Axis;
	if (!UAFLW_FrontEndMarket::ClassifyStoreAxis(CosmeticId, Axis))
	{
		return;
	}
	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAFLCosmeticLoadoutComponent* Loadout = PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		SetStatus(NSLOCTEXT("AFLRetail", "NoLoadout", "Loadout unavailable."), Bad);
		return;
	}
	FAFLCosmeticSelection Sel = Loadout->GetSelection();
	switch (Axis)
	{
	case EAFLLoadoutAxis::Weapon:   Sel.WeaponId = CosmeticId; break;
	case EAFLLoadoutAxis::Facemask: Sel.FacemaskId = CosmeticId; break;
	case EAFLLoadoutAxis::Emblem:   Sel.EmblemId = CosmeticId; break;
	case EAFLLoadoutAxis::Beam:     Sel.BeamId = CosmeticId; break;
	default:
		SetStatus(NSLOCTEXT("AFLRetail", "CreatorAxis", "This equips from the Creator."), Dim);
		return;
	}
	Loadout->ServerSetCosmeticSelection(Sel);
	SetStatus(NSLOCTEXT("AFLRetail", "Equipped", "EQUIPPED — walking out wearing it."), Good);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_RETAIL: EQUIP '%s' via the selection seam."), *CosmeticId.ToString());
}
