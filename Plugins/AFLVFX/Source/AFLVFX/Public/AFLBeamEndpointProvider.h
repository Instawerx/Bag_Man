// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AFLBeamEndpointProvider.generated.h"

/**
 * The published-value bridge a sustained-beam cue reads to learn WHERE the beam ends.
 *
 * The authoritative GameplayAbility traces and publishes its confirmed impact point into
 * a component on the firing pawn; that component implements this interface. The looping
 * cue (AAFLCueNotify_LaserBeam) finds the component on its target actor and reads
 * GetBeamImpactPoint() each frame to drive the Niagara User."Beam End". Nothing else
 * crosses the gameplay/cosmetic boundary -- exactly one world-space point.
 *
 * The interface lives in AFLVFX (the always-on cosmetic plugin that owns the cue) so the
 * cue depends only on this contract, never on the concrete gameplay component. The
 * gameplay side (AFLCombat, a GameFeature) implements it -- a GameFeature depending on the
 * always-on plugin is the correct load-order direction (mirrors IAFLLaserVisualProvider).
 *
 * BlueprintNativeEvent so a future Blueprint component could also publish. The cue calls
 * via the generated thunk: IAFLBeamEndpointProvider::Execute_GetBeamImpactPoint(Comp).
 */
UINTERFACE(BlueprintType, MinimalAPI)
class UAFLBeamEndpointProvider : public UInterface
{
	GENERATED_BODY()
};

class AFLVFX_API IAFLBeamEndpointProvider
{
	GENERATED_BODY()

public:
	/** Current authoritative beam endpoint, world-space. Drives User."Beam End". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Laser")
	FVector GetBeamImpactPoint() const;

	/** True while a beam is actively channeling on the owning pawn. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Laser")
	bool IsBeamActive() const;
};
