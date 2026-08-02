// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_RoundHeader.h"

#include "AFLCombat.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Round/AFLRoundManagerComponent.h"
#include "Teams/LyraTeamSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_RoundHeader)


void UAFLW_RoundHeader::NativeConstruct()
{
	Super::NativeConstruct();
	TryArm();
}

void UAFLW_RoundHeader::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmRetryTimer);
	}
	Super::NativeDestruct();
}

void UAFLW_RoundHeader::TryArm()
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	UAFLRoundManagerComponent* Resolved = GS ? GS->FindComponentByClass<UAFLRoundManagerComponent>() : nullptr;
	if (!Resolved)
	{
		// The GameState (and its replicated round component) can arrive after widget construct.
		// Bounded retry poll -- mirrors UAFLW_EnergyMeter::TryArm's possession-race handling.
		if (World)
		{
			World->GetTimerManager().SetTimer(ArmRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { TryArm(); }), 0.5f, false);
		}
		return;
	}
	Round = Resolved;

	// Map the local player to its score slot (my-vs-enemy). Falls back to slot 0 = "mine" if the
	// team subsystem isn't ready yet -- the header still reads, just without the my-side mapping.
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			if (const ULyraTeamSubsystem* Teams = World->GetSubsystem<ULyraTeamSubsystem>())
			{
				const int32 ResolvedSlot = Resolved->SlotForTeam(Teams->FindTeamFromObject(PS));
				if (ResolvedSlot != INDEX_NONE)
				{
					LocalSlot = ResolvedSlot;
				}
			}
		}
	}
	Refresh();
}

void UAFLW_RoundHeader::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Round.IsValid())
	{
		Refresh();
	}
}

void UAFLW_RoundHeader::Refresh()
{
	UAFLRoundManagerComponent* R = Round.Get();
	if (!R)
	{
		return;
	}

	// Round N / best-of target.
	const int32 CurRound = FMath::Max(1, R->CurrentRound);
	if (CurRound != LastRound)
	{
		LastRound = CurRound;
		if (RoundText)
		{
			// Round NUMBER only ("ROUND 8"). RoundsToWin (7) is the WIN TARGET (first-to-7, best-of-13),
			// NOT the round count -- "ROUND 8 / 7" mis-reads as "exceeded 7". The team-win score chips carry
			// the first-to-7 progress; the match ends when a score reaches RoundsToWin (Server_ResolveRound).
			RoundText->SetText(FText::Format(
				NSLOCTEXT("AFL", "RoundHeader", "ROUND {0}"),
				FText::AsNumber(CurRound)));
		}
		OnRoundChanged(CurRound);
	}

	// Scores: my slot vs enemy slot (slot 0 = "mine" when the local team is unresolved).
	const int32 ScoreSlot = (LocalSlot == INDEX_NONE) ? 0 : LocalSlot;
	const int32 MyScore = (ScoreSlot == 0) ? R->Team0Score : R->Team1Score;
	const int32 EnemyScore = (ScoreSlot == 0) ? R->Team1Score : R->Team0Score;
	if (MyScore != LastMyScore)
	{
		LastMyScore = MyScore;
		if (MyScoreText) { MyScoreText->SetText(FText::AsNumber(MyScore)); }
		OnMyScoreChanged(MyScore);
	}
	if (EnemyScore != LastEnemyScore)
	{
		LastEnemyScore = EnemyScore;
		if (EnemyScoreText) { EnemyScoreText->SetText(FText::AsNumber(EnemyScore)); }
		OnEnemyScoreChanged(EnemyScore);
	}

	// Clock while RoundActive, else the phase label. Re-text only on a phase or whole-second change.
	const uint8 PhaseByte = static_cast<uint8>(R->Phase);
	const int32 ClockSec = (R->Phase == EAFLRoundPhase::RoundActive)
		? FMath::CeilToInt(FMath::Max(0.0f, R->RoundTimeRemaining)) : -1;
	if (PhaseByte != LastPhase || ClockSec != LastClockSec)
	{
		LastPhase = PhaseByte;
		LastClockSec = ClockSec;
		if (TimerText)
		{
			FText Out;
			FLinearColor Col = TimerNormalColor;
			switch (R->Phase)
			{
			case EAFLRoundPhase::RoundActive:
				Out = FormatClock(R->RoundTimeRemaining);
				if (R->RoundTimeRemaining <= TimerLowThreshold) { Col = TimerLowColor; }
				break;
			case EAFLRoundPhase::WarmUp:   Out = NSLOCTEXT("AFL", "PhWarm", "WARMUP"); break;
			case EAFLRoundPhase::RoundEnd: Out = NSLOCTEXT("AFL", "PhEnd", "ROUND END"); break;
			case EAFLRoundPhase::HalfTime: Out = NSLOCTEXT("AFL", "PhHalf", "HALFTIME"); break;
			case EAFLRoundPhase::MatchEnd: Out = NSLOCTEXT("AFL", "PhMatch", "MATCH END"); break;
			default: break;
			}
			TimerText->SetText(Out);
			TimerText->SetColorAndOpacity(FSlateColor(Col));
		}
	}
}

FText UAFLW_RoundHeader::FormatClock(float Seconds)
{
	const int32 Total = FMath::CeilToInt(FMath::Max(0.0f, Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
}
