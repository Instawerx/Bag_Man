// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
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
 * The server AddGameplayCue's the cue (server-auth); GAS replicates the cue container to all clients and
 * re-applies on late-join. OnActive (every client) hides the zone bone; OnRemove un-hides. It reaches the
 * gameplay-side dismember component THROUGH IAFLDismemberCosmeticTarget (FName-based, no concrete cross-
 * module dep) -- the exact pattern AAFLCueNotify_LaserBeam uses with IAFLBeamEndpointProvider.
 *
 * STATIC notify (NOT an Actor like the beam cues): this is a STATELESS ROUTER -- no Niagara, no Tick, no
 * spawned instance. GAS calls OnActive/OnRemove on the CDO directly, so OnRemove ALWAYS fires on
 * RemoveGameplayCue. (It was briefly an AGameplayCueNotify_Actor during the relocation; a referenceless
 * router actor is GC'd between sever and reattach, so GAS's weak-ptr instance tracking lost it ->
 * RemoveGameplayCue found no instance -> OnRemove silently skipped -> the bone never un-hid (measured: 0
 * OnRemove / 40 OnActive). Static has no instance to lose, so the un-hide is reliable. The beam cues stay
 * Actor -- they own Niagara that keeps them referenced and are removed the same ability tick.)
 */
UCLASS()
class AFLVFX_API UAFLCueNotify_ZoneSever : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	//~ Static notify: stateless, GAS calls these on the CDO directly (no instance, no pool, no GC). const per
	//  the UGameplayCueNotify_Static contract. OnActive hides the zone bone all-client; OnRemove un-hides.
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	//~ Relevance/late-join (CORRECTNESS at Battle-Royale scale): GAS calls WhileActive -- NOT OnActive -- when a
	//  client becomes relevant to an ALREADY-active cue (a player entering relevance of a robot dismembered
	//  seconds/minutes ago elsewhere). OnActive fired only for clients present at sever time, so WhileActive
	//  re-applies the SAME hide for the relevance-join (else that client sees a head that should be gone).
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
