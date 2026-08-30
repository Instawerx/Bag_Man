// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_GrantLoadout.h"

#include "AFLCombat.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"   // Block 28: the durable WeaponId selection
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "Inventory/LyraInventoryManagerComponent.h"
#include "Character/LyraPawnExtensionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_GrantLoadout)

UAFLAG_GrantLoadout::UAFLAG_GrantLoadout()
{
	// Server-authoritative grant. Instanced per actor (default for Lyra abilities);
	// the BP child sets ActivationPolicy = OnSpawn so this fires once on spawn.
	// NetExecutionPolicy left to the BP child (ServerOnly is correct for a grant);
	// we additionally guard on authority inside ActivateAbility.
}

void UAFLAG_GrantLoadout::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	// See the header: the one-shot avatar-init sweep misses late-granted specs. This covers both
	// orderings and is idempotent (TryActivateAbilityOnSpawn early-outs on Spec.IsActive()).
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOADOUT: OnAvatarSet re-attempting on-spawn activation (specActive=%d)."),
		Spec.IsActive() ? 1 : 0);
	TryActivateAbilityOnSpawn(ActorInfo, Spec);
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

	// ASC-READY DEFERRAL (the A-pose first-equip race): granting here can precede the pawn's ASC
	// wiring, and FLyraEquipmentList::AddEntry grants a weapon's AbilitySets only `if (ASC)` --
	// silently, so the first equipped weapon holds no anim-layer abilities until re-equipped.
	// Route the grant through the pawn extension's OnAbilitySystemInitialized (the delegate proven
	// from pawn-side code: AFLDeathComponent, AFLHubNetProfileComponent) -- it calls IMMEDIATELY
	// when the ASC is already up, so both orderings land the grant post-ASC.
	if (APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get()))
	{
		if (ULyraPawnExtensionComponent* PawnExt = ULyraPawnExtensionComponent::FindPawnExtensionComponent(AvatarPawn))
		{
			PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
				FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UAFLAG_GrantLoadout::GrantWhenReady));
			return; // GrantWhenReady ends the ability when it runs
		}
	}
	UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOADOUT: no pawn extension on the avatar -- granting immediately (race-exposed)."));
	GrantWhenReady();
}

void UAFLAG_GrantLoadout::GrantWhenReady()
{
	const FGameplayAbilitySpecHandle Handle = CurrentSpecHandle;
	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	const FGameplayAbilityActivationInfo ActivationInfo = CurrentActivationInfo;
	if (!ActorInfo || !IsActive())
	{
		return; // late/duplicate broadcast (respawn re-init) after this activation already granted+ended
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOADOUT: ASC ready -- granting now."));

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

	// DEFER TO THE COSMETIC SELECTION (Block 28). Read the player's DURABLE, replicated selection off the
	// PlayerState -- never "has the cosmetic spine run yet". Both this OnSpawn ability and
	// UAFLSkinColorControllerComponent::RefreshWeaponForPawn hang off possession with no guaranteed order
	// between them; keying on state rather than sequence is what makes that race stop mattering.
	//
	// Slots 0..N are still granted and populated above -- only the "what do I spawn holding" decision is
	// handed over, so the player can still cycle to Ripsaw/Verdant/Scatterhawk.
	bool bCosmeticWeaponSelected = false;
	if (bDeferActiveSlotToCosmeticSelection)
	{
		if (const APlayerState* PS = Controller->PlayerState)
		{
			if (const UAFLCosmeticLoadoutComponent* Loadout = PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>())
			{
				bCosmeticWeaponSelected = (Loadout->GetSelection().WeaponId != NAME_None);
			}
		}
	}

	// Equip the active slot so the hero holds a weapon on spawn (BP event -> SetActiveSlotIndex).
	if (GrantedCount > 0 && !bCosmeticWeaponSelected)
	{
		EquipActiveSlot(ActiveSlotIndex);
	}
	else if (bCosmeticWeaponSelected)
	{
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_LOADOUT: cosmetic WeaponId selected -- slots granted, active-slot equip deferred to the selection."));
	}

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_LOADOUT: granted %d/%d weapons on %s, active slot %d."),
		GrantedCount, Weapons.Num(), *GetNameSafe(Controller), ActiveSlotIndex);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
