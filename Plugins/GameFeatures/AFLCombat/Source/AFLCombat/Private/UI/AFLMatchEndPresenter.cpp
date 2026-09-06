// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLMatchEndPresenter.h"

#include "AFLCombat.h"
#include "BattleRoyale/AFLBattleRoyaleComponent.h"   // the mode probe: BR present on the GameState -> BR card
#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Cook/AFLCookedAssetRegistry.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "UI/AFLW_BRResult.h"
#include "UI/AFLW_MatchScoreboard.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchEndPresenter)

namespace
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Match_Ended_Presenter, "Event.Match.Ended");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Menu_Presenter, "UI.Layer.Menu");
}

// Declared AND enrolled for cook validation in one statement. This widget was absent from every
// packaged build until 07ff32d8 -- named only by a literal here, so the cooker never packaged it.
// A finished match then had no results board, no CONTINUE button and no way back to the lobby,
// and sat in PostGame indefinitely; one instance held a GameLift session for 6.5 hours. The load
// below happens at MATCH END, minutes into a session -- enrolling it moves detection to startup.
AFL_COOKED_ASSET(GMatchScoreboardWidget,
	TEXT("/AFLBagMan/UI/WBP_AFL_MatchScoreboard.WBP_AFL_MatchScoreboard_C"));

// The Battle Royale card (placement / match length / eliminations). Same enrolment discipline.
AFL_COOKED_ASSET(GBRResultWidget,
	TEXT("/AFLBagMan/UI/WBP_AFL_BRResult.WBP_AFL_BRResult_C"));

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

TSoftClassPtr<UCommonActivatableWidget> UAFLMatchEndPresenter::ResolveResultsWidgetClass(bool& bOutIsBattleRoyale) const
{
	// The mode is a fact of the GameState, not of this component: the BR structure layer lives there
	// (replicated, client-visible), so probing it is safe on every client at match-end.
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	bOutIsBattleRoyale = (GS && GS->FindComponentByClass<UAFLBattleRoyaleComponent>() != nullptr);
	return bOutIsBattleRoyale
		? GBRResultWidget.ToSoftClassPtr<UCommonActivatableWidget>()
		: GMatchScoreboardWidget.ToSoftClassPtr<UCommonActivatableWidget>();
}

void UAFLMatchEndPresenter::PushResults()
{
	if (bPushed) { return; }   // one takeover per match

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LP) { return; }

	bool bBattleRoyale = false;
	TSubclassOf<UCommonActivatableWidget> LoadedClass = ResolveResultsWidgetClass(bBattleRoyale).LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MATCHEND: results takeover WBP (%s) failed to load; no board shown."),
			bBattleRoyale ? TEXT("BR") : TEXT("scoreboard"));
		return;
	}

	bPushed = true;
	UCommonActivatableWidget* Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, TAG_UI_Layer_Menu_Presenter, LoadedClass);
	// Hand in the collected EARNED + render. The HUD-hide happens on the widget's activation.
	if (UAFLW_BRResult* BRCard = Cast<UAFLW_BRResult>(Pushed))
	{
		BRCard->ShowResults(EarnedWatts);
	}
	else if (UAFLW_MatchScoreboard* Board = Cast<UAFLW_MatchScoreboard>(Pushed))
	{
		Board->ShowResults(EarnedWatts);
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_MATCHEND: pushed %s takeover (%s)."),
		bBattleRoyale ? TEXT("BR result") : TEXT("match scoreboard"), *GetNameSafe(Pushed));
}
