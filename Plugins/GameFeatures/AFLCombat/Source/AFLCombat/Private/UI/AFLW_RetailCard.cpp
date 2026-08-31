// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_RetailCard.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Retail/AFLRetailSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_RetailCard)

namespace AFLRetailCard
{
	// The product page's palette (one brand voice on every retail surface).
	static const FLinearColor Surface(0.004f, 0.006f, 0.02f, 0.92f);
	static const FLinearColor Accent(0.013f, 0.102f, 1.0f, 1.0f);
	static const FLinearColor Danger(1.0f, 0.16f, 0.22f, 0.75f);
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

TSharedRef<SWidget> UAFLW_RetailCard::RebuildWidget()
{
	using namespace AFLRetailCard;
	UFont* Display = Orbitron();
	UFont* Body = NotoSans();

	// Full-screen canvas; the card is ONE bottom-right-anchored glass panel (mock: 300px, corner).
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CardCanvas"));
	WidgetTree->RootWidget = Canvas;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CardPanel"));
	Panel->SetBrushColor(Surface);
	Panel->SetPadding(FMargin(17.f, 15.f));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Panel))
	{
		S->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
		S->SetAlignment(FVector2D(1.f, 1.f));
		S->SetPosition(FVector2D(-24.f, -24.f));
		S->SetAutoSize(true);
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CardCol"));
	Panel->SetContent(Col);

	// Header row: NAME ..... WEARING
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeadRow"));
	Col->AddChildToVerticalBox(Head);
	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	Style(NameText, Display, 18.f, FLinearColor::White);
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(NameText))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	WearText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WearText"));
	Style(WearText, Display, 12.f, Accent);
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(WearText))
	{
		S->SetPadding(FMargin(10.f, 3.f, 0.f, 0.f));
	}

	// Price · meta
	PriceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PriceText"));
	Style(PriceText, Display, 16.f, Accent);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(PriceText)) { S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f)); }
	MetaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MetaText"));
	Style(MetaText, Display, 10.f, Dim);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MetaText)) { S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f)); }

	// Action rows (key-first labels; clickable when a cursor exists).
	auto AddAction = [&](const TCHAR* Name, const FLinearColor& Fill, const FText& Label,
	                     TObjectPtr<UButton>& OutBtn, TObjectPtr<UTextBlock>* OutLabel)
	{
		OutBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		OutBtn->SetBackgroundColor(Fill);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(L, Display, 13.f, FLinearColor::White);
		L->SetText(Label);
		OutBtn->AddChild(L);
		if (OutLabel) { *OutLabel = L; }
		if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(OutBtn)) { S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f)); }
	};

	AddAction(TEXT("BuyButton"), Accent, NSLOCTEXT("AFLRetail", "CardBuy", "[F]  BUY"), BuyButton, &BuyLabel);
	BuyButton->OnClicked.AddDynamic(this, &UAFLW_RetailCard::HandleBuy);
	AddAction(TEXT("CartButton"), FLinearColor(1.f, 1.f, 1.f, 0.10f),
		NSLOCTEXT("AFLRetail", "CardCart", "[C]  ADD TO CART"), CartButton, nullptr);
	CartButton->OnClicked.AddDynamic(this, &UAFLW_RetailCard::HandleCart);
	AddAction(TEXT("DiscardButton"), Danger,
		NSLOCTEXT("AFLRetail", "CardDiscard", "[Q]  DISCARD"), DiscardButton, nullptr);
	DiscardButton->OnClicked.AddDynamic(this, &UAFLW_RetailCard::HandleDiscard);
	AddAction(TEXT("DetailsButton"), FLinearColor(1.f, 1.f, 1.f, 0.06f),
		NSLOCTEXT("AFLRetail", "CardDetails", "[E]  DETAILS  →"), DetailsButton, nullptr);
	DetailsButton->OnClicked.AddDynamic(this, &UAFLW_RetailCard::HandleDetails);

	// Status + the standing hint (mock: "leaving the venue restores your look").
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	Style(StatusText, Body, 12.f, Dim);
	StatusText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(StatusText)) { S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f)); }

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
	Style(Hint, Body, 10.f, FLinearColor(1.f, 1.f, 1.f, 0.30f));
	Hint->SetText(NSLOCTEXT("AFLRetail", "CardHint", "walking off the pad puts it back"));
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Hint)) { S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f)); }

	return Super::RebuildWidget();
}

void UAFLW_RetailCard::SetHeader(const FText& Name, const FText& Meta, const FText& Price)
{
	if (NameText) { NameText->SetText(Name); }
	if (MetaText) { MetaText->SetText(Meta); }
	if (PriceText) { PriceText->SetText(Price); }
}

void UAFLW_RetailCard::SetWearState(const FText& InState)
{
	if (WearText) { WearText->SetText(InState); }
}

void UAFLW_RetailCard::SetBuyRow(const FText& Label, bool bEnabled)
{
	if (BuyLabel) { BuyLabel->SetText(Label); }
	if (BuyButton) { BuyButton->SetIsEnabled(bEnabled); }
}

void UAFLW_RetailCard::SetStatus(const FText& Text, const FLinearColor& Color)
{
	if (StatusText)
	{
		StatusText->SetText(Text);
		StatusText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UAFLW_RetailCard::HandleBuy()     { if (Owner) { Owner->OnKeyBuy(); } }
void UAFLW_RetailCard::HandleCart()    { if (Owner) { Owner->OnKeyCart(); } }
void UAFLW_RetailCard::HandleDiscard() { if (Owner) { Owner->OnKeyDiscard(); } }
void UAFLW_RetailCard::HandleDetails() { if (Owner) { Owner->OnKeyGrabDetails(); } }
