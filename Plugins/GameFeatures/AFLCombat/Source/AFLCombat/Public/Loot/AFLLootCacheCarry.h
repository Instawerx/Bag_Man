// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "Loot/AFLLootTypes.h"
#include "Loot/AFLLootable.h"

#include "AFLLootCacheCarry.generated.h"

class UAFLGrabbableComponent;
class UAFLLootGrantComponent;
class UStaticMeshComponent;

/**
 * AAFLLootCacheCarry -- a CARRY-mode arena loot cache (Loot Phase 3). The grab half of the per-cache
 * retrieval-mode flag (#2): a physics-prop mesh + a UAFLGrabbableComponent (the proven grab substrate --
 * the hero's interaction discovery finds it + offers GA_AFL_Grab, attaches it to hand_r) + a
 * UAFLLootGrantComponent. On grab the grabbable fires OnGrabbedBy -> the grant component grants the
 * configured (deterministic) Watts (grant-once guard handles re-grab). The CARRY counterpart to
 * AAFLLootCacheInstant -- a higher-value cache you must reach + pick up (vs the safe walk-over INSTANT).
 *
 * Authored as a BP child (BP_AFL_LootCacheCarry) that sets the inherited Grabbable.GrabAbility =
 * GA_AFL_Grab_C as a default -- MIRRORING the proven AAFLHeadLootBox / BP_AFL_HeadLootBox. The C++ ctor leaves
 * GrabAbility unset: GA_AFL_Grab lives in the AFLMovement GameFeature (not mounted at this CDO-ctor), so a BP
 * default resolves it post-mount + redirector-safe. Map-PLACEABLE: LootWatts/Eligibility are instance
 * UPROPERTYs. Physics-prop setup MIRRORS the proven head loot-box (PhysicsActor + simulate + snapshot
 * replication). DETERMINISTIC value (no randomized acquisition).
 */
UCLASS()
class AFLCOMBAT_API AAFLLootCacheCarry : public AActor, public IAFLLootable
{
	GENERATED_BODY()

public:
	AAFLLootCacheCarry();

	//~ IAFLLootable
	virtual UAFLLootGrantComponent* GetLootGrantComponent() const override { return LootGrant; }

protected:
	virtual void BeginPlay() override;

	/** Bound to the grabbable's OnGrabbedBy -- grant on grab (grant-once guards re-grab). */
	UFUNCTION()
	void HandleGrabbed(AActor* Grabber);

	/** Physics-prop body + root (collision so the grab line-trace/proximity finds it). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The grab substrate (discovery + carry). The BP child sets GrabAbility = GA_AFL_Grab_C (inherited-component
	 *  default; mirrors BP_AFL_HeadLootBox -- not an FClassFinder in the C++ ctor). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	TObjectPtr<UAFLGrabbableComponent> Grabbable;

	/** The shared grant (eligibility + grant-once + Watts), configured at BeginPlay. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	TObjectPtr<UAFLLootGrantComponent> LootGrant;

	/** Known Watts granted on grab (instance-set; higher than INSTANT -- a carry cache is the riskier prize). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Loot", meta = (ClampMin = "0"))
	int32 LootWatts = 200;

	/** Who may loot it (Anyone default; TeamOnly for a team cache -- Q1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	EAFLLootEligibility Eligibility = EAFLLootEligibility::Anyone;
};
