// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_GrantLoadout.h"

#include "AFLCombat.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "Inventory/LyraInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_GrantLoadout)

UAFLAG_GrantLoadout::UAFLAG_GrantLoadout()
{
	// Server-authoritative grant. Instanced per actor (default for Lyra abilities);
	// the BP child sets ActivationPolicy = OnSpawn so this fires once on spawn.
	// NetExecutionPolicy left to the BP child (ServerOnly is correct for a grant);
	// we additionally guard on authority inside ActivateAbility.
}

void UAFLAG_GrantLoadout::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Grant is authority-only — items + quickbar slots are server-owned and replicate down.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
		return;
	}

	AController* Controller = GetControllerFromActorInfo();
	if (!Controller)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOADOUT: no controller in ActorInfo; cannot grant loadout."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// The InventoryManager lives on the CONTROLLER (LAS_ShooterGame_StandardComponents adds it there).
	// AddItemDefinition is UE_API-exported -> safe to call from C++.
	ULyraInventoryManagerComponent* Inventory = Controller->FindComponentByClass<ULyraInventoryManagerComponent>();
	if (!Inventory)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_LOADOUT: controller %s missing ULyraInventoryManagerComponent; loadout not granted."),
			*GetNameSafe(Controller));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	int32 GrantedCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < Weapons.Num(); ++SlotIndex)
	{
		const TSubclassOf<ULyraInventoryItemDefinition>& ItemDef = Weapons[SlotIndex];
		if (!ItemDef)
		{
			continue;
		}

		// SAME proven flow as ShooterCore: AddItemDefinition (C++, exported) -> AddItemToSlot.
		// The QuickBar slotting goes through the BP event (AddItemToSlot is BlueprintCallable
		// but not C++-exportable -- the Lyra Cardinal Rule, see the header).
		ULyraInventoryItemInstance* Instance = Inventory->AddItemDefinition(ItemDef, /*StackCount=*/1);
		if (Instance)
		{
			SlotWeaponInQuickBar(SlotIndex, Instance);
			++GrantedCount;
		}
		else
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOADOUT: AddItemDefinition returned null for %s (slot %d)."),
				*GetNameSafe(ItemDef.Get()), SlotIndex);
		}
	}

	// Equip the active slot so the hero holds a weapon on spawn (BP event -> SetActiveSlotIndex).
	if (GrantedCount > 0)
	{
		EquipActiveSlot(ActiveSlotIndex);
	}

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_LOADOUT: granted %d/%d weapons on %s, active slot %d."),
		GrantedCount, Weapons.Num(), *GetNameSafe(Controller), ActiveSlotIndex);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
