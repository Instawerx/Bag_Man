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

	ResolveLocalTeam();   // best-effort warm read; the authoritative one happens at resolve time

	// OnRoundResolved fires server-side at resolve and on clients via OnRep_RoundResolved -- the client HUD
	// hears every round outcome with no new replication.
	ResolvedHandle = Round->OnRoundResolved.AddUObject(this, &UAFLW_RoundResultToast::HandleRoundResolved);
}

int32 UAFLW_RoundResultToast::ResolveLocalTeam()
{
	const UWorld* World = GetWorld();
	const ULyraTeamSubsystem* Teams = World ? World->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;
	if (Teams && PS)
	{
		const int32 Found = Teams->FindTeamFromObject(PS);
		if (Found != INDEX_NONE)
		{
			LocalTeam = Found;   // GUARD: only ever overwrite with a REAL team. A miss must not poison it.
		}
	}
	return LocalTeam;
}

void UAFLW_RoundResultToast::HandleRoundResolved(int32 WinningTeam, EAFLRoundWinReason Reason)
{
	// RE-RESOLVE NOW. The construct-time read is taken before the team is assigned/replicated, so it
	// reported INDEX_NONE and every round -- including wins -- printed ROUND LOST. Log-proven: the local
	// player was team 1 and won rounds 1 and 2 (score 2-0) while the toast read LOST both times. By
	// resolve time the team is always known, which is the same reasoning UAFLW_MatchScoreboard:111 states.
	const int32 MyTeam = ResolveLocalTeam();

	FText Result;
	FLinearColor Col;
	if (WinningTeam == INDEX_NONE)
	{
		Result = NSLOCTEXT("AFL", "RrDraw", "ROUND DRAW");
		Col = DrawColor;
	}
	else if (MyTeam == INDEX_NONE)
	{
		// Still unknown at resolve time -- that is a real defect, not a draw. Say so instead of guessing
		// a side; a silent wrong answer is what hid this for as long as it did.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_TOAST: local team UNRESOLVED at round resolve (winner=%d) -- cannot state an outcome."),
			WinningTeam);
		Result = NSLOCTEXT("AFL", "RrDraw", "ROUND DRAW");
		Col = DrawColor;
	}
	else if (WinningTeam == MyTeam)
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

	// The numbers are the proof, same as the phase/join coverage counts: winner and local team side by
	// side make an inverted or unresolved toast readable straight off the log.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_TOAST: winner=%d local=%d -> '%s'."),
		WinningTeam, MyTeam, *Result.ToString());

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
