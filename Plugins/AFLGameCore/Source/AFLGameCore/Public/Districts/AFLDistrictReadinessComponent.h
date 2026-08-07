// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"
#include "LoadingProcessInterface.h"

#include "AFLDistrictReadinessComponent.generated.h"

/**
 * UAFLDistrictReadinessComponent  (hold the loading screen until the district's world is actually there)
 *
 * A district is a fenced play-space streamed in as a World Partition runtime data layer. Activating it is
 * ASYNCHRONOUS: UDataLayerManager::SetDataLayerRuntimeState flips the layer's *effective state* and returns
 * true on the spot, while the cells -- the fence, the cover, the collision -- arrive over later frames. The
 * gap is small (0.76s measured on L_ShantyTown) but it is a gap, and inside it a player is standing in a
 * play-space whose walls do not exist yet.
 *
 * SPAWN PLACEMENT NO LONGER DEPENDS ON THIS. Spawn points are authored outside the runtime layer and matched
 * by AFL.Spawn.District.* tag, so they are always loaded and cannot race streaming (see AFLCoreTags.ini).
 * This component covers the remaining, purely physical case: being inside an unfenced district. Today
 * AFL.GamePhase.Warmup already holds players far longer than streaming takes, so this is a guarantee rather
 * than a fix -- it is what makes the behaviour correct on a dedicated server with warmup configured to zero,
 * where nothing else would.
 *
 * WHY THIS AND NOT A TIMEOUT IN THE SPAWN PATH. "The world is not ready" is a LOADING concern, and Lyra
 * already has the contract for it: ULoadingScreenManager polls the game state and every component on it for
 * ILoadingProcessInterface (LoadingScreenManager.cpp:326,334), which is exactly how
 * ULyraExperienceManagerComponent holds the screen while an experience loads. Answering a question the
 * engine asks is not the same as driving a timer of our own -- there is no polling, retry or deadline here.
 *
 * THE LATCH IS LOAD-BEARING. UWorldPartitionSubsystem::IsStreamingCompleted() reports on CURRENT streaming
 * sources, so it goes false again every time a moving player pulls in new cells. Reported raw, that would
 * throw a loading screen over live gameplay. Readiness is therefore a one-way door: once the district has
 * been seen complete, this component is done for the lifetime of the world.
 */
UCLASS(MinimalAPI)
class UAFLDistrictReadinessComponent final : public UGameStateComponent, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	//~ILoadingProcessInterface
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;
	//~End of ILoadingProcessInterface

private:
	/**
	 * One-way: false until the district has been observed fully streamed, true forever after.
	 *
	 * Mutable because ShouldShowLoadingScreen is const by interface contract, and the latch has to be set at
	 * the moment the observation is made -- re-deriving it later is precisely what the latch exists to avoid.
	 */
	mutable bool bReadyLatched = false;
};
