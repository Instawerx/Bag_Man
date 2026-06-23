// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLAG_GrantLoadout.generated.h"

class ULyraInventoryItemDefinition;
class ULyraInventoryItemInstance;

/**
 * UAFLAG_GrantLoadout
 *
 * P-CONTROLS / loadout: grants the hero's starting weapons on spawn, the
 * Lyra-CANONICAL way — a GameplayAbility granted via an AbilitySet on the
 * PawnData, so the loadout is a peer of the hero's other granted behavior
 * (EMP, dash) instead of inlined in a hero Blueprint's event graph (the
 * ShooterCore B_Hero_ShooterMannequin::AddInitialInventory shortcut).
 *
 * Why an ability: Lyra has NO GameFeatureAction for inventory and NO PawnData
 * inventory field (verified against live LyraGame source) — Lyra routes all
 * granted pawn behavior through abilities + AbilitySets. So an OnSpawn ability
 * is the architecturally-correct home, and it scales to per-mode / 12-weapon
 * loadouts by editing the Weapons array on a data-only BP child.
 *
 * MECHANISM (reused verbatim from ShooterCore's proven AddInitialInventory
 * flow — canonical PLACEMENT, proven MECHANISM, not a reinvented inventory
 * path): for each weapon ItemDef -> ULyraInventoryManagerComponent::
 * AddItemDefinition (C++; that method is UE_API-exported) -> the item instance
 * is slotted into the QuickBar via the SlotWeaponInQuickBar / EquipActiveSlot
 * BlueprintImplementableEvents. WHY BP events for the QuickBar: ULyraQuickBar
 * Component's AddItemToSlot/SetActiveSlotIndex are UFUNCTION(BlueprintCallable)
 * but have NO export macro (UE_API), so a cross-module C++ call LNK2019s --
 * the Lyra Cardinal Rule (don't fork engine source to add exports; reach
 * module-private members via their BlueprintCallable surface). The BP child
 * implements the two events with the stock QuickBar nodes. InventoryManager +
 * QuickBar live on the CONTROLLER (reached via GetControllerFromActorInfo).
 *
 * ActivationPolicy = OnSpawn (set on the BP child): TryActivateAbilityOnSpawn
 * fires it once the avatar + controller are set, server-authoritatively, so
 * the grant happens at the right lifecycle moment (post-possession) without a
 * hero-BP Event Possessed hook. Authority-only guard inside ActivateAbility.
 */
UCLASS()
class AFLCOMBAT_API UAFLAG_GrantLoadout : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLAG_GrantLoadout();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/**
	 * The starting weapons, in QuickBar slot order (index 0 = first equipped).
	 * Defaulted empty; the BP child (GA_AFL_GrantLoadout) sets it to
	 * [ID_BagMan_PulseCarbine, ID_BagMan_Beam_v2]. A list so per-mode / future
	 * 12-weapon loadouts are one data edit, no code change.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loadout")
	TArray<TSubclassOf<ULyraInventoryItemDefinition>> Weapons;

	/** Slot the loadout makes active after granting (the weapon the hero holds on spawn). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loadout")
	int32 ActiveSlotIndex = 0;

	/**
	 * Slot a granted item instance into the controller's QuickBar at SlotIndex. The BP child
	 * implements this with the stock ULyraQuickBarComponent::AddItemToSlot node (that method is
	 * BlueprintCallable but not C++-exportable -- see the class comment). Called once per weapon
	 * after AddItemDefinition, on authority.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Loadout")
	void SlotWeaponInQuickBar(int32 SlotIndex, ULyraInventoryItemInstance* Item);

	/**
	 * Make the QuickBar's active slot = SlotIndex so the hero holds that weapon on spawn. The BP
	 * child implements this with the stock ULyraQuickBarComponent::SetActiveSlotIndex node. Called
	 * once after all weapons are slotted (only if at least one was granted).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Loadout")
	void EquipActiveSlot(int32 SlotIndex);
};
