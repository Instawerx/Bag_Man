// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Character/LyraHeroComponent.h"

#include "AFLHeroComponent.generated.h"

class ULyraEquipmentDefinition;
class ULyraEquipmentInstance;
class ULyraEquipmentManagerComponent;


/**
 * UAFLHeroComponent
 *
 * AFL-0214 hero component used by AFLCombat-controlled pawns. Extends
 * ULyraHeroComponent so all of Lyra's input/camera/init-state plumbing is
 * inherited unchanged; AFL-specific behavior is layered as the equipment
 * lifecycle described in master doc Sec. 9.
 *
 * The AFLCombat Game Feature places this component on its designated pawns
 * via a UGameFeatureAction_AddComponents entry. The component's BeginPlay /
 * EndPlay run exactly when the feature is active for the owning pawn, so we
 * treat them as the per-pawn activate/deactivate seam:
 *
 *   - On BeginPlay (authority only), ApplyDefaultEquipment iterates
 *     DefaultEquipmentDefinitions and pushes each entry through the pawn's
 *     ULyraEquipmentManagerComponent::EquipItem. The granted ability sets
 *     (FLyraAppliedEquipmentEntry::GrantedHandles) are what hand the Pulse
 *     ability spec to the ASC — AFL code never grants abilities directly to
 *     the ASC (AFL-0215 hard rail; all grants come from ULyraPawnData ability
 *     sets via the equipment manager).
 *
 *   - On EndPlay (authority only), UnequipAll removes every instance that
 *     ApplyDefaultEquipment installed, restoring the ASC to its un-equipped
 *     baseline. This matches the Game Feature deactivation contract: tearing
 *     down a feature must not leave dangling specs on the ability system.
 *
 * DefaultEquipmentDefinitions is editable on the BP child placed by the Game
 * Feature Action so designers can swap in alternate loadouts without touching
 * C++.
 *
 * NOTE on cross-plugin contracts:
 *   - The actual equipment definition class (a ULyraEquipmentDefinition that
 *     references DA_AFL_AbilitySet_Combat_Pulse) is editor-authored content
 *     and lives in /AFLCombat/Content/Equipment/. The lifecycle hook here
 *     intentionally does NOT hard-reference any specific equipment class —
 *     DefaultEquipmentDefinitions is just a TArray the BP child fills in.
 *   - Future tasks (e.g., the inventory/loadout system in S6) may switch this
 *     to read from a designer-curated inventory data asset instead of an
 *     inline list; the public surface (ApplyDefaultEquipment / UnequipAll)
 *     stays stable so call sites do not churn.
 */
UCLASS(MinimalAPI, Blueprintable, Meta=(BlueprintSpawnableComponent))
class UAFLHeroComponent : public ULyraHeroComponent
{
	GENERATED_BODY()

public:

	UAFLHeroComponent(const FObjectInitializer& ObjectInitializer);

	// NOTE: Intentionally does NOT override GetFeatureName(). We inherit
	// ULyraHeroComponent's NAME_ActorFeatureName = "Hero" so this component
	// registers with the GameFrameworkComponentManager under the same feature
	// name Lyra's stock hero plumbing expects. Earlier versions of this class
	// declared a distinct "AFLHero" feature name based on the assumption that
	// AFL needed its own state graph; investigation 2026-05-25 confirmed the
	// rename was fully orphaned (zero call sites in the entire project
	// queried "AFLHero"), and Lyra's init-state gates are feature-name-agnostic
	// except for the PawnExtension self-check. Reverted to inherited default
	// as part of AFL-0304-Bi diagnosis.

	/**
	 * Server-authoritative: equip every entry in DefaultEquipmentDefinitions via
	 * the pawn's ULyraEquipmentManagerComponent. Invoked from BeginPlay; safe to
	 * call directly from an external system if a future loadout-swap flow needs it.
	 * No-op on clients (Lyra's equipment manager replicates the entries).
	 */
	UFUNCTION(BlueprintCallable, Category="AFL|Hero", BlueprintAuthorityOnly)
	AFLCOMBAT_API void ApplyDefaultEquipment();

	/**
	 * Server-authoritative: unequip every instance previously installed by
	 * ApplyDefaultEquipment. Invoked from EndPlay so the AFLCombat Game Feature
	 * tears down cleanly on deactivation; idempotent.
	 */
	UFUNCTION(BlueprintCallable, Category="AFL|Hero", BlueprintAuthorityOnly)
	AFLCOMBAT_API void UnequipAll();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Equipment definitions granted to the owning pawn while this component is
	 * active. Each entry is forwarded to ULyraEquipmentManagerComponent::EquipItem;
	 * the equipment instance's underlying ability set is what grants the Pulse
	 * (and future weapon) abilities to the ASC, so this is the canonical path
	 * for "the AFLCombat game feature applies its equipment when active."
	 *
	 * Empty by default — populated by the BP child the Game Feature Action
	 * spawns onto pawns. Empty list means "no AFL equipment applied," which is
	 * a valid configuration for non-combat experiences that still load this
	 * component.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Hero|Equipment")
	TArray<TSubclassOf<ULyraEquipmentDefinition>> DefaultEquipmentDefinitions;

private:

	/** Finds the pawn's equipment manager, or nullptr if the pawn has not yet attached one. */
	ULyraEquipmentManagerComponent* FindEquipmentManager() const;

	/**
	 * Live instances we created in ApplyDefaultEquipment. Held as weak refs so a
	 * destroyed equipment instance (e.g., the manager is unregistered before
	 * our EndPlay) does not crash UnequipAll. Cleared by UnequipAll.
	 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ULyraEquipmentInstance>> AppliedEquipmentInstances;
};
