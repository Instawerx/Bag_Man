// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_FrontEndMarket.h"

#include "AFLCombat.h"                // LogAFLCombat
#include "Components/Border.h"        // UBorder (root / center brush -> transparent)
#include "Components/Widget.h"        // UWidget + GetWidgetFromName result (SetVisibility)
#include "Engine/World.h"             // UWorld (world context for the live-scene probe)
#include "Input/CommonUIInputTypes.h" // FUIInputConfig, ECommonInputMode, EMouseCaptureMode
#include "Kismet/GameplayStatics.h"   // UGameplayStatics::GetActorOfClass
#include "UI/AFLLoadoutDisplayPawn.h" // AAFLLoadoutDisplayPawn (is a live display scene behind us?)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_FrontEndMarket)

TOptional<FUIInputConfig> UAFLW_FrontEndMarket::GetDesiredInputConfig() const
{
	// Byte-for-byte mirror of ULyraActivatableWidget::GetDesiredInputConfig (that class is LyraGame-private, so
	// we can't inherit it). The store WBP's authored InputConfig value carries over on reparent (same UPROPERTY)
	// -> identical input behavior.
	switch (InputConfig)
	{
	case ELyraWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case ELyraWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case ELyraWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case ELyraWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

UWidget* UAFLW_FrontEndMarket::NativeGetDesiredFocusTarget() const
{
	// Land the controller on the product browser. Resolved by NAME (NOT a BindWidget) so this never creates a
	// property that would re-type the store's graph variable -- the reparent stays graph-neutral. If the store's
	// list isn't named "ShopListView" (or a later Loadout layout omits it), fall back to the base so focus still
	// resolves somewhere sane and the CommonUI "no focus target" warning stays satisfied by the override existing.
	if (UWidget* Browser = GetWidgetFromName(TEXT("ShopListView")))
	{
		return Browser;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UAFLW_FrontEndMarket::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyShowroomMode();
}

void UAFLW_FrontEndMarket::ApplyShowroomMode()
{
	// Only dissolve the backdrop when a live display scene is behind us (the armory display robot). Over the hub
	// there is no display pawn -> leave the market's designed backdrop fully intact, so the proven store-over-hub
	// is untouched (no Step-3 regression). Self-contained: the market detects its own context, no pusher coupling.
	UWorld* World = GetWorld();
	const bool bLiveScene = World && (UGameplayStatics::GetActorOfClass(World, AAFLLoadoutDisplayPawn::StaticClass()) != nullptr);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MARKET: ApplyShowroomMode bLiveScene=%s"), bLiveScene ? TEXT("true") : TEXT("false"));
	if (!bLiveScene)
	{
		return;
	}

	// Live armory scene: hide the market's full-screen backdrop layers (the Overlay children that sit BEHIND the
	// content -- GlassBlur / BackdropGradV / GlassPanel / BackdropGlowPool) plus the center backing + the
	// "FEATURED PREVIEW" placeholder, so the UI floats on the 3D scene and the framed robot shows through the
	// center. Every widget resolved by NAME (never a BindWidget) -> graph-neutral, cannot re-type store variables.
	static const TCHAR* const HideByName[] = {
		TEXT("GlassBlur"),        // background blur pass
		TEXT("BackdropGradV"),    // full-screen vertical gradient
		TEXT("GlassPanel"),       // glass sheet
		TEXT("BackdropGlowPool"), // center glow pool
		TEXT("ShowroomBG"),       // center backing
		TEXT("ShowroomLabel")     // "FEATURED PREVIEW" placeholder (the live robot replaces it)
	};
	for (const TCHAR* Name : HideByName)
	{
		if (UWidget* W = GetWidgetFromName(FName(Name)))
		{
			W->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MARKET: widget '%s' not found by name (backdrop not fully dissolved)."), Name);
		}
	}

	// A Border's own brush can still occlude the scene even with its children hidden -> tint the root + center
	// Borders fully transparent (Cast is null-safe if either isn't actually a UBorder; children are unaffected).
	for (const TCHAR* BorderName : { TEXT("RootBorder"), TEXT("CenterShowroom") })
	{
		if (UBorder* B = Cast<UBorder>(GetWidgetFromName(FName(BorderName))))
		{
			B->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		}
	}
}
