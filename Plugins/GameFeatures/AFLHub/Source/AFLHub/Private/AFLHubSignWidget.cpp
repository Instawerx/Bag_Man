// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHubSignWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHubSignWidget)

namespace AFLHubSign
{
	// Brand lock (IRONICS_CC_DESIGN_BRIEF s0) + the ratified status-offline token.
	const FLinearColor Accent        = FLinearColor::FromSRGBColor(FColor(0x1E, 0x5A, 0xFF));
	const FLinearColor Surface       = FLinearColor::FromSRGBColor(FColor(0x0E, 0x12, 0x2B, 245));
	const FLinearColor TextPrimary   = FLinearColor::FromSRGBColor(FColor(0xE8, 0xEE, 0xFF));
	const FLinearColor TextMuted     = FLinearColor::FromSRGBColor(FColor(0x8E, 0x9A, 0xB8));
	const FLinearColor AccentSoft    = FLinearColor::FromSRGBColor(FColor(0x7F, 0xA3, 0xFF));
	const FLinearColor Offline       = FLinearColor::FromSRGBColor(FColor(0xFF, 0x5A, 0x64));
	const FLinearColor OfflineText   = FLinearColor::FromSRGBColor(FColor(0xFF, 0x8A, 0x92));

	UFont* Orbitron() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/Orbitron.Orbitron")); }
	UFont* NotoSans() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/NotoSans.NotoSans")); }

	FSlateFontInfo Face(UFont* Font, int32 Size, FName Typeface = NAME_None)
	{
		FSlateFontInfo Info(Font, Size, Typeface);
		return Info;
	}
}

TSharedRef<SWidget> UAFLHubSignWidget::RebuildWidget()
{
	using namespace AFLHubSign;

	UFont* Display = Orbitron();
	UFont* Body    = NotoSans();

	Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SignRoot"));
	WidgetTree->RootWidget = Root;

	// --- FAR: diamond marker (a rotated bordered square; glow comes from the accent outline) ---
	DiamondOuter = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Diamond"));
	DiamondOuter->SetBrushColor(Surface);
	DiamondOuter->SetPadding(FMargin(7.0f));
	DiamondOuter->SetRenderTransformAngle(45.0f);
	DiamondGlyph = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DiamondGlyph"));
	DiamondGlyph->SetRenderTransformAngle(-45.0f);
	DiamondGlyph->SetDesiredSizeOverride(FVector2D(22.0f, 22.0f));
	DiamondGlyph->SetColorAndOpacity(Accent);
	DiamondOuter->SetContent(DiamondGlyph);
	{
		FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.Width = 1.5f;
		Brush.OutlineSettings.Color = FSlateColor(Accent);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(2, 2, 2, 2);
		Brush.TintColor = FSlateColor(Surface);
		DiamondOuter->SetBrush(Brush);
	}
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(DiamondOuter))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0, 0, 0, 10));
	}

	// --- Name row (all tiers): AT-DOOR glyph badge + name ---
	UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("NameRow"));
	BadgeGlyph = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BadgeGlyph"));
	BadgeGlyph->SetDesiredSizeOverride(FVector2D(26.0f, 26.0f));
	BadgeGlyph->SetColorAndOpacity(Accent);
	if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(BadgeGlyph))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0, 0, 10, 0));
	}
	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetFont(Face(Display, 15, TEXT("Bold")));
	NameText->SetColorAndOpacity(FSlateColor(TextPrimary));
	NameText->SetShadowOffset(FVector2D(0, 0));
	NameText->SetShadowColorAndOpacity(FLinearColor(Accent.R, Accent.G, Accent.B, 0.8f));
	if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(NameText))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(NameRow))
	{
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// --- Subtitle (AtDoor only) ---
	SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SubtitleText"));
	SubtitleText->SetFont(Face(Body, 11));
	SubtitleText->SetColorAndOpacity(FSlateColor(TextMuted));
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(SubtitleText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0, 2, 0, 0));
	}

	// --- Distance (Mid only; NotoSans substitutes for DroidSansMono -- not imported) ---
	DistanceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DistanceText"));
	DistanceText->SetFont(Face(Body, 12));
	DistanceText->SetColorAndOpacity(FSlateColor(AccentSoft));
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(DistanceText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0, 2, 0, 0));
	}

	// --- Status band (AtDoor only): dot + status word on a surface strip ---
	StatusBand = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusBand"));
	{
		FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.Width = 1.0f;
		Brush.OutlineSettings.Color = FSlateColor(FLinearColor(Accent.R, Accent.G, Accent.B, 0.35f));
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(3, 3, 3, 3);
		Brush.TintColor = FSlateColor(Surface);
		StatusBand->SetBrush(Brush);
	}
	StatusBand->SetPadding(FMargin(14, 6, 14, 6));
	UHorizontalBox* BandRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BandRow"));
	StatusBand->SetContent(BandRow);

	StatusDot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusDot"));
	{
		FSlateBrush Dot; Dot.DrawAs = ESlateBrushDrawType::RoundedBox;
		Dot.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Dot.TintColor = FSlateColor(Offline);
		StatusDot->SetBrush(Dot);
	}
	StatusDot->SetPadding(FMargin(4.0f));
	if (UHorizontalBoxSlot* S = BandRow->AddChildToHorizontalBox(StatusDot))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0, 0, 8, 0));
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetFont(Face(Display, 12, TEXT("Medium")));
	if (UHorizontalBoxSlot* S = BandRow->AddChildToHorizontalBox(StatusText))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(StatusBand))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0, 8, 0, 0));
	}

	// --- Light pillar (Far/Mid): the vertical accent bar under the sign ---
	Pillar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Pillar"));
	Pillar->SetBrushColor(FLinearColor(Accent.R, Accent.G, Accent.B, 0.55f));
	Pillar->SetPadding(FMargin(1.5f, 90.0f));
	if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Pillar))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0, 6, 0, 0));
	}

	return Super::RebuildWidget();
}

