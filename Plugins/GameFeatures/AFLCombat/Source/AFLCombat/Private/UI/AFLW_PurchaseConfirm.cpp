// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_PurchaseConfirm.h"

#include "Blueprint/WidgetTree.h"
#include "Engine/Font.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_PurchaseConfirm)

TSharedRef<SWidget> UAFLW_PurchaseConfirm::RebuildWidget()
{
	static const FLinearColor Accent(0.013f, 0.102f, 1.0f, 1.0f);
	static const FLinearColor Surface(0.004f, 0.006f, 0.02f, 0.96f);
	UFont* Display = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/Orbitron.Orbitron"));
	UFont* Body = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/NotoSans.NotoSans"));

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ConfirmRoot"));
	WidgetTree->RootWidget = Root;

	// dim scrim
	UBorder* Scrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Scrim"));
	FSlateBrush ScrimBrush;
	ScrimBrush.DrawAs = ESlateBrushDrawType::Image;
	ScrimBrush.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Scrim->SetBrush(ScrimBrush);
	if (UOverlaySlot* S = Root->AddChildToOverlay(Scrim))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(Surface);
	Panel->SetPadding(FMargin(26.f, 22.f));
	if (UOverlaySlot* S = Root->AddChildToOverlay(Panel))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	Panel->SetContent(Col);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetFont(FSlateFontInfo(Display, 12));
	Title->SetColorAndOpacity(FSlateColor(Accent));
	Title->SetText(NSLOCTEXT("AFLRetail", "ConfirmTitle", "CONFIRM PURCHASE"));
	Col->AddChildToVerticalBox(Title);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	NameText->SetFont(FSlateFontInfo(Display, 18));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(NameText)) { VS->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f)); }

	PriceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Price"));
	PriceText->SetFont(FSlateFontInfo(Body, 14));
	PriceText->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.73f, 1.f, 1.f)));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(PriceText)) { VS->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f)); }

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Row)) { VS->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f)); }

	auto MakeBtn = [&](const TCHAR* Name, const FText& Label, const FLinearColor& Fill) -> UButton*
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		B->SetBackgroundColor(Fill);
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetFont(FSlateFontInfo(Display, 13));
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		T->SetText(Label);
		B->AddChild(T);
		if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(B))
		{
			HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HS->SetPadding(FMargin(4.f, 0.f));
		}
		return B;
	};
	UButton* Confirm = MakeBtn(TEXT("ConfirmBtn"), NSLOCTEXT("AFLRetail", "Confirm", "CONFIRM"), Accent);
	Confirm->OnClicked.AddDynamic(this, &UAFLW_PurchaseConfirm::HandleConfirm);
	UButton* Cancel = MakeBtn(TEXT("CancelBtn"), NSLOCTEXT("AFLRetail", "Cancel", "CANCEL"), FLinearColor(1.f, 1.f, 1.f, 0.10f));
	Cancel->OnClicked.AddDynamic(this, &UAFLW_PurchaseConfirm::HandleCancel);

	return Super::RebuildWidget();
}

void UAFLW_PurchaseConfirm::Configure(const FText& ProductName, const FText& PriceLine)
{
	if (NameText) { NameText->SetText(ProductName); }
	if (PriceText) { PriceText->SetText(PriceLine); }
}

bool UAFLW_PurchaseConfirm::NativeOnHandleBackAction()
{
	Fire(false);
	return true;
}

void UAFLW_PurchaseConfirm::HandleConfirm() { Fire(true); }
void UAFLW_PurchaseConfirm::HandleCancel() { Fire(false); }

void UAFLW_PurchaseConfirm::Fire(bool bConfirmed)
{
	if (bFired)
	{
		return;
	}
	bFired = true;
	Resolved.Broadcast(bConfirmed);
	DeactivateWidget();
}
