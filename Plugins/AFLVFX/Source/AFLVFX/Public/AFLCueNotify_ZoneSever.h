// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "AFLCueNotify_ZoneSever.generated.h"

/**
 * S4-INC4 / AFL-0408: the PERSISTENT severance cue -- replicates the dismember bone-hide to all clients.
 *
 * RELOCATED here from the AFLDismember GameFeature (the residency fix): a GameplayCueNotify in a late-
 * mounting GameFeature is invisible to the frame-0 GameplayCueManager scan, so its OnActive never fires
 * on a cold start. Living in AFLVFX (always-on, loaded at startup, beside AAFLCueNotify_LaserBeam) makes
 * it present at the cold scan -> discovered -> OnActive fires. Registered on the PARENT tag
 * GameplayCue.Combat.Dismember.State; GAS routes every .State.<Zone> child cue here (nearest-parent).
 *
 * The server AddGameplayCue's the cue (server-auth); GAS replicates the minimal-cue container (Mixed) to
 * all clients + re-applies on late-join. OnActive (every client) hides the zone bone; OnRemove un-hides.
 * It reaches the gameplay-side dismember component THROUGH IAFLDismemberCosmeticTarget (FName-based, no
 * concrete cross-module dep) -- the exact pattern AAFLCueNotify_LaserBeam uses with IAFLBeamEndpointProvider.
 *
 * Divergence from the beam cue: no Niagara, no Tick -- this cue ONLY routes to the interface (the hide
 * logic lives in the component's GatherZoneMeshes/HideBoneByName). bAutoDestroyOnRemove like the beam.
 */
UCLASS()
class AFLVFX_API AAFLCueNotify_ZoneSever : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AAFLCueNotify_ZoneSever();

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
};
