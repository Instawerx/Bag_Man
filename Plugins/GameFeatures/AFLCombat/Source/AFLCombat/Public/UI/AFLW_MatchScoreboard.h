// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Templates/SubclassOf.h"

#include "AFLW_MatchScoreboard.generated.h"

class UTextBlock;
class UVerticalBox;
class UAFLW_ScoreboardRow;
class UAFLRoundManagerComponent;
class APlayerState;

/**
 * UAFLW_MatchScoreboard  (Surface 4 -- the match-end results board; the ExtractionAnnounce S-later debt)
 *
 * Plain UUserWidget (passive auto-show, NOT Lyra's activatable W_MatchScoreBoard). C++ owns bindings; the
 * WBP child owns layout (BindWidget). Renders on PostGame, driven by the per-player Event.Match.Ended
 * message -- the SAME signal the MATCH-COMPLETE banner rode (UAFLW_ExtractionAnnounce's match-end line is
 * now folded OUT into this board). Mirrors the surfaces-1+2 split (the round header / result toast).
 *
 * STAT SOURCES (zero new backend except the future EXTRACTED column):
 *  - TEAM RESULT: the already-replicated UAFLRoundManagerComponent (a UGameStateComponent). The match
 *    winner is NOT stored, it is DERIVED: winner = LastWinningTeam (== the team whose score crossed the
 *    win target at MatchEnd), final score = Team0Score/Team1Score. RoundsToWin is server-only, so there
 *    is NO client-side ">= N" test -- the winner comes from LastWinningTeam vs the local slot.
 *  - K / D / A: ALyraPlayerState::GetStatTagStackCount(ShooterGame.Score.Eliminations/Deaths/Assists),
 *    written by the ShooterCore scoring component wired into the experience -- replicated StatTags.
 *  - WATTS EARNED: collected from the per-player Event.Match.Ended broadcast (Target = PlayerState,
 *    Magnitude = this-match Watts = kills + extraction; the client can't self-compute it -- the start
 *    snapshot is server-only). HONEST label EARNED, not extracted. The true per-player EXTRACTED column
 *    (option B) is future backend -- the row already reserves its optional cell.
 *
 * LastWinReason is the DECIDING ROUND's reason -- shown captioned ("FINAL ROUND - ...") so it is never
 * mis-read as a match-level reason.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_MatchScoreboard : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** "VICTORY" / "DEFEAT" / "DRAW" from the local team vs the derived winner. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> OutcomeText;

	/** Final match score, local POV: "{MyScore} - {EnemyScore}" (rounds won). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> ScoreText;

	/** The DECIDING ROUND's reason, captioned ("FINAL ROUND - ELIMINATION"). Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ReasonText;

	/** Per-team row containers (slot 0 / slot 1). The C++ spawns one RowWidgetClass per player into these. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UVerticalBox> Team0RowBox;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UVerticalBox> Team1RowBox;

	/** The per-player row widget (a WBP child of UAFLW_ScoreboardRow), set on the WBP. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Scoreboard")
	TSubclassOf<UAFLW_ScoreboardRow> RowWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Scoreboard") FLinearColor VictoryColor = FLinearColor(0.0f, 0.94f, 1.0f); // house cyan
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Scoreboard") FLinearColor DefeatColor  = FLinearColor(1.0f, 0.25f, 0.25f); // hostile red
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Scoreboard") FLinearColor DrawColor    = FLinearColor(1.0f, 0.70f, 0.0f);  // amber

	/** WBP hook: the board is populated + visible; the WBP plays the entrance + tints my team (LocalSlot). */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Scoreboard")
	void OnBoardShown(int32 InLocalSlot, bool bVictory);

private:
	void TryArm();
	void HandleMatchEnded(FGameplayTag Channel, const struct FLyraVerbMessage& Msg);
	void RebuildBoard();

	TWeakObjectPtr<UAFLRoundManagerComponent> Round;
	int32 LocalSlot = INDEX_NONE;
	bool bShown = false;

	/** Per-player this-match Watts EARNED, collected from the Event.Match.Ended per-player broadcast. */
	TMap<TWeakObjectPtr<APlayerState>, int32> EarnedWatts;

	FGameplayMessageListenerHandle MatchEndedListener;
	FTimerHandle ArmRetryTimer;
	FTimerHandle RebuildTimer;   // coalesces the N per-player messages into one paint
};
