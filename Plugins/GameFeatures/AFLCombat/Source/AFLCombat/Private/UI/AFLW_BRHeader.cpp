// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_BRHeader.h"

#include "AFLCombat.h"
#include "BattleRoyale/AFLBattleRoyaleComponent.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/LyraPlayerState.h"   // replicated StatTags -- the client-visible placement
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_BRHeader)

void UAFLW_BRHeader::NativeConstruct()
{
	Super::NativeConstruct();
	TryArm();
}

void UAFLW_BRHeader::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmRetryTimer);
	}
	Super::NativeDestruct();
}

void UAFLW_BRHeader::TryArm()
{
	UWorld* World = GetWorld();
	AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	UAFLBattleRoyaleComponent* Resolved = GS ? GS->FindComponentByClass<UAFLBattleRoyaleComponent>() : nullptr;
	if (!Resolved)
	{
		// The GameState (and its replicated BR component) can arrive after widget construct -- bounded retry.
		if (World)
		{
			World->GetTimerManager().SetTimer(ArmRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { TryArm(); }), 0.5f, false);
		}
		return;
	}
	BR = Resolved;
	Refresh();
}

void UAFLW_BRHeader::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (BR.IsValid())
	{
		Refresh();
	}
}

void UAFLW_BRHeader::Refresh()
{
	UAFLBattleRoyaleComponent* B = BR.Get();
	if (!B)
	{
		return;
	}

	// COUNT-UP clock: elapsed match seconds (frozen at match end = the recorded length). FLOOR, not ceil --
	// an elapsed clock reads 0:00 at start and ticks up as each whole second passes. Re-text only on change.
	const int32 ClockSec = FMath::FloorToInt(FMath::Max(0.0f, B->GetElapsedMatchSeconds()));
	if (ClockSec != LastClockSec)
	{
		LastClockSec = ClockSec;
		if (TimeText)
		{
			TimeText->SetText(FormatClock(static_cast<float>(ClockSec)));
			TimeText->SetColorAndOpacity(FSlateColor(TimeColor));
		}
	}

	// Alive / Total.
	if (B->AlivePlayers != LastAlive || B->TotalParticipants != LastTotal)
	{
		const bool bAliveMoved = (B->AlivePlayers != LastAlive);
		LastAlive = B->AlivePlayers;
		LastTotal = B->TotalParticipants;
		if (AliveText)
		{
			AliveText->SetText(FText::Format(NSLOCTEXT("AFL", "BRAlive", "{0} / {1}"),
				FText::AsNumber(B->AlivePlayers), FText::AsNumber(B->TotalParticipants)));
		}
		if (bAliveMoved)
		{
			OnAliveChanged(B->AlivePlayers);
		}
	}

	// Placement of the local player. 0 = not yet eliminated -> a dash until the ladder books them.
	// CLIENT-VISIBLE: the component's Placements map is server-only, so read the replicated StatTag the server
	// writes at booking (TAG_AFL_Stat_BR_Placement); the server map is only a fallback for a listen host.
	int32 Rank = 0;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			if (const ALyraPlayerState* LPS = Cast<ALyraPlayerState>(PS))
			{
				Rank = LPS->GetStatTagStackCount(TAG_AFL_Stat_BR_Placement);
			}
			if (Rank <= 0)
			{
				Rank = B->GetPlacementForPlayer(PS);
			}
		}
	}
	if (Rank != LastRank)
	{
		LastRank = Rank;
		if (RankText)
		{
			RankText->SetText(Rank > 0
				? FText::Format(NSLOCTEXT("AFL", "BRRank", "#{0}"), FText::AsNumber(Rank))
				: NSLOCTEXT("AFL", "BRRankNone", "--"));
		}
	}
}

FText UAFLW_BRHeader::FormatClock(float Seconds)
{
	const int32 Total = FMath::FloorToInt(FMath::Max(0.0f, Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
}
