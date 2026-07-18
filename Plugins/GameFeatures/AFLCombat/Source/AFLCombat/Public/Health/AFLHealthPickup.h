// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "AFLHealthPickup.generated.h"

class UAFLOverlapCollectComponent;
class UStaticMeshComponent;
class ULyraInventoryItemDefinition;
class ULyraInventoryItemInstance;

/**
 * FAFLHealthCollectedMessage -- broadcast on Event.Health.Collected when a health pickup applies.
 * Mirror of FAFLEnergyCollectedMessage (HUD/score/FX listeners via UGameplayMessageSubsystem; NOT
 * net-serialized, so it lives here in the GameFeature per the residency rule). bInstantHeal distinguishes
 * the med-station heal (HealAmount > 0) from the carriable-item grant (HealAmount = 0, an item was added).
 */
USTRUCT(BlueprintType)
struct AFLCOMBAT_API FAFLHealthCollectedMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Health")
	TObjectPtr<AActor> Collector = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Health")
	float HealAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Health")
	bool bInstantHeal = false;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Health")
	FVector Location = FVector::ZeroVector;
};

/**
 * AAFLHealthPickup  (Phase 1 of the health consumable -- SSOT Docs/IRONICS_HEALTH_CONSUMABLE_SSOT.md)
 *
 * CONFORM-TO-PROVEN clone of AAFLEnergyPickup: same runtime-spawned/placed replicated posture, the same
 * PROVEN UAFLOverlapCollectComponent root (overlap + one-shot guard + viable-collector check). It diverges
 * from the energy pickup in exactly three justified ways:
 *   (a) NO magnet -- a health pack is a walk-over / walk-into pickup, not a vacuum-up currency drop, so
 *       MagnetRadius stays 0 (the substrate's supported pure-overlap mode; see its header).
 *   (b) the collect GRANT differs (heal vs item, below) instead of the energy gain GE.
 *   (c) it heals UAFLAttributeSet_Combat.Health (AFL's own combat-health economy), via the reused
 *       UAFLGE_OverloadRestore SetByCaller shell.
 * It also omits the energy pickup's InitEnergyValue deferred-spawn hook (no health-spawn cheat in scope; the
 * pickup is placed in-editor + granted via System-A loadout). Add one later if a cheat-spawn is wanted.
 *
 * Two collect modes (ruling (1) -- BOTH shapes in one class):
 *   - bInstantHeal = false (default, CARRIABLE): grants HealItemDef (ID_AFL_Heal) to the collector's
 *     inventory (the ULyraInventoryManagerComponent on the Controller -- the same AddItemDefinition path as
 *     UAFLAG_GrantLoadout), so the player carries + uses it later. No heal happens here.
 *   - bInstantHeal = true (MED-STATION): applies UAFLGE_OverloadRestore directly with
 *     SetByCaller(Data.Health.Restore, HealAmount) -- an immediate FIXED heal (ruling (4)); grants no item.
 * Overheal is auto-capped by UAFLAttributeSet_Combat::PreAttributeChange (the rail: the attribute moves ONLY
 * through the GE, never a direct write).
 */
UCLASS(Blueprintable)
class AFLCOMBAT_API AAFLHealthPickup : public AActor
{
	GENERATED_BODY()

public:
	AAFLHealthPickup(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	float GetHealAmount() const { return HealAmount; }

protected:
	/** The health-SPECIFIC grant, bound to CollectSphere->OnCollected. The substrate fires this once,
	 *  server-auth, for a viable collector; this branches on bInstantHeal (heal vs item) then Destroy. */
	UFUNCTION()
	void HandleCollected(AActor* Collector);

	/** Optional QuickBar auto-slot hook for the CARRIABLE grant. ULyraQuickBarComponent::AddItemToSlot is
	 *  BlueprintCallable-only (the Lyra Cardinal Rule -- not C++-exportable), so a BP child implements this if
	 *  the granted consumable should auto-slot; the C++ default (inventory-add only) leaves it in inventory.
	 *  Mirrors how UAFLAG_GrantLoadout::SlotWeaponInQuickBar routes the slotting through a BP event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Health")
	void OnConsumableGranted(AActor* Collector, ULyraInventoryItemInstance* Instance);

	/** Collect mode (ruling (1)). false = grant the carriable item; true = instant heal (med-station). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Health")
	bool bInstantHeal = false;

	/** Fixed heal magnitude for the INSTANT/med-station mode (ruling (4); default ~50% of a 100 max, tune
	 *  per-instance). The CARRIABLE mode does NOT heal here -- the item's use-ability computes its own magnitude. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Health", meta = (ClampMin = "0.0"))
	float HealAmount = 50.0f;

	/** The consumable item granted in CARRIABLE mode (ID_AFL_Heal). Null -> nothing to grant (logged). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Health")
	TSubclassOf<ULyraInventoryItemDefinition> HealItemDef;

	/** Collect volume + root: the proven overlap substrate (UAFLOverlapCollectComponent). DIVERGENCE from
	 *  AAFLEnergyPickup: MagnetRadius left 0 (pure walk-over) -- a health pack should not fly at the player. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Health")
	TObjectPtr<UAFLOverlapCollectComponent> CollectSphere;

	/** Default visible body (engine sphere) so C++-spawned/placed pickups are watchable; the reskin BP
	 *  restyles to the IRONICS health-pack mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Health")
	TObjectPtr<UStaticMeshComponent> VisualMesh;
};
