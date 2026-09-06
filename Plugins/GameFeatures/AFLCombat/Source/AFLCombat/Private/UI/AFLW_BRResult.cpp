// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_BRResult.h"

#include "AFLCombat.h"
#include "BattleRoyale/AFLBattleRoyaleComponent.h"
#include "CommonButtonBase.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "Input/CommonUIInputTypes.h"
#include "Match/AFLMatchOutcomeComponent.h"      // replicated stake / payout / rating delta
#include "NativeGameplayTags.h"
#include "Player/LyraPlayerState.h"
#include "PrimaryGameLayout.h"
#include "TimerManager.h"                       // auto-return countdown
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_BRResult)

namespace
{
	// The in-match HUD lives on UI.Layer.Game; the takeover collapses it (Apex-style) while active.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Game_BRResult, "UI.Layer.Game");
}

TOptional<FUIInputConfig> UAFLW_BRResult::GetDesiredInputConfig() const
{
	// Menu input while the takeover owns the screen -> CONTINUE is clickable + the cursor is visible.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UAFLW_BRResult::ShowResults(const TMap<TWeakObjectPtr<APlayerState>, int32>& InEarnedWatts)
{
	EarnedWatts = InEarnedWatts;
	RebuildCard();
}

void UAFLW_BRResult::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Apex/ARC-style takeover: hide the in-match HUD so only the results own the screen.
	SetHUDHidden(true);

	// Wire CONTINUE once. Menu input mode is driven by GetDesiredInputConfig (the CommonUI activatable path).
	if (ContinueButton && !bContinueBound)
	{
		bContinueBound = true;
		ContinueButton->OnClicked().AddUObject(this, &UAFLW_BRResult::HandleContinueClicked);
	}

	// Economy result: paint what is known NOW, then subscribe -- settle and rating answer seconds after this
	// card is already up, so it repaints on arrival rather than reading once and freezing on a pending state.
	RefreshOutcome();
	if (UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::Find(this))
	{
		OutcomeChangedHandle = Outcome->OnOutcomesChanged.AddUObject(this, &UAFLW_BRResult::RefreshOutcome);
	}

	// Arm the auto-return: a finished match must never depend on someone pressing a button to end.
	bReturning = false;
	SecondsRemaining = FMath::Max(0, AutoReturnSeconds);
	if (SecondsRemaining > 0)
	{
		if (UWorld* World = GetWorld())
		{
			TickAutoReturn();   // paint the first number NOW -- a 1s blank label reads as broken
			World->GetTimerManager().SetTimer(AutoReturnTimer, this, &UAFLW_BRResult::TickAutoReturn, 1.0f, /*loop=*/true);
		}
	}
	else if (CountdownText)
	{
		CountdownText->SetVisibility(ESlateVisibility::Collapsed);   // disabled -> no stale "0" on screen
	}
}

void UAFLW_BRResult::NativeOnDeactivated()
{
	// Release the outcome subscription -- the component lives on the GameState and outlives this widget.
	if (OutcomeChangedHandle.IsValid())
	{
		if (UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::Find(this))
		{
			Outcome->OnOutcomesChanged.Remove(OutcomeChangedHandle);
		}
		OutcomeChangedHandle.Reset();
	}

	// Kill the countdown FIRST: a card dismissed by any route other than our own travel must not leave a live
	// timer that later yanks the player out of wherever they ended up.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoReturnTimer);
	}

	SetHUDHidden(false);
	Super::NativeOnDeactivated();
}

