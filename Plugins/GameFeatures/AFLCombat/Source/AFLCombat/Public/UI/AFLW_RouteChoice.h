// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"

#include "AFLW_RouteChoice.generated.h"

class UButton;
class UTextBlock;

/**
 * UAFLW_RouteChoice -- the post-login quick screen (operator ruling 2026-09-01, amending the
 * UX-flow SSOT invariant 1.3): WHERE TO? Outpost Lobby | Matchmaking. Asked every session by
 * ruling ("login saves" -- no choice memory). Matchmaking routes through the HOME SCREEN's own
 * League door (a pending-route flag the home screen consumes on activation), so the queue path
 * stays the one proven door wiring.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLCOMBAT_API UAFLW_RouteChoice : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UAFLW_RouteChoice();

	DECLARE_MULTICAST_DELEGATE_OneParam(FAFLRouteChosen, bool /*bMatchmaking*/);
	FAFLRouteChosen OnRouteChosen;

	/** Home screen pulls this exactly once on activation: TRUE = auto-open the League door. */
	static bool ConsumePendingMatchmakingRoute();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual bool NativeOnHandleBackAction() override; // back = the default (Lobby -> the Outpost)
	// Gamepad/keyboard focus lands on the default (Lobby) door. Without this the doors are raw UButtons with
	// no focus target, so CommonUI reported "isn't focusable - focusing the game viewport" (observed live
	// 2026-09-02) -- controller/keyboard users could not act on the cards. The Home screen focuses its League
	// door the same way.
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	// Own the input mode so the WHERE TO? door cards are always clickable from a cold-boot input state
	// (the first-launch dead-button race). Matches every working menu; the Landing sets the same.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// P0 FIX 2026-09-13 (the "dead WHERE TO? after a match / after Sign Out" bug): CommonUI layer stacks POOL
	// widget instances (UCommonActivatableWidgetContainerBase::GeneratedWidgetsPool) -- the SAME RouteChoice
	// object comes back on every later visit to the front end, carrying whatever state the last visit left.
	// The one-shot `bChosen` latch stayed true after the first choice, so every door AND the Esc back-handler
	// returned early forever after (proven 09-13: input reached the SButton, the handlers did nothing). Reset
	// per activation -- activation is the unit of "one choice", not the object's lifetime.
	virtual void NativeOnActivated() override;

#if !UE_BUILD_SHIPPING
	// Input-pipeline probe hooks (see AFLInputProbe.h): log the mouse-down/key events that actually reach THIS
	// widget so a dead screen can be bisected between "input never reached Slate" and "the widget got it".
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
#endif

	UFUNCTION() void HandleLobby();
	UFUNCTION() void HandleMatchmaking();

	void Choose(bool bMatchmaking);

	/** OUTPOST LOBBY: travel to the base (the same destination the Home STORE button reaches). Was the bug —
	 *  the lobby door used to only deactivate, dropping the player on the Home/Loadout surface instead. */
	void EnterOutpost();

private:
	static bool bPendingMatchmakingRoute;
	/** One choice per ACTIVATION (double-click / Esc-after-click guard). Reset in NativeOnActivated -- see the
	 *  pooling note there; a per-object lifetime latch is the bug this file was dead with for weeks. */
	bool bChosen = false;

	/** The default (Lobby) door -- captured in RebuildWidget so NativeGetDesiredFocusTarget can focus it. */
	UPROPERTY(Transient)
	TObjectPtr<UButton> DefaultFocusDoor = nullptr;
};
