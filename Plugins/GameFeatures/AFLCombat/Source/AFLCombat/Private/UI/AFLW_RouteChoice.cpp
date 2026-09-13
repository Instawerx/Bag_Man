// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_RouteChoice.h"

#include "AFLCombat.h"
#include "UI/AFLW_HomeScreen.h"   // EAFLHomeDoor + the shared Outpost travel URL
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
#include "GameFramework/PlayerController.h"  // ClientTravel to the Outpost

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_RouteChoice)

bool UAFLW_RouteChoice::bPendingMatchmakingRoute = false;
bool UAFLW_RouteChoice::bHasPendingReturnDoor = false;
EAFLHomeDoor UAFLW_RouteChoice::PendingReturnDoor = EAFLHomeDoor::League;

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

void UAFLW_RouteChoice::SetPendingReturnDoor(EAFLHomeDoor Door)
{
	bHasPendingReturnDoor = true;
	PendingReturnDoor = Door;
}

bool UAFLW_RouteChoice::ConsumePendingReturnDoor(EAFLHomeDoor& OutDoor)
{
	if (!bHasPendingReturnDoor)
	{
		return false;
	}
	OutDoor = PendingReturnDoor;
	bHasPendingReturnDoor = false; // consumed exactly once, like the matchmaking route
	return true;
}

bool UAFLW_RouteChoice::HasPendingReturnDoor()
{
	return bHasPendingReturnDoor;
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
			DefaultFocusDoor = Door; // gamepad/keyboard focus target (see NativeGetDesiredFocusTarget).
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

void UAFLW_RouteChoice::NativeOnActivated()
{
	Super::NativeOnActivated();

	// A MATCH RETURN SKIPS THIS SCREEN. When a lobby recorded a return door, the player just finished a match
	// and should land back in that lobby, not be re-asked WHERE TO?. Deactivate immediately (before the cards
	// are meaningfully interactable) and leave the door pending for the Home screen to consume and reopen. Gated
	// on HasPendingReturnDoor, which is ONLY set by a lobby commit, so the ordinary first-login flow is
	// untouched. bChosen guards against re-entrancy the same way Choose() does.
	if (!bChosen && HasPendingReturnDoor())
	{
		bChosen = true;
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUTE: match return -> skipping WHERE TO?, Home will reopen the last lobby."));
		DeactivateWidget();
	}
}

UWidget* UAFLW_RouteChoice::NativeGetDesiredFocusTarget() const
{
	// Focus the default (Lobby) door so controller/keyboard users can act on the cards immediately.
	return DefaultFocusDoor ? static_cast<UWidget*>(DefaultFocusDoor) : Super::NativeGetDesiredFocusTarget();
}

bool UAFLW_RouteChoice::NativeOnHandleBackAction()
{
	EnterOutpost(); // back = the default route (the base) -- and the base is a real place, so travel to it
	return true;
}

TOptional<FUIInputConfig> UAFLW_RouteChoice::GetDesiredInputConfig() const
{
	// Menu mode, visible uncaptured cursor -- the door cards need the mouse, and owning the input mode
	// keeps them clickable regardless of the input state the Landing handoff leaves active.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UAFLW_RouteChoice::HandleLobby()       { EnterOutpost(); }
void UAFLW_RouteChoice::HandleMatchmaking() { Choose(true); }

void UAFLW_RouteChoice::Choose(bool bMatchmaking)
{
	if (bChosen)
	{
		return;
	}
	bChosen = true;
	bPendingMatchmakingRoute = bMatchmaking;
	// A deliberate WHERE TO? choice supersedes any stale return-to-last-lobby intent — the player is telling us
	// where to go right now, so a leftover return door must not fire on the next Home activation.
	bHasPendingReturnDoor = false;
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUTE: chose %s."), bMatchmaking ? TEXT("MATCHMAKING") : TEXT("LOBBY"));
	OnRouteChosen.Broadcast(bMatchmaking);
	DeactivateWidget();
}

void UAFLW_RouteChoice::EnterOutpost()
{
	if (bChosen)
	{
		return;
	}
	bChosen = true;
	// OUTPOST LOBBY is a real destination, not a fall-through: travel to the base, the same map+experience the
	// Home STORE button reaches. Before this, the lobby door only deactivated and dropped the player on the
	// Home/Loadout surface (the reported "old unfinished Loadout screen"). A fresh route decision, so clear any
	// stale return door too.
	bPendingMatchmakingRoute = false;
	bHasPendingReturnDoor = false;
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUTE: chose LOBBY -> traveling to the Outpost base."));
	OnRouteChosen.Broadcast(false);
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ClientTravel(UAFLW_HomeScreen::OutpostTravelURL(), TRAVEL_Absolute);
	}
	else
	{
		// No controller to travel with — do not strand the player on a dead modal; fall back to deactivating so
		// the front end shows underneath, matching the old behaviour rather than hanging.
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_ROUTE: LOBBY chosen but no owning player -- cannot travel to the base."));
		DeactivateWidget();
	}
}
