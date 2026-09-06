// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "Containers/Ticker.h"   // FTSTicker -- bounded verify-and-retry of the round-reset re-equip

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
	/** LATE-GRANT ON-SPAWN FIX (the A-pose loadout race): Lyra's on-spawn sweep runs ONCE at avatar
	 *  init, so a spec granted AFTER it (grant order vs InitAbilityActorInfo flips intermittently)
	 *  was never attempted -- this ability silently dead for the pawn's whole life, zero AFL_LOADOUT
	 *  lines. GAS fires OnAvatarSet in BOTH orderings; re-attempting here is Epic's own idempotent
	 *  pattern (mirrors LyraPlayerController::OnRep_PlayerState's late-PC retry). */
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** The actual grant body -- runs only once the pawn's ASC is INITIALIZED. Granting earlier hit
	 *  Epic's silent AbilitySet skip (FLyraEquipmentList::AddEntry only grants `if (ASC)`), leaving
	 *  the first equipped weapon mute + the pawn A-posed until a manual cycle re-equipped it. */
	void GrantWhenReady();

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

	/** Once-per-controller-life latch. OnAvatarSet fires more than once (Lyra re-runs
	 *  InitAbilityActorInfo during the component-ready cascade), and each re-fire can re-activate
	 *  this ability after the first grant already ended it -- duplicating weapons into the
	 *  persistent controller inventory. Set only on a successful grant, so failure paths retry. */
	UPROPERTY(Transient)
	bool bLoadoutGranted = false;

	/** The pawn whose equip this ability last drove (or handed to the cosmetic spine). Latch-path
	 *  activations compare against it: same pawn = a duplicate init broadcast (skip), a DIFFERENT
	 *  pawn = a respawn on the persistent controller -- the new pawn's equipment manager is empty
	 *  and nothing else re-equips a quickbar on possession change, so the equip must be re-driven
	 *  here or the pawn plays zero anim layers (A-pose + glide, the drone-capture bot bug). */
	TWeakObjectPtr<APawn> LastEquippedPawn;

	/**
	 * DEFER TO A LIVE COSMETIC SELECTION (Block 28). When true and the player's FAFLCosmeticSelection
	 * carries a WeaponId, this ability still grants and slots every loadout weapon but does NOT call
	 * EquipActiveSlot -- so the hero spawns holding the weapon they chose, not slot 0.
	 *
	 * ⚠ IT READS THE SELECTION, NOT AN EXECUTION ORDER. The cosmetic spine
	 * (UAFLSkinColorControllerComponent::RefreshWeaponForPawn) and this OnSpawn ability both hang off
	 * possession and their relative order is NOT guaranteed by anything in the engine or Lyra. Keying
	 * off the durable, replicated PlayerState selection is what makes that order stop mattering -- do
	 * NOT "improve" this by testing whether Refresh has already run.
	 *
	 * With NO selection (WeaponId == NAME_None) this is inert and spawn behaviour is byte-identical to
	 * before it existed, which is the regression gate for every player who never opens Loadout.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loadout")
	bool bDeferActiveSlotToCosmeticSelection = true;

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
	 *
	 * RESPAWN NOTE: the latch path also calls this twice (a bounce through another slot index) --
	 * SetActiveSlotIndex no-ops when the index is unchanged, and on respawn the persistent
	 * quickbar still holds the old index while the NEW pawn's equipment manager is empty. The
	 * bounce forces a real unequip/equip so the new pawn links anim layers. A bounce through an
	 * EMPTY slot is safe (index-valid slots equip nothing and the return trip equips the weapon).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Loadout")
	void EquipActiveSlot(int32 SlotIndex);

	/** True when the equip decision belongs to the cosmetic spine instead of this ability:
	 *  deferral enabled + a durable WeaponId selection + a PLAYER controller. The player gate is
	 *  load-bearing and symmetric with FIX A (bot-fire, 2026-07-17): the deferral's only consumer,
	 *  UAFLSkinColorControllerComponent::RefreshWeaponForPawn, skips non-player controllers -- so a
	 *  deferred BOT equip never happens and the bot plays zero anim layers (A-pose + glide). On a
	 *  logged-in listen host every bot PlayerState used to hydrate the host's persisted selection
	 *  (WeaponId included) through MakePlayerId's global-login fallback, which is how the deferral
	 *  started firing for bots at all. That hydration is now bot-gated at its source
	 *  (UAFLCosmeticLoadoutComponent::IsBotOwned, 2026-09-01) -- a bot selection carries no durable
	 *  WeaponId anymore, so this player gate is the second layer, kept for the same reason FIX A is. */
	bool ShouldDeferEquipToCosmeticSelection(const AController* Controller) const;

	/** Cancel a pending re-equip retry when the ability ends (or is cancelled) so no ticker outlives it. */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	/**
	 * THE ROUND-2 RE-EQUIP RACE (offline-standalone ServerTravel host, proven from a Shipping log 2026-09-05):
	 * on a round-reset respawn the latch-path bounce (EquipActiveSlot) fires while the fresh pawn's
	 * GameFeature-added ULyraEquipmentManagerComponent is not yet attached, so the equip silently no-ops --
	 * no OnEquipped, weapon=None, no anim layer -> A-pose + empty weapon-wheel on BOTH player and bot. The one
	 * shot bounce was proven on a listen host and never covered this offline-host timing. The fix is to VERIFY
	 * the equip actually produced a spawned weapon and, if not, retry the bounce on a bounded ticker until it
	 * takes. Verify-BEFORE-bounce so a pawn that already equipped is never stomped back to the loadout slot.
	 */
	/** True when Pawn already holds a ULyraWeaponInstance whose actors have spawned (the equip took). */
	bool IsLoadoutWeaponSpawned(APawn* Pawn) const;

	/** Bounce the QuickBar active slot through another index to force a real unequip/equip on a fresh pawn. */
	void BounceEquipForPawn();

	/** Begin a bounded next-frame retry loop that re-bounces until IsLoadoutWeaponSpawned(Pawn) or it caps. */
	void StartEquipVerifyRetry(APawn* Pawn);

	/** Remove the retry ticker and clear its state (idempotent; safe to call when none is pending). */
	void StopEquipVerifyRetry();

	/** One retry tick: verify -> latch+end on success, give up after the cap, else re-bounce. Returns
	 *  true to keep ticking, false to unregister. */
	bool TickEquipVerifyRetry();

	/** Active retry-ticker handle (thread-safe core ticker; game-thread only in practice). */
	FTSTicker::FDelegateHandle EquipRetryTickHandle;

	/** The fresh pawn the retry is driving; the loop aborts if the current avatar changes to a newer one. */
	TWeakObjectPtr<APawn> EquipRetryPawn;

	/** Retry attempts so far; capped (~40 x ~0.05s ~= 2s) so a pawn that never accepts the equip cannot spin. */
	int32 EquipRetryAttempts = 0;
};
