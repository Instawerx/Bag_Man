// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_RetailCartChip.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Retail/AFLRetailSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_RetailCartChip)

namespace AFLRetailChip
{
	static const FLinearColor Surface(0.004f, 0.006f, 0.02f, 0.90f);
	static const FLinearColor Accent(0.013f, 0.102f, 1.0f, 1.0f);
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

TSharedRef<SWidget> UAFLW_RetailCartChip::RebuildWidget()
{
	using namespace AFLRetailChip;
	UFont* Display = Orbitron();
	UFont* Body = NotoSans();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ChipCanvas"));
	WidgetTree->RootWidget = Canvas;

	Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChipPanel"));
	Panel->SetBrushColor(Surface);
	Panel->SetPadding(FMargin(10.f, 8.f));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Panel))
	{
		// Pinned LOWER-RIGHT (operator ruling, lap-2); the subsystem lifts it above the card via
		// SetCornerOffset when both are visible.
		S->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
		S->SetAlignment(FVector2D(1.f, 1.f));
		S->SetPosition(FVector2D(-24.f, -24.f));
		S->SetAutoSize(true);
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChipCol"));
	Panel->SetContent(Col);

	// The always-visible summary row doubles as the open/minimize control ([V] or click).
	ToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToggleButton"));
	ToggleButton->SetBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.05f));
	SummaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SummaryText"));
	Style(SummaryText, Display, 10.f, Accent);
	ToggleButton->AddChild(SummaryText);
	ToggleButton->OnClicked.AddDynamic(this, &UAFLW_RetailCartChip::HandleToggle);
	Col->AddChildToVerticalBox(ToggleButton);

	LinesBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LinesBox"));
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(LinesBox)) { S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }

	CheckoutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CheckoutButton"));
	CheckoutButton->SetBackgroundColor(Accent);
	CheckoutText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CheckoutText"));
	Style(CheckoutText, Display, 10.f, FLinearColor::White);
	CheckoutButton->AddChild(CheckoutText);
	CheckoutButton->OnClicked.AddDynamic(this, &UAFLW_RetailCartChip::HandleCheckout);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(CheckoutButton)) { S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChipStatus"));
	Style(StatusText, Body, 9.f, Dim);
	StatusText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(StatusText)) { S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f)); }

	return Super::RebuildWidget();
}

void UAFLW_RetailCartChip::SetCartView(const FText& SummaryLine, const TArray<FText>& Lines,
	const FText& CheckoutLabel, const FText& Status, bool bExpanded)
{
	using namespace AFLRetailChip;
	if (SummaryText) { SummaryText->SetText(SummaryLine); }

	if (LinesBox)
	{
		LinesBox->ClearChildren();
		if (bExpanded)
		{
			UFont* Body = NotoSans();
			for (const FText& Line : Lines)
			{
				UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				Style(T, Body, 10.f, FLinearColor(1.f, 1.f, 1.f, 0.8f));
				T->SetText(Line);
				if (UVerticalBoxSlot* S = LinesBox->AddChildToVerticalBox(T)) { S->SetPadding(FMargin(2.f, 2.f, 2.f, 0.f)); }
			}
		}
		LinesBox->SetVisibility(bExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CheckoutButton)
	{
		CheckoutButton->SetVisibility((bExpanded && !CheckoutLabel.IsEmpty())
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CheckoutText) { CheckoutText->SetText(CheckoutLabel); }
	if (StatusText)
	{
		StatusText->SetText(Status);
		StatusText->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UAFLW_RetailCartChip::SetCornerOffset(const FVector2D& Offset)
{
	if (UCanvasPanelSlot* S = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr)
	{
		S->SetPosition(Offset);
	}
}

void UAFLW_RetailCartChip::HandleToggle()   { if (Owner) { Owner->OnKeyChipToggle(); } }
void UAFLW_RetailCartChip::HandleCheckout() { if (Owner) { Owner->OnKeyCheckout(); } }
