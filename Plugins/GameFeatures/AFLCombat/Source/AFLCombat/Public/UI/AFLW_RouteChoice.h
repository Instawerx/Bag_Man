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
	virtual bool NativeOnHandleBackAction() override; // back = the default (Lobby)

	UFUNCTION() void HandleLobby();
	UFUNCTION() void HandleMatchmaking();

	void Choose(bool bMatchmaking);

private:
	static bool bPendingMatchmakingRoute;
	bool bChosen = false;
};
