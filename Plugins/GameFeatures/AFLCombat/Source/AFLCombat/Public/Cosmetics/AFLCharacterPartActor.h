// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameFramework/CheatManagerDefines.h"   // UE_WITH_CHEAT_MANAGER guard for the panel-watch DebugSetMID* decls (must match the .cpp + cheats gate)
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"

#include "AFLCharacterPartActor.generated.h"

class UAFLSkinColorAsset;
class UMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;          // AuthoredSlot1Material member (the captured pre-swap slot-1 base)
class UMaterialInstanceConstant;   // ApplyFacemask param (the facemask MIC swapped onto slot 1)

/** Per-mesh owned-MID slot map (USTRUCT so it can live in a UPROPERTY TMap value). */
USTRUCT()
struct FAFLSkinMIDSlots
{
	GENERATED_BODY()

	// Slot index -> the MID WE created for that slot. We only ever write to MIDs in here.
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> SlotMIDs;
};

/**
 * BagMan-owned base for the robot body CharacterPart actors (B_AFL_Robot_* reparent to this).
 *
 * Two roles in the L5 race-safe color system:
 *  - PATH 1 (part-arrives-second): on BeginPlay, resolve the owning pawn's UAFLSkinColorComponent and
 *    self-apply the current color. The part attaches TO the pawn (spawned via the pawn's CharacterParts
 *    component), so the pawn + that component are guaranteed present at this BeginPlay. If the color VALUE
 *    hasn't replicated yet, GetSkinColor() is null -> ApplySkinColor no-ops -> the component's OnRep-push
 *    (PATH 2) applies when the color arrives. Both paths are required.
 *  - FIX 1 (a) OWN-YOUR-MID: ApplySkinColor writes ONLY to MIDs WE created (cached in OwnedMIDs), never a
 *    foreign MID another system put in the slot (the body mesh has a hit-flash / HitPosition0 MID path).
 *    Per-part-instance + Transient: a mark-change destroy+respawn makes a fresh part -> fresh empty cache.
 *
 * TAG CONTRACT (load-bearing): the B_AFL_Robot_* BPs descend from ALyraTaggedActor and carry
 * Cosmetic.AnimationStyle.* + Cosmetic.BodyStyle.* tags that Lyra's AnimBP reads via
 * IGameplayTagAssetInterface to pick animation style + body proportions. ALyraTaggedActor is
 * module-private (no LYRAGAME_API) so we cannot subclass it (same link wall as the CharacterParts
 * classes). We therefore implement IGameplayTagAssetInterface DIRECTLY (it is GAMEPLAYTAGS_API ->
 * exported -> links) with the SAME property name (StaticGameplayTags) so each robot's per-CDO tag
 * values carry across the reparent by name-match. Do NOT rename StaticGameplayTags.
 */
UCLASS()
class AFLCOMBAT_API AAFLCharacterPartActor : public AActor, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	//~IGameplayTagAssetInterface (mirrors ALyraTaggedActor so the reparented robots keep their tag contract)
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	//~End of IGameplayTagAssetInterface

	/**
	 * Apply a typed color preset to THIS part's mesh components. Engine-only, OWN-MID (create-once,
	 * write-only-ours), null-safe. Public so the pawn component's OnRep-push (PATH 2) can also call it
	 * on already-spawned parts. Idempotent: re-fire (PATH 1 + PATH 2) reuses our cached MIDs.
	 */
	void ApplySkinColor(const UAFLSkinColorAsset* ColorAsset);

	/**
	 * Equip a FACEMASK on THIS part: swap the SLOT-1 base material (M_HeadLegs, the head/visor region) to the
	 * given mask MIC -- the proven slot-1 base-MI facemask path (MI_AFL_FaceMask_Pink). FacemaskMIC==nullptr
	 * RESTORES slot 1 to the part's authored base material (cached on first equip). Public so the pawn
	 * component's OnRep-push (PATH 2) calls it on already-spawned parts.
	 *
	 * COMPOSITION (the load-bearing bit): swapping slot 1's base material DROPS our owned MID on that slot
	 * (the finish's color params lived in that MID). So after the swap we FORGET our slot-1 MID and re-call
	 * ApplySkinColor(ColorToReapply) -- which re-MIDs the SWAPPED material and re-pushes the finish params on
	 * top. Order: material swap, then param re-push -> the facemask and the finish coexist (the exact failure
	 * mode if you swapped without re-applying: a facemask would strand the finish). Idempotent.
	 */
	void ApplyFacemask(class UMaterialInstanceConstant* FacemaskMIC, const UAFLSkinColorAsset* ColorToReapply);

	/**
	 * DEFECT-2: the exact UAFLSkinColorAsset this part last painted its MIDs with in ApplySkinColor -- the finish
	 * ACTUALLY on the live runtime MID. The dismember gib color source reads THIS (server-side) instead of the
	 * pawn component's GetSkinColor() (which can drift to the ARIA-pink default), so a severed head/limb gib
	 * reproduces the live part's finish. A content asset -> replication-safe; null until the first ApplySkinColor.
	 */
	UAFLSkinColorAsset* GetLastAppliedColor() const { return LastAppliedColor; }

