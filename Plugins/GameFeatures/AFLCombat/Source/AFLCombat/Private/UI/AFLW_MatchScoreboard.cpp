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
#include "Match/AFLMatchOutcomeComponent.h"      // replicated stake / payout / rating delta
#include "Round/AFLRoundManagerComponent.h"
#include "TimerManager.h"                       // auto-return countdown
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

	// Economy result. Paint whatever is known NOW (usually just the stake -- escrow confirmed at match start),
	// then subscribe: settle and rating answer seconds after this board is already up, so the board has to
	// repaint on arrival rather than read once and freeze on a pending state.
	RefreshOutcome();
	if (UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::Find(this))
	{
		OutcomeChangedHandle = Outcome->OnOutcomesChanged.AddUObject(this, &UAFLW_MatchScoreboard::RefreshOutcome);
	}

	// Arm the auto-return. The board must not depend on someone pressing a button to end the match: a player
	// who alt-tabs at the results screen would otherwise hold the server open indefinitely. That is not
	// hypothetical -- before this widget was cooked at all there was no CONTINUE either, and one concluded
	// match held its GameLift session for 6.5 hours.
	bReturning = false;
	SecondsRemaining = FMath::Max(0, AutoReturnSeconds);
	if (SecondsRemaining > 0)
	{
		if (UWorld* World = GetWorld())
		{
			TickAutoReturn();   // paint the first number NOW -- a 1s blank label reads as broken
			World->GetTimerManager().SetTimer(AutoReturnTimer, this,
				&UAFLW_MatchScoreboard::TickAutoReturn, 1.0f, /*loop=*/true);
		}
	}
	else if (CountdownText)
	{
		CountdownText->SetVisibility(ESlateVisibility::Collapsed);   // disabled -> no stale "0" on screen
	}
}

void UAFLW_MatchScoreboard::NativeOnDeactivated()
{
	// Release the outcome subscription. The component lives on the GameState and outlives this widget, so a
	// handle left bound would fire into a dead board on the next match's writes.
	if (OutcomeChangedHandle.IsValid())
	{
		if (UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::Find(this))
		{
			Outcome->OnOutcomesChanged.Remove(OutcomeChangedHandle);
		}
		OutcomeChangedHandle.Reset();
	}

	// Kill the countdown FIRST. A board dismissed by any route other than our own travel (a widget pushed
	// over the top, a travel someone else initiated) must not leave a live timer that later yanks the player
	// out of wherever they ended up. The timer's lifetime is the board's, not the session's.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoReturnTimer);
	}

	// Restore the HUD (moot on a real CONTINUE -> we travel to the hub, but clean for any other dismissal).
	SetHUDHidden(false);
	Super::NativeOnDeactivated();
}

void UAFLW_MatchScoreboard::RefreshOutcome()
{
	// Nothing bound on the WBP yet -> nothing to paint. Cheap early-out so this stays free until the labels
	// are added, rather than doing lookups whose results are discarded.
	if (!StakeText && !PayoutText && !RatingDeltaText)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* MyState = PC ? PC->PlayerState : nullptr;
	const UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::Find(this);
	const FAFLPlayerOutcome* Mine = (Outcome && MyState) ? Outcome->FindOutcome(MyState) : nullptr;

	// PENDING vs ZERO. A service that has not answered shows a dash; only a service that HAS answered may
	// print a number. Collapsing the two would tell a player they were paid nothing while the settle call is
	// still in flight -- a false statement about their money, made confidently.
	static const FText Pending = NSLOCTEXT("AFL", "ScoreboardPending", "-");

	if (StakeText)
	{
		StakeText->SetText((Mine && Mine->Stake > 0) ? FText::AsNumber(Mine->Stake) : Pending);
	}
	if (PayoutText)
	{
		PayoutText->SetText((Mine && Mine->bHasSettle) ? FText::AsNumber(Mine->Payout) : Pending);
	}
	if (RatingDeltaText)
	{
		if (Mine && Mine->bHasRating)
		{
			// Signed, always -- "+2.75" and "-1.94" read as rating movement; a bare "2.75" does not.
			FNumberFormattingOptions Fmt;
			Fmt.SetMinimumFractionalDigits(2);
			Fmt.SetMaximumFractionalDigits(2);
			Fmt.SetAlwaysSign(true);
			RatingDeltaText->SetText(FText::AsNumber(Mine->RatingDelta, &Fmt));
		}
		else
		{
			RatingDeltaText->SetText(Pending);
		}
	}
}

void UAFLW_MatchScoreboard::TickAutoReturn()
{
	if (bReturning)
	{
		return;   // CONTINUE already won the race -- let the travel it started finish
	}

	if (CountdownText)
	{
		CountdownText->SetText(FText::Format(
			NSLOCTEXT("AFL", "ScoreboardAutoReturn", "RETURNING IN {0}"), FText::AsNumber(SecondsRemaining)));
	}

	if (SecondsRemaining <= 0)
	{
		BeginReturnToHub();
		return;
	}
	--SecondsRemaining;
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
	BeginReturnToHub();   // CONTINUE just skips the wait; the return itself is one path (guarded below)
}

void UAFLW_MatchScoreboard::BeginReturnToHub()
{
	// GUARD. Two independent triggers now reach here -- the CONTINUE button and the countdown -- and a player
	// pressing CONTINUE on the final countdown second hits both. ReturnToMainMenu tears down the session and
	// travels; calling it twice tears down a teardown. This is cheap insurance against a race that is rare,
	// input-timing dependent, and therefore exactly the kind that survives testing and ships.
	if (bReturning)
	{
		return;
	}
	bReturning = true;

	// Stop the countdown before travelling, so a queued tick cannot fire during teardown.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoReturnTimer);
	}

	// Framework clean return: session teardown + travel to the front-end (IRONICS hub). Closes AFL-1911.
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_SCOREBOARD: returning to hub (ReturnToMainMenu)."));
			GI->ReturnToMainMenu();
		}
	}
}
