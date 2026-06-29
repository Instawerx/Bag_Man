// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "AFLW_ScoreboardRow.generated.h"

class UTextBlock;

/**
 * UAFLW_ScoreboardRow  (Surface 4 -- one per-player row of the match-end scoreboard)
 *
 * Plain UUserWidget (passive). C++ owns bindings; the WBP child owns layout (BindWidget). One row =
 * NAME | K | D | A | WATTS EARNED. UAFLW_MatchScoreboard spawns one per player from GameState->PlayerArray
 * and calls SetRow.
 *
 * FUTURE-PROOFED for option B (true per-player WATTS EXTRACTED): WattsExtractedText is BindWidgetOptional
 * and SetRow takes a defaulted WattsExtracted, so when the per-player stats component lands the EXTRACTED
 * column drops into the SAME row with NO C++ rework -- add the text block to the WBP + pass a value >= 0.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_ScoreboardRow : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fill the row. WattsExtracted < 0 (default) leaves the optional EXTRACTED cell untouched (Layer 1). */
	void SetRow(const FText& PlayerName, int32 Kills, int32 Deaths, int32 Assists,
		int32 WattsEarned, bool bLocalPlayer, int32 WattsExtracted = INDEX_NONE);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> KillsText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> DeathsText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> AssistsText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) TObjectPtr<UTextBlock> WattsEarnedText;

	/** The future EXTRACTED column (Surface-4 option B). Optional so the Layer-1 WBP need not provide it yet. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> WattsExtractedText;

	/** WBP hook: C++ has filled the row; the WBP highlights the local player's row + plays any motion. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Scoreboard")
	void OnRowSet(bool bLocalPlayer);
};
