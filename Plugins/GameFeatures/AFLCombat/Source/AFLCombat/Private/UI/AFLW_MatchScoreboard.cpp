// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_MatchScoreboard.h"

#include "AFLCombat.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "Player/LyraPlayerState.h"
#include "Round/AFLRoundManagerComponent.h"
#include "Teams/LyraTeamSubsystem.h"
#include "TimerManager.h"
#include "UI/AFLW_ScoreboardRow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_MatchScoreboard)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Match_Ended_Scoreboard, "Event.Match.Ended");

namespace
{
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

void UAFLW_MatchScoreboard::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);   // hidden until the match concludes

	if (UWorld* World = GetWorld())
	{
		MatchEndedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
			TAG_Event_Match_Ended_Scoreboard,
			[this](FGameplayTag Channel, const FLyraVerbMessage& Msg) { HandleMatchEnded(Channel, Msg); });
	}
	TryArm();
}

void UAFLW_MatchScoreboard::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmRetryTimer);
		World->GetTimerManager().ClearTimer(RebuildTimer);
	}
	if (MatchEndedListener.IsValid()) { MatchEndedListener.Unregister(); }
	Super::NativeDestruct();
}

void UAFLW_MatchScoreboard::TryArm()
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	UAFLRoundManagerComponent* Resolved = GS ? GS->FindComponentByClass<UAFLRoundManagerComponent>() : nullptr;
	if (!Resolved)
	{
		// The GameState round component can arrive after construct -- bounded poll (mirrors the round header/toast).
		if (World)
		{
			World->GetTimerManager().SetTimer(ArmRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { TryArm(); }), 0.5f, false);
		}
		return;
	}
	Round = Resolved;

	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			if (const ULyraTeamSubsystem* Teams = World->GetSubsystem<ULyraTeamSubsystem>())
			{
				const int32 TeamSlot = Resolved->SlotForTeam(Teams->FindTeamFromObject(PS));
				if (TeamSlot != INDEX_NONE) { LocalSlot = TeamSlot; }
			}
		}
	}
}

void UAFLW_MatchScoreboard::HandleMatchEnded(FGameplayTag /*Channel*/, const FLyraVerbMessage& Msg)
{
	// The driver fires this ONCE PER PLAYER at conclusion (Target = PlayerState, Magnitude = this-match
	// Watts; multicast to all clients + a local host broadcast). Receiving it IS the match-end gate -- it
	// only ever fires at PostGame, so no separate Phase-property gate (which would risk a rep-order race
	// dropping the board). Collect each player's EARNED, then coalesce a single paint ~50ms after the LAST
	// message, so all N messages + the replicated scores/StatTags settle first (no per-message flicker).
	if (APlayerState* PS = Cast<APlayerState>(Msg.Target))
	{
		EarnedWatts.Add(PS, FMath::RoundToInt(Msg.Magnitude));
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebuildTimer);
		World->GetTimerManager().SetTimer(RebuildTimer,
			FTimerDelegate::CreateWeakLambda(this, [this] { RebuildBoard(); }), 0.05f, false);
	}
	else
	{
		RebuildBoard();
	}
}

void UAFLW_MatchScoreboard::RebuildBoard()
{
	UWorld* World = GetWorld();
	UAFLRoundManagerComponent* R = Round.Get();
	if (!R)   // resolve late if the construct-time arm hasn't landed yet
	{
		AGameStateBase* GSResolve = World ? World->GetGameState() : nullptr;
		R = GSResolve ? GSResolve->FindComponentByClass<UAFLRoundManagerComponent>() : nullptr;
		if (R) { Round = R; }
	}
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!R || !GS) { return; }

	const ULyraTeamSubsystem* Teams = World->GetSubsystem<ULyraTeamSubsystem>();

	// Late local-slot resolve (mirrors the header/toast fallback; by match-end the team is known).
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

	// -- PER-PLAYER ROWS (PlayerArray; K/D/A from replicated StatTags, EARNED from the collected map) --
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

	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!bShown)
	{
		bShown = true;
		OnBoardShown(ScoreSlot, bVictory);
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_SCOREBOARD: shown outcome=%s score=%d-%d players=%d"),
			bDraw ? TEXT("DRAW") : (bVictory ? TEXT("VICTORY") : TEXT("DEFEAT")),
			MyScore, EnemyScore, GS->PlayerArray.Num());
	}
}
