// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_MatchScoreboard.h"

#include "AFLCombat.h"
#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Input/CommonUIInputTypes.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Player/LyraPlayerState.h"
#include "PrimaryGameLayout.h"
#include "Round/AFLRoundManagerComponent.h"
#include "Teams/LyraTeamSubsystem.h"
#include "UI/AFLW_ScoreboardRow.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_MatchScoreboard)

namespace
{
	// The in-match HUD lives on UI.Layer.Game; the takeover collapses it (Apex-style) while active.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Game_Scoreboard, "UI.Layer.Game");

	FText ReasonCaption(EAFLRoundWinReason Reason)
	{
		switch (Reason)
		{
		case EAFLRoundWinReason::Elimination: return NSLOCTEXT("AFL", "SbElim", "FINAL ROUND - ELIMINATION");
		case EAFLRoundWinReason::Extraction:  return NSLOCTEXT("AFL", "SbExtract", "FINAL ROUND - EXTRACTION");
		case EAFLRoundWinReason::Timeout:     return NSLOCTEXT("AFL", "SbTimeout", "FINAL ROUND - TIMEOUT");
		case EAFLRoundWinReason::Replay:      return NSLOCTEXT("AFL", "SbReplay", "FINAL ROUND - REPLAY");
		default: return FText::GetEmpty();
		}
	}
}

TOptional<FUIInputConfig> UAFLW_MatchScoreboard::GetDesiredInputConfig() const
{
	// Menu input while the takeover owns the screen -> the CONTINUE button is clickable + the cursor is
	// visible. Mirrors ULyraActivatableWidget's Menu case (which is module-private to LyraGame).
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UAFLW_MatchScoreboard::ShowResults(const TMap<TWeakObjectPtr<APlayerState>, int32>& InEarnedWatts)
{
	// Called by UAFLMatchEndUISubsystem right after the push (post-activation), with the collected EARNED map.
	EarnedWatts = InEarnedWatts;
	RebuildBoard();
}

void UAFLW_MatchScoreboard::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Apex/ARC-style takeover: hide the in-match HUD so only the results own the screen.
	SetHUDHidden(true);

	// Wire CONTINUE once. Menu input mode is driven by GetDesiredInputConfig (the CommonUI activatable path).
	if (ContinueButton && !bContinueBound)
	{
		bContinueBound = true;
		ContinueButton->OnClicked().AddUObject(this, &UAFLW_MatchScoreboard::HandleContinueClicked);
	}
}

void UAFLW_MatchScoreboard::NativeOnDeactivated()
{
	// Restore the HUD (moot on a real CONTINUE -> we travel to the hub, but clean for any other dismissal).
	SetHUDHidden(false);
	Super::NativeOnDeactivated();
}

UWidget* UAFLW_MatchScoreboard::NativeGetDesiredFocusTarget() const
{
	return ContinueButton;
}

