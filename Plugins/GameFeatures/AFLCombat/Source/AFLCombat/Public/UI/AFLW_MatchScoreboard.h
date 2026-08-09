// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Templates/SubclassOf.h"

#include "AFLW_MatchScoreboard.generated.h"

class UTextBlock;
class UVerticalBox;
class UWidget;
class UCommonButtonBase;
class UAFLW_ScoreboardRow;
class UAFLRoundManagerComponent;
class APlayerState;
struct FUIInputConfig;

/**
 * UAFLW_MatchScoreboard  (Surface 4 -- the match-end results TAKEOVER)
 *
 * A UCommonActivatableWidget PUSHED full-screen onto UI.Layer.Menu at match-end (Apex/ARC-style
 * takeover), NOT a HUD-slot overlay. UAFLMatchEndUISubsystem owns the Event.Match.Ended trigger
 * and the per-player EARNED collection, then pushes this widget and calls ShowResults() with the
 * data. On activate this widget renders the board AND hides the in-match HUD (UI.Layer.Game), so
 * the full viewport dims/blurs edge-to-edge (the Menu layer fills the screen -- no content-sizing,
 * no corner problem). C++ owns bindings; the WBP child owns layout + the AAA styling.
 *
 * (Subclasses UCommonActivatableWidget directly + drives Menu input via GetDesiredInputConfig --
 * ULyraActivatableWidget is module-private to LyraGame, so it cannot be a cross-module base here.)
 *
 * STAT SOURCES (zero new backend except the future EXTRACTED column):
 *  - TEAM RESULT: the replicated UAFLRoundManagerComponent -- winner DERIVED (LastWinningTeam vs
 *    the local slot), final score = Team0Score/Team1Score.
 *  - K / D / A: ALyraPlayerState::GetStatTagStackCount(ShooterGame.Score.*) -- replicated StatTags.
 *  - WATTS EARNED: collected by UAFLMatchEndUISubsystem from the per-player Event.Match.Ended
 *    broadcast and handed in via ShowResults(). HONEST label EARNED, not extracted.
 *
 * LastWinReason is the DECIDING ROUND's reason -- captioned ("FINAL ROUND - ...").
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_MatchScoreboard : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Called by the match-end subsystem right after the push: hand in the collected EARNED map + render. */
	void ShowResults(const TMap<TWeakObjectPtr<APlayerState>, int32>& InEarnedWatts);

protected:
	//~UCommonActivatableWidget interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	/** Menu input while active -> clickable CONTINUE + visible cursor (mirrors ULyraActivatableWidget's Menu mode). */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End of UCommonActivatableWidget interface

	/** "VICTORY" / "DEFEAT" / "DRAW" from the local team vs the derived winner. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> OutcomeText;

	/** Final match score, local POV: "{MyScore} - {EnemyScore}" (rounds won). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> ScoreText;

	/** The DECIDING ROUND's reason, captioned ("FINAL ROUND - ELIMINATION"). Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ReasonText;

	/** Per-team row containers (slot 0 / slot 1). The C++ spawns one RowWidgetClass per player into these. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UVerticalBox> Team0RowBox;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UVerticalBox> Team1RowBox;

	/** CONTINUE -> clean return to the IRONICS hub (ReturnToMainMenu). A CommonUI button (the proven
	 *  W_LyraMenuButton treatment). Optional so a pre-Continue WBP still binds. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UCommonButtonBase> ContinueButton;

	/** The per-player row widget (a WBP child of UAFLW_ScoreboardRow), set on the WBP. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Scoreboard")
	TSubclassOf<UAFLW_ScoreboardRow> RowWidgetClass;

	/** "RETURNING IN 12..." -- the auto-return countdown. Optional so a pre-countdown WBP still binds. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CountdownText;

	/**
	 * ECONOMY RESULT for the LOCAL player, read from the replicated UAFLMatchOutcomeComponent. All optional:
	 * the WBP has no such labels yet, so the C++ lands first and these light up the moment the text blocks
	 * are added, with no further code change.
	 *
	 * These populate LATE and that is correct, not a glitch -- settle and rating are HTTP round-trips that
	 * answer ~2-3 s after this board is already on screen, so each shows a pending dash until its service
	 * replies. The dash is deliberate: "-" means not yet known, whereas "0" would be a claim that the match
	 * paid nothing, which is a different and possibly false statement.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StakeText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PayoutText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RatingDeltaText;

	/**
	 * Seconds the board holds before returning to the hub on its own. CONTINUE skips the wait.
	 *
	 * A board with no timer is not merely less convenient -- it is how a finished match becomes a match that
	 * never ends. Before the results widget was cooked at all there was no CONTINUE either, so a concluded
	 * match sat in PostGame indefinitely; one observed run held its GameLift session for 6.5 hours. A player
	 * who alt-tabs must not be able to reproduce that, so the return does not depend on input.
	 *
	 * Set to 0 to disable the timer and require CONTINUE (not recommended for staked play).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Scoreboard", meta = (ClampMin = "0"))
	int32 AutoReturnSeconds = 20;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Scoreboard") FLinearColor VictoryColor = FLinearColor(0.013f, 0.102f, 1.0f); // electric-blue #1E5AFF
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Scoreboard") FLinearColor DefeatColor  = FLinearColor(1.0f, 0.25f, 0.25f); // hostile red
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Scoreboard") FLinearColor DrawColor    = FLinearColor(1.0f, 0.70f, 0.0f);  // amber

	/** WBP hook: the board is populated + visible; the WBP plays the entrance + tints my team (LocalSlot). */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Scoreboard")
	void OnBoardShown(int32 InLocalSlot, bool bVictory);

private:
	void RebuildBoard();

	/** Apex-style takeover: hide/restore the in-match HUD (UI.Layer.Game) while the results own the screen. */
	void SetHUDHidden(bool bHidden);

	/** CONTINUE click -> the one return path below. */
	void HandleContinueClicked();

	/** Countdown tick (1 Hz): repaint CountdownText, and return to the hub when it reaches zero. */
	void TickAutoReturn();

	/** Repaint the stake/payout/rating labels from the outcome component. Safe to call before data arrives. */
	void RefreshOutcome();

	/**
	 * THE ONE RETURN PATH -- UGameInstance::ReturnToMainMenu (clean session teardown + travel to the hub).
	 *
	 * Both CONTINUE and the countdown funnel through here, and it is GUARDED. There are now two independent
	 * triggers racing on the same board: a player pressing CONTINUE on the last countdown second would
	 * otherwise call ReturnToMainMenu twice, tearing the session down mid-teardown. bReturning makes the
	 * second caller a no-op rather than relying on the two never coinciding.
	 */
	void BeginReturnToHub();

	TWeakObjectPtr<UAFLRoundManagerComponent> Round;
	int32 LocalSlot = INDEX_NONE;
	bool bContinueBound = false;

	/** Auto-return state. Cleared on deactivate so a dismissed board cannot travel behind the player. */
	FTimerHandle AutoReturnTimer;
	int32 SecondsRemaining = 0;
	bool bReturning = false;

	/** Subscription to the outcome component; released on deactivate (the component outlives this widget). */
	FDelegateHandle OutcomeChangedHandle;

	/** Per-player this-match Watts EARNED, handed in by the match-end subsystem via ShowResults(). */
	TMap<TWeakObjectPtr<APlayerState>, int32> EarnedWatts;
};
