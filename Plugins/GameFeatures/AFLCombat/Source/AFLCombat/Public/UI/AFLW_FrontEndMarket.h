// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "UI/LyraActivatableWidget.h" // ELyraWidgetInputMode (the enum only -- the class is private, we don't subclass it)

#include "AFLW_FrontEndMarket.generated.h"

struct FUIInputConfig;
class UWidget;

/** Which flavor the shared front-end market chassis presents. STEP 3 ships Store only (the store WBP's own
 *  graph still drives it, UNCHANGED); Loadout mode lands in a later step. */
UENUM(BlueprintType)
enum class EAFLMarketMode : uint8
{
	Store   UMETA(DisplayName = "Store"),   // browse purchasable -> BUY
	Loadout UMETA(DisplayName = "Loadout")  // browse owned -> EQUIP
};

/**
 * UAFLW_FrontEndMarket -- the NEW front-end "Digital Market" chassis (#7/B Phase 1). The store WBP
 * (AFLW_Menu_CosmeticShop) reparents here; it renders as a full-screen overlay ON the live L_IRONICS_Armory
 * scene -- the staged display robot shows THROUGH a transparent center (NO SceneCapture / render target; the
 * 3D character IS the scene, the UI floats on it).
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
	 *  store's graph variable (a base-typed BindWidget is exactly what broke the first reparent). Null-safe. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~End

	//~UUserWidget
	virtual void NativeConstruct() override;
	//~End

	/** Center hero slot -> live scene: when an armory display pawn is present behind us, dissolve the market's
	 *  full-screen backdrop (the Overlay glass/gradient/glow layers + the root Border brush) and the center backing
	 *  so the UI floats on the 3D scene and the framed robot shows through. Over the hub (no display pawn) the
	 *  backdrop is left exactly as authored -> store-over-hub unchanged. All by NAME (GetWidgetFromName). */
	void ApplyShowroomMode();

	/** Input mode while this UI is active -- SAME UPROPERTY (name + type) as ULyraActivatableWidget, so the
	 *  reparented store WBP's authored value carries over unchanged. */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	ELyraWidgetInputMode InputConfig = ELyraWidgetInputMode::Default;

	/** Mouse behavior when the game gets input -- mirrors ULyraActivatableWidget. */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	// NOTE: NO BindWidgets. A C++ BindWidget whose name matches a store widget that the graph DRIVES re-types that
	// widget's graph variable to the C++ property's type -- declaring the stat-meter segments (StatVisual_Seg0..4)
	// as the base UWidget downcast them and broke every graph pin on reparent. Every widget this chassis touches --
	// the gamepad focus target (NativeGetDesiredFocusTarget) and the showroom backing (ApplyShowroomMode) -- is
	// fetched by NAME at runtime (GetWidgetFromName), which creates no property and so cannot re-type anything.
	// That is why it is safe where a BindWidget was not. Any future genuine bind must be typed to the widget's
	// VERIFIED type, in the step that consumes it.
};
