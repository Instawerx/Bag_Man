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

	// FIX 1 (a): the MIDs WE created, keyed by mesh then slot. ApplySkinColor writes only to these.
	UPROPERTY(Transient)
	TMap<TObjectPtr<UMeshComponent>, FAFLSkinMIDSlots> OwnedMIDs;
};
