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
#include "Equipment/LyraEquipmentManagerComponent.h"   // verify the round-reset re-equip actually took
#include "Equipment/LyraEquipmentInstance.h"           // GetSpawnedActors()
#include "Weapons/LyraWeaponInstance.h"                 // the equipped-weapon type to verify

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

int32 UAFLAG_GrantLoadout::CountLoadoutDefsPresent(const ULyraInventoryManagerComponent* Inventory, int32& OutDefsTotal) const
{
	// The CONSUMER'S state: how many of this loadout's item definitions the controller's inventory holds RIGHT NOW.
	// After the inherited hero graph's ClearInventory (K2_OnReset for survivors of a round reset, K2_OnDeathFinished
	// for the dead) this is 0 -- and that, not a boolean set in a previous pawn life, is what decides a re-grant.
	// GetTotalItemCountByDefinition is UE_API-exported (LyraInventoryManagerComponent.h) -- no engine fork.
	OutDefsTotal = 0;
	int32 Present = 0;
	for (const TSubclassOf<ULyraInventoryItemDefinition>& ItemDef : Weapons)
	{
		if (!ItemDef)
		{
			continue;
		}
		++OutDefsTotal;
		if (Inventory && Inventory->GetTotalItemCountByDefinition(ItemDef) > 0)
		{
			++Present;
		}
	}
	return Present;
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
	APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());

	// The InventoryManager lives on the CONTROLLER (LAS_ShooterGame_StandardComponents adds it there).
	ULyraInventoryManagerComponent* Inventory =
		Controller ? Controller->FindComponentByClass<ULyraInventoryManagerComponent>() : nullptr;

	// ---------------------------------------------------------------------------------------------------------
	// THE STATE READ (the round-2 A-pose, 2026-09-06 -- root-caused from the Shipping log + a 4-way code trace).
	//
	// Lyra's own pairing is "ClearInventory on death/reset, re-grant on every possession". The teardown half runs
	// per pawn life in the INHERITED ShooterCore hero graph (B_Hero_ShooterMannequin::ClearInventory off
	// K2_OnReset -- which UAFLRoundManagerComponent::Server_ResetRoundActors triggers on every survivor via
	// OldPawn->Reset() -- and off K2_OnDeathFinished for the dead): RemoveItemFromSlot on every QuickBar slot
	// (Slots[i]=null, ActiveSlotIndex=-1) + RemoveItemInstance on every item. The controller reaches round 2 EMPTY.
	// The grant half had been made once-per-CONTROLLER-life by the bLoadoutGranted latch (cc379c89, 2026-08-30,
	// added to stop duplicate items on a mid-round respawn where the inventory PERSISTS). The latch never knew
	// about the wipe: on the fresh round-2 pawn it re-drove the EQUIP (a slot bounce) against slots that were now
	// null -- ULyraQuickBarComponent::EquipItemInSlot equips nothing for a null slot, silently -- so no weapon was
	// ever equipped, no anim layer was ever linked (Lyra links them only from ULyraWeaponInstance::OnEquipped;
	// there is no default unarmed layer in C++) and every pawn stood in A-pose with an empty wheel. The earlier
	// "retry the bounce" fix (24cdb47b) could not converge: it verified the OUTPUT (a spawned weapon) but never
	// read the INPUT (the controller's inventory), and every round-2 pawn already had its equipment manager
	// (log: "equipMgr=yes"). Bouncing an empty wheel 41 times equips nothing 41 times.
	//
	// So the latch defers to the CONSUMER'S STATE: if the controller still holds the loadout, this is the
	// same-pawn duplicate broadcast (or a respawn that kept the inventory) and the equip-only path below is
	// correct -- no duplicates. If it holds none/part of it, the controller was cleared and we RESTOCK through the
	// same grant path round 1 used, per-definition idempotent (only definitions that are absent get added).
	// ---------------------------------------------------------------------------------------------------------
	int32 DefsTotal = 0;
	const int32 DefsPresent = CountLoadoutDefsPresent(Inventory, DefsTotal);
	const bool bInventoryHoldsLoadout = (DefsTotal > 0) && (DefsPresent >= DefsTotal);
	const bool bWasLatched = bLoadoutGranted;   // captured BEFORE any re-arm: "this activation is a RESTOCK"

	if (bLoadoutGranted)
	{
		// THE INSTRUMENT -- one line that proves the seam in a single PIE run. Prediction: the round-1 same-pawn
		// re-fire logs items=N defs=N/N; every round-2 pawn logs items=0 defs=0/N equipMgr=yes -> RESTOCK.
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_LOADOUT: LATCH on %s pawn %s -- controller inventory items=%d, loadout defs present=%d/%d, equipMgr=%s -> %s"),
			*GetNameSafe(Controller), *GetNameSafe(AvatarPawn),
			Inventory ? Inventory->GetAllItems().Num() : -1, DefsPresent, DefsTotal,
			(AvatarPawn && AvatarPawn->FindComponentByClass<ULyraEquipmentManagerComponent>()) ? TEXT("yes") : TEXT("no"),
			bInventoryHoldsLoadout ? TEXT("EQUIP-ONLY") : TEXT("RESTOCK"));

		if (!bInventoryHoldsLoadout)
		{
			// RESTOCK: the controller was cleared by the hero graph (round reset / death). Re-arm the latch and fall
			// through to the grant path below -- the same path that built round 1.
			bLoadoutGranted = false;
		}
		else
		{
			// EQUIP-ONLY: the inventory persists (same-pawn duplicate init broadcast, or a respawn that kept it). Each
			// NEW pawn on this controller still needs its EQUIP driven: the persistent quickbar keeps ActiveSlotIndex
			// while the fresh pawn's equipment manager is empty, SetActiveSlotIndex(same index) is a stock no-op, and
			// no possession hook re-equips. Bounce through another slot to force the equip. Same pawn as last time =
			// a duplicate init broadcast, not a respawn -- skip (bouncing a live pawn would stomp a cosmetic-selected
			// weapon back to the loadout slot).
			if (AvatarPawn && AvatarPawn != LastEquippedPawn.Get() && !ShouldDeferEquipToCosmeticSelection(Controller))
			{
				StopEquipVerifyRetry();   // a new pawn supersedes a retry still driving the previous one

				// VERIFY BEFORE BOUNCE -- a fresh pawn that already equipped must NOT be stomped back to the
				// loadout slot (that would clobber a cosmetic-selected weapon, and re-open the same race).
				if (IsLoadoutWeaponSpawned(AvatarPawn))
				{
					UE_LOG(LogAFLCombat, Log,
						TEXT("AFL_LOADOUT: respawn on %s -- new pawn %s already equipped, no bounce needed."),
						*GetNameSafe(Controller), *GetNameSafe(AvatarPawn));
					LastEquippedPawn = AvatarPawn;
				}
				else
				{
					UE_LOG(LogAFLCombat, Log,
						TEXT("AFL_LOADOUT: respawn on %s -- re-driving equip for new pawn %s (bounce)."),
						*GetNameSafe(Controller), *GetNameSafe(AvatarPawn));
					BounceEquipForPawn();
					if (IsLoadoutWeaponSpawned(AvatarPawn))
					{
						LastEquippedPawn = AvatarPawn;
					}
					else
					{
						// The equip did not take this frame -- retry the bounce until it does. The retry latches
						// LastEquippedPawn and ends the ability, so DO NOT end it here.
						StartEquipVerifyRetry(AvatarPawn, /*GraceTicks=*/0);
						return;
					}
				}
			}
			else
			{
				UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOADOUT: already granted this controller life -- skipping duplicate activation."));
			}
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}
	}

	// A RESTOCK is exactly "we came through the latch and it re-armed" -- never inferred from LastEquippedPawn,
	// whose round-1 pawn is destroyed (and the weak ptr invalid) by the time round 2 spawns.
	const bool bRestock = bWasLatched;
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOADOUT: ASC ready -- %s now (defs present %d/%d)."),
		bRestock ? TEXT("RESTOCKING") : TEXT("granting"), DefsPresent, DefsTotal);

	if (!Controller)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOADOUT: no controller in ActorInfo; cannot grant loadout."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
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

		// PER-DEFINITION IDEMPOTENCE: a definition the controller still holds is re-SLOTTED, never re-ADDED (the
		// duplicate-item bug the old latch existed for stays closed). AddItemToSlot no-ops on an occupied slot, so
		// re-slotting a present item is harmless; after a full ClearInventory every slot is null and fills.
		ULyraInventoryItemInstance* Instance = Inventory->FindFirstItemStackByDefinition(ItemDef);
		if (!Instance)
		{
			// SAME proven flow as ShooterCore: AddItemDefinition (C++, exported) -> AddItemToSlot.
			// The QuickBar slotting goes through the BP event (AddItemToSlot is BlueprintCallable
			// but not C++-exportable -- the Lyra Cardinal Rule, see the header).
			Instance = Inventory->AddItemDefinition(ItemDef, /*StackCount=*/1);
		}
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
	LastEquippedPawn = AvatarPawn;

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_LOADOUT: %s %d/%d weapons on %s, active slot %d."),
		bRestock ? TEXT("restocked") : TEXT("granted"), GrantedCount, Weapons.Num(), *GetNameSafe(Controller), ActiveSlotIndex);

	// SAFETY NET on a restock for a cosmetic-selected player: the equip was handed to the cosmetic spine, which
	// hangs off the same possession with no ordering guarantee. Verify a weapon actually lands; if none has after
	// a short grace (the spine's window to equip its selection), bounce-equip the loadout slot so the pawn is
	// armed and animating rather than A-posed -- the spine still REPLACES the primary when it does run. The
	// verify-before-bounce inside the retry means a cosmetic weapon that did land is never stomped.
	if (bRestock && bCosmeticWeaponSelected && GrantedCount > 0 && AvatarPawn && !IsLoadoutWeaponSpawned(AvatarPawn))
	{
		StartEquipVerifyRetry(AvatarPawn, /*GraceTicks=*/10);   // ~0.5 s of verify-only before the first bounce
		return;   // the retry ends the ability
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UAFLAG_GrantLoadout::IsLoadoutWeaponSpawned(APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}
	// A ULyraWeaponInstance whose actors have SPAWNED is the proof the QuickBar equip actually took -- the
	// held weapon mesh exists and its anim-layer abilities are live. GetFirstInstanceOfType is
	// LYRAGAME_API-exported; GetSpawnedActors is inline. An empty/absent instance = the equip silently no-op'd.
	if (ULyraEquipmentManagerComponent* EqMgr = Pawn->FindComponentByClass<ULyraEquipmentManagerComponent>())
	{
		if (const ULyraWeaponInstance* Weapon = EqMgr->GetFirstInstanceOfType<ULyraWeaponInstance>())
		{
			return Weapon->GetSpawnedActors().Num() > 0;
		}
	}
	return false;
}