#if UE_WITH_CHEAT_MANAGER
	/**
	 * PANEL-WATCH INSTRUMENT (afl.Cosmetic.SetParam): poke a single named material param on THIS part's
	 * live MIDs so the operator can watch which physical region the param paints. Ensures a MID exists on
	 * every slot (creates+caches via the same OWN-MID path as ApplySkinColor) so it works even before any
	 * skin apply, then SetScalar/SetVector on all of them. Returns the number of MID slots written.
	 * Cheat-only (compiled out of shipping). Returns 0 if the part has no mesh/slots.
	 */
	int32 DebugSetMIDVectorParam(FName ParamName, const FLinearColor& Value);
	int32 DebugSetMIDScalarParam(FName ParamName, float Value);
#endif

protected:
	virtual void BeginPlay() override;

	// Gameplay-related tags associated with this actor. IDENTICAL name+type to ALyraTaggedActor so the
	// reparented robots' per-CDO Cosmetic.* tag values carry across the reparent by name-match. Read by
	// Lyra's AnimBP via GetOwnedGameplayTags for animation-style + body-proportion selection.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Actor)
	FGameplayTagContainer StaticGameplayTags;

	/**
	 * UV1 VISOR CONTRACT (unique-body fleet, 2026-07-23): TRUE on parts whose mesh is a UNIQUE body
	 * (own Tripo UV atlas + the standardized UV1 visor projection from the conform recipe) rather than
	 * the shared SKM_Manny. When set, ApplyFacemask swaps in the AUTO-DERIVED visor variant
	 * (MI_AFL_FaceMask_<X> -> MI_AFL_FaceMaskV_<X>, generated from the same LogoTexture, sampling UV1
	 * via M_AFL_FaceMask_Visor) so every stock mask DESIGN renders correctly on the unique visor.
	 * FALSE (default) = the stock Manny path, byte-identical behavior for the 34 stock identities.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Cosmetics")
	bool bUniqueBodyUVs = false;

	// FIX 1 (a): the MIDs WE created, keyed by mesh then slot. ApplySkinColor writes only to these.
	UPROPERTY(Transient)
	TMap<TObjectPtr<UMeshComponent>, FAFLSkinMIDSlots> OwnedMIDs;

	// FACEMASK: the part's AUTHORED slot-1 base material per mesh, captured the FIRST time a facemask is
	// equipped on that mesh -- so equipping nullptr RESTORES the original (the robot's BP-default slot-1, e.g.
	// MI_<id>_Limbs) instead of leaving the mask stuck. Keyed by mesh; the value is the pre-swap base material.
	UPROPERTY(Transient)
	TMap<TObjectPtr<UMeshComponent>, TObjectPtr<UMaterialInterface>> AuthoredSlot1Material;

	// DEFECT-2: the asset ApplySkinColor last painted THIS part's MIDs with -- the finish ACTUALLY on the live
	// runtime MID. Recorded each apply. The dismember gib color source reads it (server-side) so a severed gib
	// reproduces the live finish, not a default. Transient + per-machine (NOT Replicated): the part is a
	// locally-spawned cosmetic child actor (color derives from the pawn's REPLICATED SkinColor), so ApplySkinColor
	// runs + records on EVERY machine (server on the authority paint, clients on OnRep). The gib's OWN replicated
	// color (HeadSkinColor/PartSkinColor) carries it to clients -- the existing server-read + gib-replicate path --
	// so this property need not (and the local child actor cannot, without an architecture change) network-replicate.
	UPROPERTY(Transient)
	TObjectPtr<UAFLSkinColorAsset> LastAppliedColor = nullptr;
};
