// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AFLDismemberedHead.h"
#include "Loot/AFLLootable.h"          // IAFLLootable contract (AFLCombat)

#include "AFLHeadLootBox.generated.h"

class UAFLGrabbableComponent;
class UAFLLootGrantComponent;
class ULyraHealthComponent;

/**
 * S4-INC3 PHASE B-1: the dismembered-head LOOT-BOX.
 *
 * The locked head model: 3 head shots -> the head pops with ZERO body damage; the head becomes a
 * pickup tied to the owner's life; self-retrieve = reattach; a kill is the normal health bar only
 * (decapitation is a SURVIVABLE state change, not a kill). This actor is that pickup.
 *
 * It is the decoupled head prop (AAFLDismemberedHead -> sphere mesh + roll audio + physics +
 * snapshot replication) PLUS a UAFLGrabbableComponent so the existing grab substrate discovers and
 * carries it -- exactly the forward-compat consumer the grabbable's own docstring names ("dismembered
 * heads"). No new carry code: it wears the proven component and binds its OnGrabbedBy seam.
 *
 * Lifetime (the "tied to owner life" rule):
 *   - InitialLifeSpan = 0 -> it does NOT auto-destroy like the cosmetic 5s pop prop; it persists to
 *     be retrieved.
 *   - It binds the OWNER pawn's ULyraHealthComponent::OnDeathStarted -> if the owner dies before the
 *     head is collected, the head vanishes (a dead player's head is no longer retrievable loot).
 *
 * Pickup dispatch (OnGrabbedBy -> UAFLLootGrantComponent::TryGrant, server-authority -- Loot Phase 1):
 *   - SELF (the grabber is the decapitated owner, matched by controller) -> the grant component fires
 *     OnOwnerRetrieved -> HandleOwnerRetrieved calls UAFLDismemberComponent::RestoreZone(Head) (un-hide,
 *     clear State.Decapitated -> camera auto-pops, restore head-HP) and destroys this loot-box. No Watts.
 *   - ENEMY -> the grant component grants the head's LootWatts (EarnWattsAuthority "head-loot") + marks spent.
 *
 * The spawn path (UAFLDismemberComponent's head-sever, PHASE B-2) calls Initialize(OwnerPawn) right
 * after spawning this in place of the plain head prop for the Head zone.
 */
UCLASS()
class AFLDISMEMBER_API AAFLHeadLootBox : public AAFLDismemberedHead, public IAFLLootable
{
	GENERATED_BODY()

public:
	AAFLHeadLootBox();

	/** Bind this loot-box to the pawn it was severed from (server-side, called right after spawn).
	 *  Stores the owner + binds its death so the head vanishes if the owner dies uncollected.
	 *  InLootWatts = the COMBAT-LOOT value granted to an ENEMY collector (head row's LootWatts, e.g.
	 *  160); the owner self-retrieving reattaches and is granted nothing. Default 0 keeps any prior
	 *  one-arg caller compiling (grants no loot until the DA value is set). */
	UFUNCTION(BlueprintCallable, Category = "AFL|Dismember")
	void Initialize(APawn* InOwnerPawn, int32 InLootWatts = 0);

	/** True once an enemy has collected this head (the grant component's grant-once state). */
	UFUNCTION(BlueprintPure, Category = "AFL|Dismember")
	bool IsCollected() const;

	//~ IAFLLootable -- expose the grant component polymorphically (Phase-3 director/queries; no reparenting).
	virtual UAFLLootGrantComponent* GetLootGrantComponent() const override { return LootGrant; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Bound to the grabbable's OnGrabbedBy -- branches self-retrieve (reattach) vs enemy-collect. */
	UFUNCTION()
	void OnGrabbedBy(AActor* Grabber);

	/** Bound to the owner pawn's ULyraHealthComponent::OnDeathStarted -- vanish if uncollected. */
	UFUNCTION()
	void OnOwnerDeathStarted(AActor* OwningActor);

	/** Bound to LootGrant->OnOwnerRetrieved -- the owner-branch action. The grant component fires this when the
	 *  OWNER retrieves (it never knows about RestoreZone -- the dependency-inversion seam): reattach via
	 *  RestoreZone(Head) + destroy. */
	UFUNCTION()
	void HandleOwnerRetrieved(AActor* Retriever);

	/** The grab substrate marker + policy (discovery + carry). BP child sets GrabAbility = GA_AFL_Grab_C. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAFLGrabbableComponent> Grabbable;

	/** The generalized loot grant (eligibility + grant-once + Watts), shared with limbs/caches (Loot Phase 1).
	 *  Configured at Initialize; OnGrabbedBy routes to it. MIRRORS the proven inline grant, now extracted. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAFLLootGrantComponent> LootGrant;

private:
	/** The pawn this head was severed from (the retrieve target + death source). Weak: the pawn may die. */
	UPROPERTY()
	TWeakObjectPtr<APawn> OwnerPawn;
};
