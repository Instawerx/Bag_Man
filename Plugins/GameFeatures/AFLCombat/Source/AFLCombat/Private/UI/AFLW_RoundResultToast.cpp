// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_RoundResultToast.h"

#include "AFLCombat.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Teams/LyraTeamSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_RoundResultToast)

namespace
{
	FText ReasonToText(EAFLRoundWinReason Reason)
	{
		switch (Reason)
		{
		case EAFLRoundWinReason::Elimination: return NSLOCTEXT("AFL", "RrElim", "ELIMINATION");
		case EAFLRoundWinReason::Extraction:  return NSLOCTEXT("AFL", "RrExtract", "EXTRACTION");
		case EAFLRoundWinReason::Timeout:     return NSLOCTEXT("AFL", "RrTimeout", "TIMEOUT");
		case EAFLRoundWinReason::Replay:      return NSLOCTEXT("AFL", "RrReplay", "REPLAY");
		default: return FText::GetEmpty();
		}
	}
}

void UAFLW_RoundResultToast::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);   // transient -- hidden until a round resolves
	TryArm();
}

void UAFLW_RoundResultToast::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmRetryTimer);
		World->GetTimerManager().ClearTimer(HoldTimer);
	}
	if (Round.IsValid() && ResolvedHandle.IsValid())
	{
		Round->OnRoundResolved.Remove(ResolvedHandle);
	}
	Super::NativeDestruct();
}

void UAFLW_RoundResultToast::TryArm()
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	UAFLRoundManagerComponent* Resolved = GS ? GS->FindComponentByClass<UAFLRoundManagerComponent>() : nullptr;
	if (!Resolved)
	{
		// The GameState round component can arrive after construct -- bounded poll (mirrors the round header).
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
				LocalTeam = Teams->FindTeamFromObject(PS);
			}
		}
	}

	// OnRoundResolved fires server-side at resolve and on clients via OnRep_RoundResolved -- the client HUD
	// hears every round outcome with no new replication.
	ResolvedHandle = Round->OnRoundResolved.AddUObject(this, &UAFLW_RoundResultToast::HandleRoundResolved);
}

void UAFLW_RoundResultToast::HandleRoundResolved(int32 WinningTeam, EAFLRoundWinReason Reason)
{
	FText Result;
	FLinearColor Col;
	if (WinningTeam == INDEX_NONE)
	{
		Result = NSLOCTEXT("AFL", "RrDraw", "ROUND DRAW");
		Col = DrawColor;
	}
	else if (WinningTeam == LocalTeam)
	{
		Result = NSLOCTEXT("AFL", "RrWon", "ROUND WON");
		Col = WonColor;
	}
	else
	{
		Result = NSLOCTEXT("AFL", "RrLost", "ROUND LOST");
		Col = LostColor;
	}

	if (ResultText)
	{
		ResultText->SetText(Result);
		ResultText->SetColorAndOpacity(FSlateColor(Col));
	}
	if (ReasonText)
	{
		ReasonText->SetText(ReasonToText(Reason));
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	OnToastShown();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HoldTimer,
			FTimerDelegate::CreateWeakLambda(this, [this] { Hide(); }), HoldSeconds, false);
	}
}

void UAFLW_RoundResultToast::Hide()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
