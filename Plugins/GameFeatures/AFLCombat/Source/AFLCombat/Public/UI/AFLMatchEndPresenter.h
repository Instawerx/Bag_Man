// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "AFLMatchEndPresenter.generated.h"

class APlayerState;
class UAFLW_MatchScoreboard;

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

	/** The results takeover WBP (a UCommonActivatableWidget child); soft so it lazy-loads at match-end. */
	TSoftClassPtr<UAFLW_MatchScoreboard> ResultsWidgetClass;

	TMap<TWeakObjectPtr<APlayerState>, int32> EarnedWatts;
	FGameplayMessageListenerHandle MatchEndedListener;
	FTimerHandle CoalesceTimer;
	bool bPushed = false;
};
