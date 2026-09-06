// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "AFLMatchEndPresenter.generated.h"

class APlayerState;
class UCommonActivatableWidget;

/**
 * UAFLMatchEndPresenter -- the client-side match-end results PUSHER (the takeover trigger).
 *
 * Added to the PlayerController via the experience's AddComponents action, so it is ACTIVATION-tied:
 * present as soon as AFLCombat activates. (A UWorldSubsystem would NOT work here -- AFLCombat is an
 * ExplicitlyLoaded GameFeature whose module loads on activation, AFTER the arena world initializes its
 * subsystems, so a world subsystem from it is never created for that world.)
 *
 * On the LOCAL controller only, it listens for the per-player Event.Match.Ended (the proven trigger),
 * collects each player's EARNED Watts, coalesces (~50ms), then pushes the results widget (a
 * UCommonActivatableWidget) full-screen onto UI.Layer.Menu and calls ShowResults(). This decouples the
 * match-end takeover from the HUD -- the board is no longer a content-sized HUD-slot overlay.
 *
 * MODE-AWARE CARD (2026-09-06): the card is resolved BY MODE at push time, not hardcoded. A Battle Royale
 * match (UAFLBattleRoyaleComponent on the GameState) gets WBP_AFL_BRResult (placement / match length /
 * eliminations); every team mode keeps WBP_AFL_MatchScoreboard. Before this, BR inherited the team
 * scoreboard -- "0 - 0", TEAM A / TEAM B and a raw "Text Block" placeholder on a free-for-all result.
 */
UCLASS()
class AFLCOMBAT_API UAFLMatchEndPresenter : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLMatchEndPresenter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleMatchEnded(FGameplayTag Channel, const struct FLyraVerbMessage& Msg);
	void PushResults();

	/** Pick the takeover class for THIS match's mode (resolved lazily at push -- the GameState and its mode
	 *  component are certainly present by match-end, which is not guaranteed at BeginPlay on a client). */
	TSoftClassPtr<UCommonActivatableWidget> ResolveResultsWidgetClass(bool& bOutIsBattleRoyale) const;

	TMap<TWeakObjectPtr<APlayerState>, int32> EarnedWatts;
	FGameplayMessageListenerHandle MatchEndedListener;
	FTimerHandle CoalesceTimer;
	bool bPushed = false;
};
