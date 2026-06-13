// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayCueNotify_Static.h"

#include "AFLCueNotify_ZoneSever.generated.h"

/**
 * S4-INC4 (AFL-0408): the ALL-CLIENT replicated bone-hide for dismemberment.
 *
 * Registered on the PARENT tag GameplayCue.Combat.Dismember.State -- GAS routes every
 * .State.<Zone> child cue to this notify (nearest-parent match). The server ADDS the cue
 * (UAFLDismemberComponent::SeverZone -> VictimASC->AddGameplayCue) so it lands in the
 * replicated minimal-cue container; GAS replicates that container to all clients (Mixed mode)
 * AND re-applies it on late-join / relevancy-rejoin -- which is why the persistent ADD cue
 * (not a one-shot Execute) is correct for the bone-hide, which is persistent STATE.
 *
 *   OnActive  (every client, incl. late-join): resolve the zone from the cue tag -> find the
 *             target's UAFLDismemberComponent -> ApplyZoneHideCosmetic(zone) (GatherZoneMeshes +
 *             HideBoneByName on the visible CharacterPart mesh -- the B-2 logic, now all-client).
 *   OnRemove  (every client): ApplyZoneRestoreCosmetic(zone) -> un-hide. The server REMOVES the
 *             cue at reattach (RestoreZone -> RemoveGameplayCue); restore is symmetric, free.
 *
 * Pure-logic cue (no spawned actor) -> UGameplayCueNotify_Static is the right base (vs the BP
 * GCN_* one-shots / GCNL_* loopers). The transient detach pop (GameplayCue.Combat.Dismember.HeadPop,
 * sparks/sound) stays its own one-shot Execute cue, UNCHANGED.
 *
 * The SEVER is server-authoritative (the AddGameplayCue); the RENDER is all-client (this notify).
 * Clean split: server decides, every client draws.
 *
 * REGISTRATION (post-build content): UGameplayCueManager discovers cue notifies by scanning BP ASSETS
 * (a UObjectLibrary of UGameplayCueNotify_Static), NOT bare native classes. So a thin BP child of this
 * class -- GC_AFL_Dismember_State, GameplayCueTag = GameplayCue.Combat.Dismember.State -- must exist in
 * a scanned cue directory for GAS to route to it. ALL logic lives here in C++; the BP is an empty
 * registration shell (the same way GCN_AFL_Dismember_HeadPop is a BP child of a Lyra base).
 */
UCLASS()
class AFLDISMEMBER_API UAFLCueNotify_ZoneSever : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UAFLCueNotify_ZoneSever();

	//~UGameplayCueNotify_Static -- OnActive/WhileActive/OnRemove are BlueprintNativeEvents; the C++
	// body goes in the _Implementation override (NOT the bare name, which is the generated dispatcher).
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	//~End of UGameplayCueNotify_Static
};
