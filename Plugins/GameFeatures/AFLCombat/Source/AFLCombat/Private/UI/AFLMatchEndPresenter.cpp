// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLMatchEndPresenter.h"

#include "AFLCombat.h"
#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "UI/AFLW_MatchScoreboard.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchEndPresenter)

namespace
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Match_Ended_Presenter, "Event.Match.Ended");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Menu_Presenter, "UI.Layer.Menu");
}

UAFLMatchEndPresenter::UAFLMatchEndPresenter()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLMatchEndPresenter::BeginPlay()
{
	Super::BeginPlay();

	// Per-local-player UI: only the locally-controlled PlayerController pushes (skips remote PCs on the server).
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// The AFL results takeover WBP (styled child of UAFLW_MatchScoreboard). Soft ref -> lazy-loaded on match-end.
	ResultsWidgetClass = TSoftClassPtr<UAFLW_MatchScoreboard>(
		FSoftObjectPath(TEXT("/AFLBagMan/UI/WBP_AFL_MatchScoreboard.WBP_AFL_MatchScoreboard_C")));

	// Reuse the proven trigger: the per-player Event.Match.Ended broadcast (fires once per player at PostGame).
	if (UWorld* World = GetWorld())
	{
		MatchEndedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
			TAG_Event_Match_Ended_Presenter,
			[this](FGameplayTag Channel, const FLyraVerbMessage& Msg) { HandleMatchEnded(Channel, Msg); });
	}
}

void UAFLMatchEndPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MatchEndedListener.IsValid()) { MatchEndedListener.Unregister(); }
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(CoalesceTimer); }
	Super::EndPlay(EndPlayReason);
}

void UAFLMatchEndPresenter::HandleMatchEnded(FGameplayTag /*Channel*/, const FLyraVerbMessage& Msg)
{
	// Collect each player's EARNED (Target = PlayerState, Magnitude = this-match Watts), then coalesce a single
	// push ~50ms after the LAST message (so all N messages + the replicated scores/StatTags settle first).
	if (APlayerState* PS = Cast<APlayerState>(Msg.Target))
	{
		EarnedWatts.Add(PS, FMath::RoundToInt(Msg.Magnitude));
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CoalesceTimer);
		World->GetTimerManager().SetTimer(CoalesceTimer,
			FTimerDelegate::CreateWeakLambda(this, [this] { PushResults(); }), 0.05f, false);
	}
	else
	{
		PushResults();
	}
}

void UAFLMatchEndPresenter::PushResults()
{
	if (bPushed) { return; }   // one takeover per match

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LP) { return; }

	TSubclassOf<UAFLW_MatchScoreboard> LoadedClass = ResultsWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MATCHEND: results takeover WBP failed to load; no board shown."));
		return;
	}

	bPushed = true;
	UCommonActivatableWidget* Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, TAG_UI_Layer_Menu_Presenter, LoadedClass);
	if (UAFLW_MatchScoreboard* Board = Cast<UAFLW_MatchScoreboard>(Pushed))
	{
		// Hand in the collected EARNED + render. The HUD-hide happens on the widget's activation.
		Board->ShowResults(EarnedWatts);
	}
}
