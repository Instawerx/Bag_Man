// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "Loot/AFLLootTypes.h"
#include "Loot/AFLLootable.h"

#include "AFLLootCacheInstant.generated.h"

class UAFLOverlapCollectComponent;
class UAFLLootGrantComponent;
class UStaticMeshComponent;

/**
 * AAFLLootCacheInstant -- an INSTANT-mode arena loot cache (Loot Phase 3). The walk-over half of the
 * per-cache retrieval-mode flag (#2): wears a UAFLOverlapCollectComponent (the substrate extracted from
 * the energy pickup) as its collect volume + a UAFLLootGrantComponent. On overlap by a viable pawn the
 * substrate fires OnCollected -> the grant component grants the configured (deterministic) Watts, then the
 * cache is consumed. Map-PLACEABLE: LootWatts/Eligibility are instance UPROPERTYs set per placement (the
 * director's spawn-from-config loop is the Phase-3 follow-up). Magnet off by default (a cache should not
 * fly at you -- pure walk-over). DETERMINISTIC value (IRONICS_ECONOMY_SPEC.md, no randomized acquisition).
 */
UCLASS()
class AFLCOMBAT_API AAFLLootCacheInstant : public AActor, public IAFLLootable
{
	GENERATED_BODY()

public:
	AAFLLootCacheInstant();

	//~ IAFLLootable
	virtual UAFLLootGrantComponent* GetLootGrantComponent() const override { return LootGrant; }

protected:
	virtual void BeginPlay() override;

	/** Bound to the overlap substrate's OnCollected -- grant + consume. */
	UFUNCTION()
	void HandleCollected(AActor* Collector);

	/** The INSTANT retrieval substrate (overlap + optional magnet), root + collect volume. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	TObjectPtr<UAFLOverlapCollectComponent> Overlap;

	/** The shared grant (eligibility + grant-once + Watts), configured at BeginPlay. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	TObjectPtr<UAFLLootGrantComponent> LootGrant;

	/** Cosmetic body (engine cube default so a placed cache is watchable; restyle per placement). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	/** Known Watts granted on collect (instance-set per placement; deterministic -- never rolled). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Loot", meta = (ClampMin = "0"))
	int32 LootWatts = 50;

	/** Who may collect (Anyone for a free supply cache; TeamOnly for a team cache -- Q1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	EAFLLootEligibility Eligibility = EAFLLootEligibility::Anyone;
};
