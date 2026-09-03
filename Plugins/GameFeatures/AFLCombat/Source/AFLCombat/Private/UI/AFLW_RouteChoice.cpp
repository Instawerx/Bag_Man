// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_RouteChoice.h"

#include "AFLCombat.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_RouteChoice)

bool UAFLW_RouteChoice::bPendingMatchmakingRoute = false;

namespace AFLRoute
{
	static const FLinearColor Ground(0.0107f, 0.0180f, 0.0439f, 0.96f); // deep panel wash over the world
	static const FLinearColor Surface(0.0028f, 0.0043f, 0.0231f, 1.f);  // #0E122B
	static const FLinearColor Accent(0.013f, 0.102f, 1.0f, 1.0f);       // #1E5AFF
	static const TCHAR* LogoPath = TEXT("/Game/Characters/Cosmetics/T_IRONICS_Logo_Transparent.T_IRONICS_Logo_Transparent");

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

UAFLW_RouteChoice::UAFLW_RouteChoice()
{
	bIsBackHandler = true;
}

bool UAFLW_RouteChoice::ConsumePendingMatchmakingRoute()
{
	const bool bWas = bPendingMatchmakingRoute;
	bPendingMatchmakingRoute = false;
	return bWas;
}

TSharedRef<SWidget> UAFLW_RouteChoice::RebuildWidget()
{
	using namespace AFLRoute;
	UFont* Display = Orbitron();
	UFont* Body = NotoSans();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RouteCanvas"));
	WidgetTree->RootWidget = Canvas;

	UBorder* Wash = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Wash"));
	Wash->SetBrushColor(Ground);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Wash))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Style(Title, Display, 28.f, FLinearColor::White);
	Title->SetText(NSLOCTEXT("AFLRoute", "Title", "WHERE TO?"));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Title))
	{
		S->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
		S->SetAlignment(FVector2D(0.5f, 0.f));
		S->SetPosition(FVector2D(0.f, 84.f));
		S->SetAutoSize(true);
	}

	auto MakeDoor = [&](const TCHAR* Name, const FText& DoorTitle, const FText& Blurb, const FText& CtaText,
		bool bPrimary, float XOffset, void (UAFLW_RouteChoice::*Handler)()) -> void
	{
		UButton* Door = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Door->SetBackgroundColor(Surface);
		if (Handler == &UAFLW_RouteChoice::HandleLobby)
		{
			Door->OnClicked.AddDynamic(this, &UAFLW_RouteChoice::HandleLobby);
		}
		else
		{
			Door->OnClicked.AddDynamic(this, &UAFLW_RouteChoice::HandleMatchmaking);
		}
		if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Door))
		{
			S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetPosition(FVector2D(XOffset, 40.f));
			S->SetSize(FVector2D(400.f, 360.f));
		}
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Door->AddChild(Col);
		if (bPrimary)
		{
			// The real logo crowns the default door (game art only).
			UImage* Logo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, LogoPath))
			{
				Logo->SetBrushFromTexture(Tex, false);
			}
			if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Logo))
			{
				VS->SetHorizontalAlignment(HAlign_Center);
				VS->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
			}
			Logo->SetDesiredSizeOverride(FVector2D(120.f, 120.f));
		}
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(T, Display, 21.f, FLinearColor::White);
		T->SetText(DoorTitle);
		if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(T))
		{
			VS->SetHorizontalAlignment(HAlign_Center);
			VS->SetPadding(FMargin(0.f, bPrimary ? 14.f : 120.f, 0.f, 0.f));
		}
		UTextBlock* B = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(B, Body, 12.f, FLinearColor(1.f, 1.f, 1.f, 0.65f));
		B->SetAutoWrapText(true);
		B->SetJustification(ETextJustify::Center);
		B->SetText(Blurb);
		if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(B))
		{
			VS->SetPadding(FMargin(24.f, 8.f, 24.f, 0.f));
		}
		UTextBlock* Cta = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(Cta, Display, 12.f, bPrimary ? AFLRoute::Accent : FLinearColor(1.f, 1.f, 1.f, 0.8f));
		Cta->SetText(CtaText);
		if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Cta))
		{
			VS->SetHorizontalAlignment(HAlign_Center);
			VS->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
		}
	};

	MakeDoor(TEXT("LobbyDoor"),
		NSLOCTEXT("AFLRoute", "Lobby", "OUTPOST LOBBY"),
		NSLOCTEXT("AFLRoute", "LobbyBlurb", "Walk the base — the PX houses, weapon field, range, clubs. Deploy whenever you want."),
		NSLOCTEXT("AFLRoute", "LobbyCta", "ENTER THE BASE  [ENTER]"),
		true, -230.f, &UAFLW_RouteChoice::HandleLobby);

	MakeDoor(TEXT("MatchDoor"),
		NSLOCTEXT("AFLRoute", "Match", "MATCHMAKING"),
		NSLOCTEXT("AFLRoute", "MatchBlurb", "Straight to the queue — League play, pick your bracket, deploy. The base can wait."),
		NSLOCTEXT("AFLRoute", "MatchCta", "FIND A MATCH  [TAB]"),
		false, 230.f, &UAFLW_RouteChoice::HandleMatchmaking);

	UTextBlock* Foot = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Foot"));
	Style(Foot, Body, 11.f, FLinearColor(1.f, 1.f, 1.f, 0.45f));
	Foot->SetText(NSLOCTEXT("AFLRoute", "Foot", "You can always switch at the Deployments door in the base."));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Foot))
	{
		S->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
		S->SetAlignment(FVector2D(0.5f, 1.f));
		S->SetPosition(FVector2D(0.f, -26.f));
		S->SetAutoSize(true);
	}

	return Super::RebuildWidget();
}

bool UAFLW_RouteChoice::NativeOnHandleBackAction()
{
	Choose(false); // back = the default route (the base)
	return true;
}

void UAFLW_RouteChoice::HandleLobby()       { Choose(false); }
void UAFLW_RouteChoice::HandleMatchmaking() { Choose(true); }

void UAFLW_RouteChoice::Choose(bool bMatchmaking)
{
	if (bChosen)
	{
		return;
	}
	bChosen = true;
	bPendingMatchmakingRoute = bMatchmaking;
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUTE: chose %s."), bMatchmaking ? TEXT("MATCHMAKING") : TEXT("LOBBY"));
	OnRouteChosen.Broadcast(bMatchmaking);
	DeactivateWidget();
}
