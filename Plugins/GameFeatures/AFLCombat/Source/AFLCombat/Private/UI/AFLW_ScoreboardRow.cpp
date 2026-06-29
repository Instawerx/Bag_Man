// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_ScoreboardRow.h"

#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_ScoreboardRow)

void UAFLW_ScoreboardRow::SetRow(const FText& PlayerName, int32 Kills, int32 Deaths, int32 Assists,
	int32 WattsEarned, bool bLocalPlayer, int32 WattsExtracted)
{
	if (NameText)        { NameText->SetText(PlayerName); }
	if (KillsText)       { KillsText->SetText(FText::AsNumber(Kills)); }
	if (DeathsText)      { DeathsText->SetText(FText::AsNumber(Deaths)); }
	if (AssistsText)     { AssistsText->SetText(FText::AsNumber(Assists)); }
	if (WattsEarnedText) { WattsEarnedText->SetText(FText::AsNumber(WattsEarned)); }

	// Future EXTRACTED column (option B): writes only if the WBP added the optional cell AND a real value is passed.
	if (WattsExtractedText && WattsExtracted >= 0)
	{
		WattsExtractedText->SetText(FText::AsNumber(WattsExtracted));
	}

	OnRowSet(bLocalPlayer);
}
