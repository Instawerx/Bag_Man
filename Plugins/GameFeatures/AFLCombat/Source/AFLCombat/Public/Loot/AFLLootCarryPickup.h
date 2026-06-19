// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "AFLBodyZone.h"   // V7-1: the part token's OriginZone (EAFLBodyZone, AFLCore)

#include "AFLLootCarryPickup.generated.h"

class UAFLOverlapCollectComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;   // PRESENTATION: the per-spawn gib material override (the victim's slot-1 MIC)

/**
 * AAFLLootCarryPickup  (Loot-Carry Model, Phase A)
 *
 * A recoverable loot-value pickup. Wears the proven overlap-collect substrate (UAFLOverlapCollectComponent,
 * magnet off = pure walk-over) as its root; on overlap by a viable pawn it adds `LootValue` to that pawn's
 * UAFLLootCarryComponent carried pool, then consumes. Serves two roles in Phase A:
 *   - the TEST COLLECTIBLE (place one, walk over it -> the pool grows);
 *   - the SCATTER pickup the carry component spawns on damage/death (recoverable by ANYONE, incl. the dropper).
 * DETERMINISTIC value (no roll). No shipped code touched -- mirrors AAFLLootCacheInstant's composition, but
 * routes to the carried pool instead of the wallet.
 */
UCLASS()
class AFLCOMBAT_API AAFLLootCarryPickup : public AActor
{
	GENERATED_BODY()

public:
	AAFLLootCarryPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Set the carried value this pickup grants (the scatter spawn calls this with the per-pickup chunk). */
	void SetValue(int32 InValue) { LootValue = InValue; }

	/** AUTHORITY: override the cosmetic body mesh per-spawn (the form-accurate scatter sets a limb-gib mesh;
	 *  null keeps the default cube). Replicated so the scattered FORM reads correctly on every client. */
	void SetVisualMesh(UStaticMesh* InMesh);

	/** AUTHORITY: override the cosmetic body MATERIAL per-spawn (the dismember scatter sets the victim's slot-1
	 *  MIC so the gib is skinned). Replicated so the skinned FORM reads correctly on every client. */
	void SetVisualMaterial(UMaterialInterface* InMaterial);

	/** AUTHORITY: set the overlap's present-before-collectible arm delay per-spawn -- the CACHE scatter sets it (via
	 *  deferred spawn, BEFORE BeginPlay) so a point-blank cube LANDS instead of self-collecting on the dropper. The
	 *  part scatter sets the same beat inline in InitPartToken. */
	void SetArmDelay(float Seconds);

	/** AUTHORITY (V7-1): make this a dismember PART pickup -- set the token's {owner, zone, value} + the gib
	 *  mesh/material. OwnerPlayerId != INDEX_NONE marks it a PART (HandleCollected routes to CollectPart); owner +
	 *  zone replicate for the V7-2 owner-check. The carry component's SpawnPartPickup calls this. */
	void InitPartToken(int32 InOwnerPlayerId, EAFLBodyZone InZone, int32 InValue, UStaticMesh* InMesh, UMaterialInterface* InMaterial);

protected:
	virtual void BeginPlay() override;

	/** Bound to the overlap substrate's OnCollected -- add LootValue to the collector's pool + consume. */
	UFUNCTION()
	void HandleCollected(AActor* Collector);

	/** Apply the replicated per-spawn mesh override to VisualMesh (else keep the ctor cube). */
	void ApplyVisualMesh();

	UFUNCTION()
	void OnRep_VisualMesh();

	UFUNCTION()
	void OnRep_VisualMaterial();

	/** Collect volume + root: the proven overlap+magnet substrate (magnet off -> walk-over). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	TObjectPtr<UAFLOverlapCollectComponent> Overlap;

	/** Cosmetic body (engine cube default so a placed/scattered pickup is watchable). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Per-spawn mesh override (replicated): the scattered FORM (a limb gib in C2/C3; the C1 test sphere;
	 *  null = the ctor cube). Set by SetVisualMesh on the authority; clients apply it via OnRep_VisualMesh. */
	UPROPERTY(ReplicatedUsing = OnRep_VisualMesh)
	TObjectPtr<UStaticMesh> OverrideMesh = nullptr;

	/** Per-spawn material override (replicated): the dismember gib's victim slot-1 MIC, applied to every slot
	 *  (null = the mesh's own material). Set by SetVisualMaterial on authority; clients apply via OnRep. */
	UPROPERTY(ReplicatedUsing = OnRep_VisualMaterial)
	TObjectPtr<UMaterialInterface> OverrideMaterial = nullptr;

	/** V7-1 PART TOKEN -- the scattered part's CATEGORY identity (replicated for the V7-2 owner-check + display).
	 *  OwnerPlayerId == INDEX_NONE -> this is a fungible CUBE (the value rail); else a dismember PART (CollectPart). */
	UPROPERTY(Replicated)
	int32 OwnerPlayerId = INDEX_NONE;

	UPROPERTY(Replicated)
	EAFLBodyZone OriginZone = EAFLBodyZone::None;

	/** Value added to the collector's carried pool on collect (instance-set; the scatter sets per-chunk). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Loot", meta = (ClampMin = "0"))
	int32 LootValue = 50;
};
