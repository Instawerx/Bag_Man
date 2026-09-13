// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"

#include "AFLW_RouteChoice.generated.h"

class UButton;
class UTextBlock;
enum class EAFLHomeDoor : uint8; // defined in AFLW_HomeScreen.h; only referenced by value here

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

	/**
	 * RETURN-TO-LAST-LOBBY (the after-match bug). A lobby records the door it committed a match from; it
	 * survives the return ClientTravel because it is static, exactly like bPendingMatchmakingRoute. On the way
	 * back to the front end the Home screen consumes it and reopens that lobby, and THIS screen peeks it to skip
	 * the WHERE TO? cards entirely — a match return should land in the lobby, not re-ask where to go.
	 */
	static void SetPendingReturnDoor(EAFLHomeDoor Door);
	/** Home pulls this exactly once on activation. Returns false (and leaves OutDoor untouched) when none is set. */
	static bool ConsumePendingReturnDoor(EAFLHomeDoor& OutDoor);
	/** Non-destructive test — this screen uses it to decide whether to auto-skip; only Home consumes. */
	static bool HasPendingReturnDoor();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;        // auto-skip the cards on a match return
	virtual bool NativeOnHandleBackAction() override; // back = the default (Lobby -> the Outpost)
	// Gamepad/keyboard focus lands on the default (Lobby) door. Without this the doors are raw UButtons with
	// no focus target, so CommonUI reported "isn't focusable - focusing the game viewport" (observed live
	// 2026-09-02) -- controller/keyboard users could not act on the cards. The Home screen focuses its League
	// door the same way.
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	// Own the input mode so the WHERE TO? door cards are always clickable from a cold-boot input state
	// (the first-launch dead-button race). Matches every working menu; the Landing sets the same.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION() void HandleLobby();
	UFUNCTION() void HandleMatchmaking();

	void Choose(bool bMatchmaking);

	/** OUTPOST LOBBY: travel to the base (the same destination the Home STORE button reaches). Was the bug —
	 *  the lobby door used to only deactivate, dropping the player on the Home/Loadout surface instead. */
	void EnterOutpost();

private:
	static bool bPendingMatchmakingRoute;
	/** The lobby (League/Staked) a match was last committed from, for the return-to-last-lobby flow. */
	static bool bHasPendingReturnDoor;
	static EAFLHomeDoor PendingReturnDoor;
	bool bChosen = false;

	/** The default (Lobby) door -- captured in RebuildWidget so NativeGetDesiredFocusTarget can focus it. */
	UPROPERTY(Transient)
	TObjectPtr<UButton> DefaultFocusDoor = nullptr;
};