void UAFLAG_GrantLoadout::BounceEquipForPawn()
{
	// Bounce through another slot so SetActiveSlotIndex is a real change (a same-index set is a stock no-op),
	// forcing a genuine unequip/equip that links the new pawn's anim layers. A bounce through an empty slot
	// is safe -- it equips nothing and the return trip equips the active weapon.
	const int32 BounceSlot = (ActiveSlotIndex == 0) ? 1 : 0;
	EquipActiveSlot(BounceSlot);
	EquipActiveSlot(ActiveSlotIndex);
}

void UAFLAG_GrantLoadout::StartEquipVerifyRetry(APawn* Pawn, int32 GraceTicks)
{
	StopEquipVerifyRetry();
	EquipRetryPawn = Pawn;
	EquipRetryAttempts = 0;
	EquipRetryGraceTicks = FMath::Max(0, GraceTicks);

	TWeakObjectPtr<UAFLAG_GrantLoadout> WeakThis(this);
	// ~0.05s cadence; the cap lives in TickEquipVerifyRetry. Weak-this so a GC'd ability never dereferences,
	// and the tick itself aborts if the avatar has moved on to a newer respawn.
	EquipRetryTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakThis](float /*Dt*/) -> bool
		{
			UAFLAG_GrantLoadout* Self = WeakThis.Get();
			return Self ? Self->TickEquipVerifyRetry() : false;
		}), 0.05f);

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_LOADOUT: equip not yet verified on %s -- starting bounded verify/re-equip retry (grace %d ticks)."),
		*GetNameSafe(Pawn), EquipRetryGraceTicks);
}

