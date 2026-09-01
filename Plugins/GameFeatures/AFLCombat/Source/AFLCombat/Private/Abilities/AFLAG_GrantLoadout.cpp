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

bool UAFLAG_GrantLoadout::ShouldDeferEquipToCosmeticSelection(const AController* Controller) const
{
	// See the header: bot symmetry with the FIX A IsPlayerController gate in RefreshWeaponForPawn.
	// A deferred equip with no consumer is an equip that never happens.
	if (!bDeferActiveSlotToCosmeticSelection || !Controller || !Controller->IsPlayerController())
	{
		return false;
	}
	if (const APlayerState* PS = Controller->PlayerState)
	{
		if (const UAFLCosmeticLoadoutComponent* Loadout = PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>())
		{
			return Loadout->GetSelection().WeaponId != NAME_None;
		}
	}
	return false;
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

	AController* Controller = GetControllerFromActorInfo();

	if (bLoadoutGranted)
	{
		// Once-per-controller-life latch holds for the GRANT (duplicating items is the bug it
		// exists for), but each NEW pawn on this controller still needs its EQUIP driven: the
		// persistent quickbar keeps ActiveSlotIndex while the fresh pawn's equipment manager is
		// empty, SetActiveSlotIndex(same index) is a stock no-op, and no possession hook re-equips.
		// Without this, every round-reset / respawned pawn plays ZERO anim layers -- the A-pose +
		// glide bots in the drone-capture reels. Bounce through another slot to force the equip.
		// Same pawn as last time = a duplicate init broadcast, not a respawn -- skip (bouncing a
		// live pawn would stomp a cosmetic-selected weapon back to the loadout slot).
		APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
		if (AvatarPawn && AvatarPawn != LastEquippedPawn.Get() && !ShouldDeferEquipToCosmeticSelection(Controller))
		{
			const int32 BounceSlot = (ActiveSlotIndex == 0) ? 1 : 0;
			UE_LOG(LogAFLCombat, Log,
				TEXT("AFL_LOADOUT: respawn on %s -- re-driving equip for new pawn %s (bounce slot %d -> %d)."),
				*GetNameSafe(Controller), *GetNameSafe(AvatarPawn), BounceSlot, ActiveSlotIndex);
			EquipActiveSlot(BounceSlot);
			EquipActiveSlot(ActiveSlotIndex);
			LastEquippedPawn = AvatarPawn;
		}
		else
		{
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOADOUT: already granted this controller life -- skipping duplicate activation."));
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOADOUT: ASC ready -- granting now."));

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

	bLoadoutGranted = true; // controller + inventory resolved -- this activation IS the grant

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
	// PLAYERS ONLY (the helper's IsPlayerController gate): the deferral's sole consumer skips bots, so a
	// deferred bot equip never lands and the bot A-poses unarmed -- bots always equip here.
	//
	// Slots 0..N are still granted and populated above -- only the "what do I spawn holding" decision is
	// handed over, so the player can still cycle to Ripsaw/Verdant/Scatterhawk.
	const bool bCosmeticWeaponSelected = ShouldDeferEquipToCosmeticSelection(Controller);

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
	LastEquippedPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_LOADOUT: granted %d/%d weapons on %s, active slot %d."),
		GrantedCount, Weapons.Num(), *GetNameSafe(Controller), ActiveSlotIndex);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
