// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"

#include "AFLW_BRResult.generated.h"

class UTextBlock;
class UWidget;
class UCommonButtonBase;
class UAFLBattleRoyaleComponent;
class APlayerState;
struct FUIInputConfig;

/**
 * UAFLW_BRResult  (the Battle Royale match-end TAKEOVER -- the BR sibling of UAFLW_MatchScoreboard)
 *
 * A UCommonActivatableWidget PUSHED full-screen onto UI.Layer.Menu at match-end by UAFLMatchEndPresenter,
 * which resolves the card BY MODE (a UAFLBattleRoyaleComponent on the GameState -> this card). Before it
 * existed, BR inherited the team scoreboard: "0 - 0", TEAM A / TEAM B and a raw "Text Block" placeholder on a
 * free-for-all result. Operator-approved design (2026-09-06): mode label / hero verdict (VICTORY, ELIMINATED,
 * DRAW) / "#K OF N" placement / a stats row (placement, MATCH LENGTH, eliminations, Watts EARNED) / the
 * optional STAKE-PAYOUT-RATING strip for staked play / CONTINUE + the auto-return countdown.
 *
 * C++ owns every binding; the WBP child owns layout + styling (BindWidget). Same lifecycle discipline as the
 * scoreboard: HUD hidden while active, Menu input mode, GUARDED single return path, an auto-return timer so
 * a finished match can never sit in PostGame forever (the 6.5-hour GameLift session).
 *
 * STAT SOURCES (zero new backend):
 *  - WINNER / TOTAL / MATCH LENGTH: the replicated UAFLBattleRoyaleComponent (WinnerPlayerId,
 *    TotalParticipants, GetElapsedMatchSeconds -- frozen at match end = the recorded length).
 *  - PLACEMENT: the PlayerState's replicated StatTag TAG_AFL_Stat_BR_Placement (the server mirrors each booked
 *    rung there; the component's own map is server-only). Falls back to the server map on a listen host.
 *  - ELIMINATIONS: ALyraPlayerState::GetStatTagStackCount(ShooterGame.Score.Eliminations) -- replicated.
 *  - WATTS EARNED: the per-player Event.Match.Ended broadcast, handed in via ShowResults(). HONEST label EARNED.
 *  - STAKE / PAYOUT / RATING: the replicated UAFLMatchOutcomeComponent (pending dash until each service answers).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_BRResult : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Called by the presenter right after the push: hand in the collected EARNED map + render. */
	void ShowResults(const TMap<TWeakObjectPtr<APlayerState>, int32>& InEarnedWatts);

protected:
	//~UCommonActivatableWidget interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	/** Menu input while active -> clickable CONTINUE + visible cursor (mirrors ULyraActivatableWidget's Menu mode). */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End of UCommonActivatableWidget interface

	/** "VICTORY" / "ELIMINATED" / "DRAW" -- the hero verdict for the LOCAL player. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> VerdictText;

	/** "#K" -- the local player's finishing position (a dash if never booked). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> PlacementText;

	/** "M:SS" -- the frozen count-up match length. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> MatchLengthText;

	/** "OF N" beside the placement. Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OfText;

	/** "LAST ONE STANDING" (winner) / "WINNER · <name>" (eliminated) / "NO SURVIVOR" (draw). Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SublineText;

	/** The stats-row placement tile, "#K/N". Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PlacementTileText;

	/** The local player's eliminations this match. Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EliminationsText;

	/** "+N" Watts EARNED this match (signed). Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EarnedText;

	/** CONTINUE -> clean return to the IRONICS hub (ReturnToMainMenu). Optional so a pre-Continue WBP binds. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UCommonButtonBase> ContinueButton;

	/** "RETURNING IN 12" -- the auto-return countdown. Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CountdownText;

	/** Economy result for the LOCAL player (staked play). All optional; pending dash until each service answers. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StakeText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PayoutText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RatingDeltaText;

	/** Seconds the card holds before returning to the hub on its own. CONTINUE skips the wait. 0 = require CONTINUE. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|BR", meta = (ClampMin = "0"))
	int32 AutoReturnSeconds = 20;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|BR") FLinearColor VictoryColor    = FLinearColor(0.013f, 0.102f, 1.0f); // electric-blue #1E5AFF
	UPROPERTY(EditDefaultsOnly, Category = "AFL|BR") FLinearColor EliminatedColor = FLinearColor(1.0f, 0.25f, 0.25f);   // hostile red
	UPROPERTY(EditDefaultsOnly, Category = "AFL|BR") FLinearColor DrawColor       = FLinearColor(1.0f, 0.70f, 0.0f);    // amber

	/** WBP hook: the card is populated + visible; the WBP plays the entrance. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|BR")
	void OnResultShown(int32 InPlacement, bool bVictory);

private:
	void RebuildCard();

	/** Apex-style takeover: hide/restore the in-match HUD (UI.Layer.Game) while the results own the screen. */
	void SetHUDHidden(bool bHidden);

	/** CONTINUE click -> the one return path below. */
	void HandleContinueClicked();

	/** Countdown tick (1 Hz): repaint CountdownText, and return to the hub when it reaches zero. */
	void TickAutoReturn();

	/** Repaint the stake/payout/rating labels from the outcome component. Safe to call before data arrives. */
	void RefreshOutcome();

	/** THE ONE RETURN PATH -- UGameInstance::ReturnToMainMenu, GUARDED against the CONTINUE/countdown race. */
	void BeginReturnToHub();

	static FText FormatClock(float Seconds);

	TWeakObjectPtr<UAFLBattleRoyaleComponent> BR;
	bool bContinueBound = false;

	/** Auto-return state. Cleared on deactivate so a dismissed card cannot travel behind the player. */
	FTimerHandle AutoReturnTimer;
	int32 SecondsRemaining = 0;
	bool bReturning = false;

	/** Subscription to the outcome component; released on deactivate (the component outlives this widget). */
	FDelegateHandle OutcomeChangedHandle;

	/** Per-player this-match Watts EARNED, handed in by the presenter via ShowResults(). */
	TMap<TWeakObjectPtr<APlayerState>, int32> EarnedWatts;
};
