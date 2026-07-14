// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "UI/LyraActivatableWidget.h" // ELyraWidgetInputMode (the enum only -- the class is private, we don't subclass it)

#include "AFLW_FrontEndMarket.generated.h"

struct FUIInputConfig;
class UWidget;

/** Which flavor the shared front-end market chassis presents. STEP 3 ships Store only (the store WBP's own
 *  graph still drives it, UNCHANGED); Loadout mode + the live-armory 3D stage land in later steps. */
UENUM(BlueprintType)
enum class EAFLMarketMode : uint8
{
	Store   UMETA(DisplayName = "Store"),   // browse purchasable -> BUY
	Loadout UMETA(DisplayName = "Loadout")  // browse owned -> EQUIP
};

/**
 * UAFLW_FrontEndMarket -- the NEW front-end "Digital Market" chassis (#7/B Phase 1). The store WBP
 * (AFLW_Menu_CosmeticShop) reparents here; later it also serves Loadout mode + hosts the live-armory 3D stage.
 *
 * PARENT NOTE: the ruling wanted ULyraActivatableWidget, but that class is LyraGame-PRIVATE (no export) -- a
 * C++ class in AFLCombat cannot subclass it (UAFLW_LoadoutBase / UAFLW_MatchScoreboard hit the same wall,
 * AFLW_LoadoutBase.h:33-35). So we subclass UCommonActivatableWidget directly and RE-IMPLEMENT
 * ULyraActivatableWidget's entire surface -- its two EditDefaultsOnly input UPROPERTIES + GetDesiredInputConfig
 * -- with the SAME names/types. On reparent the store WBP's authored InputConfig value carries over and the
 * input behavior is byte-identical: lateral by composition, not inheritance. The store's own buy/tabs/list
 * graph is untouched.
 *
 * ZERO coupling to the in-match UAFLW_LoadoutBase (no shared base, no shared branch). Shared cosmetic logic
 * comes from the stateless UAFLCosmeticBrowserLibrary service, never inheritance.
 *
 * STEP 3a is REPARENT-ONLY on purpose: no BindWidgets (the gamepad focus target is fetched by NAME, not bound --
 * see the note below). Prove the store compiles + runs UNCHANGED under the new parent first; wire genuine widget
 * references in the later step that consumes them.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_FrontEndMarket : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Store vs Loadout. STEP 3: Store only (the WBP graph still drives the store). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Market")
	EAFLMarketMode Mode = EAFLMarketMode::Store;

protected:
	//~UCommonActivatableWidget -- ULyraActivatableWidget parity (re-implemented; that class is LyraGame-private)
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	/** Gamepad: which widget takes focus when this screen is pushed. Without it CommonUI warns "GetDesiredFocusTarget
	 *  wasn't implemented" and a controller can't navigate the screen (console cert FAILS). Resolved by NAME via
	 *  GetWidgetFromName -- deliberately NOT a BindWidget -- so it never creates a property that would re-type the
	 *  store's graph variable (a base-typed BindWidget is exactly what broke the first reparent). Null-safe: falls
	 *  back to Super if the list isn't found. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~End

	/** Input mode while this UI is active -- SAME UPROPERTY (name + type) as ULyraActivatableWidget, so the
	 *  reparented store WBP's authored value carries over unchanged. */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	ELyraWidgetInputMode InputConfig = ELyraWidgetInputMode::Default;

	/** Mouse behavior when the game gets input -- mirrors ULyraActivatableWidget. */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	// NOTE (STEP 3a): NO BindWidgets. A C++ BindWidget whose name matches a store widget that the graph DRIVES
	// re-types that widget's graph variable to the C++ property's type -- declaring the stat-meter segments
	// (StatVisual_Seg0..4) as the base UWidget downcast them and broke every graph pin on reparent. The gamepad
	// focus target above is fetched by NAME at runtime (GetWidgetFromName), which creates no property and so cannot
	// re-type anything -- that is why it is safe where a BindWidget was not. Genuine widget binds are added
	// INCREMENTALLY in the step that consumes each one, typed to the widget's VERIFIED type (CenterShowroom /
	// ShowroomBG in STEP 4; detail panel + list in STEP 5). Reparent-clean first, then extend.
};