void UAFLHubSignWidget::SetSignData(const FText& InName, const FText& InSubtitle, bool bInEnabled,
	EAFLHubSignTier InTier, float InDistanceMeters, UTexture2D* InGlyph)
{
	using namespace AFLHubSign;
	if (!NameText) // RebuildWidget not run yet
	{
		return;
	}

	NameText->SetText(InName);
	SubtitleText->SetText(InSubtitle);
	DistanceText->SetText(FText::FromString(FString::Printf(TEXT("%.0fm"), InDistanceMeters)));
	StatusText->SetText(bInEnabled
		? NSLOCTEXT("AFLHub", "SignEnter", "ENTER · [E]")
		: NSLOCTEXT("AFLHub", "SignOffline", "OFFLINE"));
	StatusText->SetColorAndOpacity(FSlateColor(bInEnabled ? AccentSoft : OfflineText));
	if (StatusDot)
	{
		FSlateBrush Dot = StatusDot->Background;
		Dot.TintColor = FSlateColor(bInEnabled ? Accent : Offline);
		StatusDot->SetBrush(Dot);
	}

	const bool bFar  = InTier == EAFLHubSignTier::Far;
	const bool bMid  = InTier == EAFLHubSignTier::Mid;
	const bool bDoor = InTier == EAFLHubSignTier::AtDoor;

	if (InGlyph)
	{
		DiamondGlyph->SetBrushFromTexture(InGlyph, false);
		DiamondGlyph->SetDesiredSizeOverride(FVector2D(22.0f, 22.0f));
		BadgeGlyph->SetBrushFromTexture(InGlyph, false);
		BadgeGlyph->SetDesiredSizeOverride(FVector2D(26.0f, 26.0f));
	}
	BadgeGlyph->SetVisibility(bDoor && InGlyph ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DiamondOuter->SetVisibility(bFar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SubtitleText->SetVisibility(bDoor && !InSubtitle.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DistanceText->SetVisibility(bMid ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	StatusBand->SetVisibility(bDoor ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Pillar->SetVisibility(bDoor ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	UFont* Display = Orbitron();
	NameText->SetFont(Face(Display, bDoor ? 21 : (bMid ? 15 : 11), bDoor ? TEXT("Black") : TEXT("Bold")));
	NameText->SetColorAndOpacity(FSlateColor(bFar ? AccentSoft : TextPrimary));
}
