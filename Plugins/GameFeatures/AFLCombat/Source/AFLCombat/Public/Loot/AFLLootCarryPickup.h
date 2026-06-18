// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "AFLLootCarryPickup.generated.h"

class UAFLOverlapCollectComponent;
class UStaticMesh;
class UStaticMeshComponent;

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

protected:
	virtual void BeginPlay() override;

	/** Bound to the overlap substrate's OnCollected -- add LootValue to the collector's pool + consume. */
	UFUNCTION()
	void HandleCollected(AActor* Collector);

	/** Apply the replicated per-spawn mesh override to VisualMesh (else keep the ctor cube). */
	void ApplyVisualMesh();

	UFUNCTION()
	void OnRep_VisualMesh();

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

	/** Value added to the collector's carried pool on collect (instance-set; the scatter sets per-chunk). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Loot", meta = (ClampMin = "0"))
	int32 LootValue = 50;
};
