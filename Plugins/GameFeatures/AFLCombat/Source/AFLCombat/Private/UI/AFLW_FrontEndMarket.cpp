// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_FrontEndMarket.h"

#include "Components/Widget.h"        // UWidget (NativeGetDesiredFocusTarget return / GetWidgetFromName result)
#include "Input/CommonUIInputTypes.h" // FUIInputConfig, ECommonInputMode, EMouseCaptureMode

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