void UAFLW_BRResult::RefreshOutcome()
{
	if (!StakeText && !PayoutText && !RatingDeltaText)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* MyState = PC ? PC->PlayerState : nullptr;
	const UAFLMatchOutcomeComponent* Outcome = UAFLMatchOutcomeComponent::Find(this);
	const FAFLPlayerOutcome* Mine = (Outcome && MyState) ? Outcome->FindOutcome(MyState) : nullptr;

	// PENDING vs ZERO: a service that has not answered shows a dash; only one that HAS answered prints a number.
	static const FText Pending = NSLOCTEXT("AFL", "BRResultPending", "-");

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

void UAFLW_BRResult::TickAutoReturn()
{
	if (bReturning)
	{
		return;   // CONTINUE already won the race -- let the travel it started finish
	}
	if (CountdownText)
	{
		CountdownText->SetText(FText::Format(
			NSLOCTEXT("AFL", "BRResultAutoReturn", "RETURNING IN {0}"), FText::AsNumber(SecondsRemaining)));
	}
	if (SecondsRemaining <= 0)
	{
		BeginReturnToHub();
		return;
	}
	--SecondsRemaining;
}

UWidget* UAFLW_BRResult::NativeGetDesiredFocusTarget() const
{
	return ContinueButton;
}

void UAFLW_BRResult::SetHUDHidden(bool bHidden)
{
	if (UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayoutForPrimaryPlayer(this))
	{
		if (UCommonActivatableWidgetContainerBase* GameLayer = Layout->GetLayerWidget(TAG_UI_Layer_Game_BRResult))
		{
			GameLayer->SetVisibility(bHidden ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
	}
}

FText UAFLW_BRResult::FormatClock(float Seconds)
{
	const int32 Total = FMath::FloorToInt(FMath::Max(0.0f, Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
}

void UAFLW_BRResult::RebuildCard()
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	UAFLBattleRoyaleComponent* B = BR.Get();
	if (!B && GS)   // resolve on demand (pushed at match-end; the BR component is present by then)
	{
		B = GS->FindComponentByClass<UAFLBattleRoyaleComponent>();
		if (B) { BR = B; }
	}
	if (!B || !GS) { return; }

	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* MyPS = PC ? PC->PlayerState : nullptr;
	const ALyraPlayerState* MyLPS = Cast<ALyraPlayerState>(MyPS);
	const int32 MyId = MyPS ? MyPS->GetPlayerId() : INDEX_NONE;

	// -- VERDICT (from the replicated winner id; INDEX_NONE = no survivor / draw) --
	const bool bDraw    = (B->WinnerPlayerId == INDEX_NONE);
	const bool bVictory = (!bDraw && MyId != INDEX_NONE && B->WinnerPlayerId == MyId);

	// -- PLACEMENT: the replicated StatTag first (client-visible), the server map as a listen-host fallback --
	int32 Placement = MyLPS ? MyLPS->GetStatTagStackCount(TAG_AFL_Stat_BR_Placement) : 0;
	if (Placement <= 0 && MyPS) { Placement = B->GetPlacementForPlayer(MyPS); }
	if (Placement <= 0 && bVictory) { Placement = 1; }
	const int32 Total = FMath::Max(B->TotalParticipants, Placement);

	// -- WINNER NAME (for the eliminated subline) --
	FString WinnerName;
	if (!bDraw)
	{
		for (const APlayerState* PS : GS->PlayerArray)
		{
			if (PS && PS->GetPlayerId() == B->WinnerPlayerId) { WinnerName = PS->GetPlayerName(); break; }
		}
	}

	if (VerdictText)
	{
		FText OutText; FLinearColor OutCol;
		if (bDraw)         { OutText = NSLOCTEXT("AFL", "BRDraw", "DRAW");            OutCol = DrawColor; }
		else if (bVictory) { OutText = NSLOCTEXT("AFL", "BRWin", "VICTORY");          OutCol = VictoryColor; }
		else               { OutText = NSLOCTEXT("AFL", "BRLose", "ELIMINATED");      OutCol = EliminatedColor; }
		VerdictText->SetText(OutText);
		VerdictText->SetColorAndOpacity(FSlateColor(OutCol));
	}
	if (PlacementText)
	{
		PlacementText->SetText(Placement > 0
			? FText::Format(NSLOCTEXT("AFL", "BRPlace", "#{0}"), FText::AsNumber(Placement))
			: NSLOCTEXT("AFL", "BRPlaceNone", "--"));
	}
	if (OfText)
	{
		OfText->SetText(FText::Format(NSLOCTEXT("AFL", "BROf", "OF {0}"), FText::AsNumber(Total)));
	}
	if (SublineText)
	{
		if (bDraw)         { SublineText->SetText(NSLOCTEXT("AFL", "BRSubDraw", "NO SURVIVOR")); }
		else if (bVictory) { SublineText->SetText(NSLOCTEXT("AFL", "BRSubWin", "LAST ONE STANDING")); }
		else               { SublineText->SetText(FText::Format(NSLOCTEXT("AFL", "BRSubLose", "WINNER · {0}"),
		                        FText::FromString(WinnerName.IsEmpty() ? TEXT("?") : WinnerName))); }
	}
	if (PlacementTileText)
	{
		PlacementTileText->SetText(Placement > 0
			? FText::Format(NSLOCTEXT("AFL", "BRPlaceTile", "#{0}/{1}"), FText::AsNumber(Placement), FText::AsNumber(Total))
			: NSLOCTEXT("AFL", "BRPlaceTileNone", "--"));
	}
	if (MatchLengthText)
	{
		MatchLengthText->SetText(FormatClock(B->GetElapsedMatchSeconds()));   // frozen at match end = the length
	}

	// -- ELIMINATIONS (replicated ShooterCore StatTag; ErrorIfNotFound=false so an unwired scorer reads 0) --
	if (EliminationsText)
	{
		static const FGameplayTag KillTag = FGameplayTag::RequestGameplayTag(FName("ShooterGame.Score.Eliminations"), false);
		const int32 K = (MyLPS && KillTag.IsValid()) ? MyLPS->GetStatTagStackCount(KillTag) : 0;
		EliminationsText->SetText(FText::AsNumber(K));
	}

	// -- WATTS EARNED (handed in by the presenter; signed) --
	if (EarnedText)
	{
		const int32* EarnedPtr = MyPS ? EarnedWatts.Find(MyPS) : nullptr;
		FNumberFormattingOptions Fmt;
		Fmt.SetAlwaysSign(true);
		EarnedText->SetText(FText::AsNumber(EarnedPtr ? *EarnedPtr : 0, &Fmt));
	}

	OnResultShown(Placement, bVictory);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_BRRESULT: takeover shown verdict=%s placement=%d/%d length=%.0fs winner=%d"),
		bDraw ? TEXT("DRAW") : (bVictory ? TEXT("VICTORY") : TEXT("ELIMINATED")),
		Placement, Total, B->GetElapsedMatchSeconds(), B->WinnerPlayerId);
}

void UAFLW_BRResult::HandleContinueClicked()
{
	BeginReturnToHub();
}

void UAFLW_BRResult::BeginReturnToHub()
{
	// GUARD: CONTINUE and the countdown can coincide on the final second; ReturnToMainMenu twice tears down a
	// teardown. The second caller is a no-op.
	if (bReturning)
	{
		return;
	}
	bReturning = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoReturnTimer);
	}
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_BRRESULT: returning to hub (ReturnToMainMenu)."));
			GI->ReturnToMainMenu();
		}
	}
}