void UAFLW_MatchScoreboard::SetHUDHidden(bool bHidden)
{
	if (UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayoutForPrimaryPlayer(this))
	{
		if (UCommonActivatableWidgetContainerBase* GameLayer = Layout->GetLayerWidget(TAG_UI_Layer_Game_Scoreboard))
		{
			GameLayer->SetVisibility(bHidden ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

void UAFLW_MatchScoreboard::RebuildBoard()
{
	UWorld* World = GetWorld();
	UAFLRoundManagerComponent* R = Round.Get();
	if (!R)   // resolve on demand (the takeover is pushed at match-end; the round component is present by then)
	{
		AGameStateBase* GSResolve = World ? World->GetGameState() : nullptr;
		R = GSResolve ? GSResolve->FindComponentByClass<UAFLRoundManagerComponent>() : nullptr;
		if (R) { Round = R; }
	}
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!R || !GS) { return; }

	const ULyraTeamSubsystem* Teams = World->GetSubsystem<ULyraTeamSubsystem>();

	// Local-slot resolve (by match-end the team is known).
	if (LocalSlot == INDEX_NONE && Teams)
	{
		if (const APlayerController* PC = GetOwningPlayer())
		{
			if (const APlayerState* PS = PC->PlayerState)
			{
				const int32 TeamSlot = R->SlotForTeam(Teams->FindTeamFromObject(PS));
				if (TeamSlot != INDEX_NONE) { LocalSlot = TeamSlot; }
			}
		}
	}

	// -- TEAM RESULT (DERIVED; the match winner is not a stored field) --
	const int32 ScoreSlot  = (LocalSlot == INDEX_NONE) ? 0 : LocalSlot;
	const int32 MyScore    = (ScoreSlot == 0) ? R->Team0Score : R->Team1Score;
	const int32 EnemyScore = (ScoreSlot == 0) ? R->Team1Score : R->Team0Score;

	const int32 WinningTeamId = R->LastWinningTeam;
	const bool  bDraw    = (WinningTeamId == INDEX_NONE);
	const int32 WinSlot  = bDraw ? INDEX_NONE : R->SlotForTeam(WinningTeamId);
	const bool  bVictory = (!bDraw && WinSlot == ScoreSlot);

	if (OutcomeText)
	{
		FText OutText; FLinearColor OutCol;
		if (bDraw)         { OutText = NSLOCTEXT("AFL", "SbDraw", "DRAW");    OutCol = DrawColor; }
		else if (bVictory) { OutText = NSLOCTEXT("AFL", "SbWin", "VICTORY");  OutCol = VictoryColor; }
		else               { OutText = NSLOCTEXT("AFL", "SbLose", "DEFEAT");  OutCol = DefeatColor; }
		OutcomeText->SetText(OutText);
		OutcomeText->SetColorAndOpacity(FSlateColor(OutCol));
	}
	if (ScoreText)
	{
		ScoreText->SetText(FText::Format(NSLOCTEXT("AFL", "SbScore", "{0} - {1}"),
			FText::AsNumber(MyScore), FText::AsNumber(EnemyScore)));
	}
	if (ReasonText)
	{
		ReasonText->SetText(ReasonCaption(R->LastWinReason));
	}

	// -- PER-PLAYER ROWS (PlayerArray; K/D/A from replicated StatTags, EARNED from the handed-in map) --
	if (Team0RowBox) { Team0RowBox->ClearChildren(); }
	if (Team1RowBox) { Team1RowBox->ClearChildren(); }

	// ShooterCore scoring tags (config tags). Requested once, lazily (runtime, never in a ctor). ErrorIfNotFound
	// = false: if the scoring component isn't wired yet the counts read 0 gracefully rather than logging.
	static const FGameplayTag KillTag   = FGameplayTag::RequestGameplayTag(FName("ShooterGame.Score.Eliminations"), false);
	static const FGameplayTag DeathTag  = FGameplayTag::RequestGameplayTag(FName("ShooterGame.Score.Deaths"), false);
	static const FGameplayTag AssistTag = FGameplayTag::RequestGameplayTag(FName("ShooterGame.Score.Assists"), false);

	const APlayerState* MyPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) { continue; }
		const int32 TeamId = Teams ? Teams->FindTeamFromObject(PS) : INDEX_NONE;
		const int32 TeamSlot = R->SlotForTeam(TeamId);
		UVerticalBox* Box = (TeamSlot == 0) ? Team0RowBox : (TeamSlot == 1 ? Team1RowBox : nullptr);
		if (!Box || !RowWidgetClass) { continue; }

		int32 K = 0, D = 0, A = 0;
		if (const ALyraPlayerState* LPS = Cast<ALyraPlayerState>(PS))
		{
			K = LPS->GetStatTagStackCount(KillTag);
			D = LPS->GetStatTagStackCount(DeathTag);
			A = LPS->GetStatTagStackCount(AssistTag);
		}
		const int32* EarnedPtr = EarnedWatts.Find(PS);
		const int32 Earned = EarnedPtr ? *EarnedPtr : 0;

		if (UAFLW_ScoreboardRow* Row = CreateWidget<UAFLW_ScoreboardRow>(this, RowWidgetClass))
		{
			Row->SetRow(FText::FromString(PS->GetPlayerName()), K, D, A, Earned, PS == MyPS);
			Box->AddChildToVerticalBox(Row);
		}
	}

	OnBoardShown(ScoreSlot, bVictory);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_SCOREBOARD: takeover shown outcome=%s score=%d-%d players=%d"),
		bDraw ? TEXT("DRAW") : (bVictory ? TEXT("VICTORY") : TEXT("DEFEAT")),
		MyScore, EnemyScore, GS->PlayerArray.Num());
}

void UAFLW_MatchScoreboard::HandleContinueClicked()
{
	// Framework clean return: session teardown + travel to the front-end (IRONICS hub). Closes AFL-1911.
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			GI->ReturnToMainMenu();
		}
	}
}