void UAFLAG_GrantLoadout::StopEquipVerifyRetry()
{
	if (EquipRetryTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(EquipRetryTickHandle);
		EquipRetryTickHandle.Reset();
	}
	EquipRetryPawn.Reset();
	EquipRetryAttempts = 0;
	EquipRetryGraceTicks = 0;
}

bool UAFLAG_GrantLoadout::TickEquipVerifyRetry()
{
	APawn* Pawn = EquipRetryPawn.Get();
	APawn* CurrentAvatar = CurrentActorInfo ? Cast<APawn>(CurrentActorInfo->AvatarActor.Get()) : nullptr;

	// Abort: the ability ended, the pawn is gone, or a newer respawn moved the avatar on. The handle is reset
	// because returning false unregisters this ticker.
	if (!IsActive() || !Pawn || CurrentAvatar != Pawn)
	{
		EquipRetryTickHandle.Reset();
		EquipRetryPawn.Reset();
		EquipRetryAttempts = 0;
		// If the ability is STILL active here (pawn destroyed, or a NEWER respawn already moved the avatar on),
		// end it so the next OnSpawn re-activates cleanly for the new pawn. Leaving it active would swallow a
		// respawn that landed inside the retry window (TryActivateAbilityOnSpawn early-outs on Spec.IsActive())
		// and re-open the very A-pose this fix closes.
		if (IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
		return false;
	}

	if (IsLoadoutWeaponSpawned(Pawn))
	{
		const int32 Attempts = EquipRetryAttempts;
		LastEquippedPawn = Pawn;
		EquipRetryTickHandle.Reset();       // reset BEFORE EndAbility -> StopEquipVerifyRetry finds no handle
		EquipRetryPawn.Reset();
		EquipRetryAttempts = 0;
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOADOUT: equip VERIFIED for %s after %d tick(s) -- weapon spawned."),
			*GetNameSafe(Pawn), Attempts);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return false;
	}

	if (++EquipRetryAttempts > 40)
	{
		const int32 Attempts = EquipRetryAttempts;
		EquipRetryTickHandle.Reset();
		EquipRetryPawn.Reset();
		EquipRetryAttempts = 0;
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_LOADOUT: equip did NOT verify for %s after %d ticks -- giving up (pawn may A-pose)."),
			*GetNameSafe(Pawn), Attempts);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return false;
	}

	// GRACE: verify-only ticks that give a deferred cosmetic equip its window before we bounce the loadout slot.
	if (EquipRetryAttempts <= EquipRetryGraceTicks)
	{
		return true;
	}

	BounceEquipForPawn();   // re-attempt until the equipment manager accepts the equip
	return true;
}

void UAFLAG_GrantLoadout::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	StopEquipVerifyRetry();   // no ticker may outlive the ability (cancel / interrupt / normal end)
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
