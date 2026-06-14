// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AFLDismemberCosmeticTarget.generated.h"

/**
 * The cross-module seam a PERSISTENT severance cue reads to drive the all-client bone-hide.
 *
 * S4-INC4 / AFL-0408 RESIDENCY FIX: the dismember bone-hide is replicated via a persistent
 * GameplayCue (AddGameplayCue server-side -> the cue's OnActive/OnRemove run the hide/un-hide
 * on every client). That cue notify MUST live in an EARLY-LOADING module so it is present at
 * the frame-0 GameplayCueManager scan -- a notify in the late-mounting AFLDismember GameFeature
 * is invisible to the cold scan and never fires OnActive. So the notify lives in AFLVFX (this
 * always-on cosmetic plugin, beside AAFLCueNotify_LaserBeam) and reaches the gameplay-side
 * dismember component through THIS interface -- never a concrete type. The gameplay side
 * (UAFLDismemberComponent, an AFLDismember GameFeature class) IMPLEMENTS it; a GameFeature
 * depending on the always-on plugin is the correct load-order direction (mirrors
 * IAFLBeamEndpointProvider / IAFLLaserVisualProvider exactly).
 *
 * FName-based so AFLVFX needs NO AFLCore dep (no EAFLBodyZone here): the cue passes the cue-tag
 * LEAF ("Head"/"LeftArm"/...) and the component maps leaf -> zone on its own side. The cue calls
 * via the generated thunk: IAFLDismemberCosmeticTarget::Execute_ApplyZoneHideByLeaf(Comp, Leaf).
 *
 * Divergence from IAFLBeamEndpointProvider: these methods are NON-const (the hide MUTATES the
 * component's meshes), where the beam's are const getters. Justified -- this is a do-it call.
 */
UINTERFACE(BlueprintType, MinimalAPI)
class UAFLDismemberCosmeticTarget : public UInterface
{
	GENERATED_BODY()
};

class AFLVFX_API IAFLDismemberCosmeticTarget
{
	GENERATED_BODY()

public:
	/** Hide the zone's bone on the visible mesh, on THIS client. ZoneLeaf = the cue-tag leaf
	 *  (Head/LeftArm/RightArm/LeftLeg/RightLeg). Called by the persistent cue's OnActive on every client. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Dismember")
	void ApplyZoneHideByLeaf(FName ZoneLeaf);

	/** Un-hide the zone's bone, on THIS client. Called by the cue's OnRemove on every client. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Dismember")
	void ApplyZoneRestoreByLeaf(FName ZoneLeaf);
};
