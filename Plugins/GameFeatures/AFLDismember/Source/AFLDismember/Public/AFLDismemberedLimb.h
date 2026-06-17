// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLDismemberedPart.h"
#include "AFLBodyZone.h"   // EAFLBodyZone (AFLCore) -- the limb stores its zone for the owner-retrieve RestoreZone
#include "Loot/AFLLootable.h"   // IAFLLootable contract (AFLCombat)

#include "AFLDismemberedLimb.generated.h"

class UStaticMesh;
class UAFLSkinColorAsset;
class UMaterialInstanceConstant;
class UAFLGrabbableComponent;   // COMBAT-LOOT: the grab substrate (AFLMovement) the limb wears to be retrievable
class UAFLLootGrantComponent;   // the shared loot grant (AFLCombat, Loot Phase 1)

/**
 * S4 LIMB-GIB (PHASE 3, AFL-0408-FU-LIMBMESH): the detached LIMB prop (arm/leg) -- a subclass of the
 * shared AAFLDismemberedPart base, the proven prim-as-root physics body. The DIRECT MIRROR of
 * AAFLDismemberedHead: same two replicated identity layers (per-skin slot-1 base MIC + the victim's
 * UAFLSkinColorAsset on top), resolved by UAFLDismemberComponent at sever from the victim's
 * UAFLSkinColorComponent / GetMaterial(1)->Parent, applied on every client via OnRep so the limb reads
 * as that specific robot. The body IS the base PartMesh (UStaticMeshComponent) -> simulates + rolls +
 * replicates + grabs on the SAME path the head + every limb prop uses.
 *
 * POP (matches the head -- VELOCITY, not force): ApplyPopImpulse is overridden to bVelChange=true, exactly
 * like AAFLDismemberedHead. The earlier assumption that "a limb is comparable-or-larger mass so the base
 * force-pop is fine" was WRONG once watched in PIE -- the extracted limb gibs are small light convex hulls
 * (arm ~38x38x62cm), so the base force-pop (impulse / mass) launched them off-screen instantly with no
 * visible tumble. Treating the same DA impulse vector (ImpulseZ=500, XY +-100) as a TARGET VELOCITY
 * (mass-independent) gives the gentle pop+tumble the head already gets. No roll-audio default (the head's
 * Head_Roll_* is head-specific; a limb-impact sound is a future polish task, not this C++).
 *
 * The actual limb GIB MESHES (SM_AFL_RobotArm_Gib / SM_AFL_RobotLeg_Gib) are a separate DATA task --
 * extracted in Blender by arm/leg bone weight via the blender_mcp bridge, exactly like SM_AFL_RobotHead_Gib
 * (origin-centered, watertight, one convex hull). Until they exist this class works with a placeholder mesh
 * set on the BP child / EditDefaultsOnly LimbGibMesh, mirroring the head's HeadGibMesh slot.
 *
 * COMBAT-LOOT (clone of AAFLHeadLootBox, B1 -- the loot behavior lives ON the limb, not a separate class):
 * the severed limb PERSISTS (InitialLifeSpan=0, limbs-only) and is a retrievable item -- it wears a
 * UAFLGrabbableComponent (the same grab substrate the head loot-box uses; discovery finds it, carry attaches
 * it to hand_r) and binds OnGrabbedBy. Initialize(owner, zone, lootWatts) (called at the SeverZone spawn site)
 * ties it to the victim + stores the zone + the loot value. Pickup branches server-side EXACTLY like the head:
 *   - OWNER (grabber controller == owner controller) -> UAFLDismemberComponent::RestoreZone(<zone>) (reattach:
 *     un-hide + revert the consequence GE + refill zone HP -- RestoreZone is already zone-generic) + Destroy.
 *     NO Watts (you do not profit from your own body).
 *   - OPPOSING -> UAFLWalletComponent::EarnWattsAuthority(LootWatts="head/8"=20, "limb-loot") on the grabber's
 *     wallet + consume. Owner-death-vanish is inherited behavior (uncollected limb of a dead owner disappears).
 * See Docs/IRONICS_ECONOMY_SPEC.md section 3a. The head's own grant goes live in the same increment (its
 * enemy-collect was a P2 stub); head=160 / limb=20 are tune-at-playtest starting values.
 */
UCLASS()
class AFLDISMEMBER_API AAFLDismemberedLimb : public AAFLDismemberedPart, public IAFLLootable
{
	GENERATED_BODY()

public:
	AAFLDismemberedLimb();

	/** SERVER-ONLY: hand the victim's skin color to the limb (replicates to all clients via OnRep).
	 *  Called by UAFLDismemberComponent at sever/spawn from the victim's UAFLSkinColorComponent.
	 *  MIRRORS AAFLDismemberedHead::SetHeadSkinColor. */
	void SetPartSkinColor(UAFLSkinColorAsset* InColor);

	/** SERVER-ONLY: hand the victim's per-skin slot-1 base MATERIAL (the replication-safe MIC) to the limb
	 *  (replicates via OnRep). Resolved by UAFLDismemberComponent at spawn from GetMaterial(1)->Parent (the
	 *  MIC behind the runtime MID). MIRRORS AAFLDismemberedHead::SetHeadMaterial. */
	void SetPartMaterial(UMaterialInstanceConstant* InMaterial);

	/** SERVER-ONLY (called at the SeverZone spawn site): tie this limb to the pawn it was severed from, store
	 *  its body zone (for the owner-retrieve RestoreZone) + its COMBAT-LOOT value (granted to an ENEMY
	 *  collector). Binds owner death so an uncollected limb of a dead owner vanishes. MIRRORS
	 *  AAFLHeadLootBox::Initialize. */
	void Initialize(APawn* InOwnerPawn, EAFLBodyZone InZone, int32 InLootWatts);

	//~ IAFLLootable -- expose the grant component polymorphically (Phase-3 director/queries; no reparenting).
	virtual UAFLLootGrantComponent* GetLootGrantComponent() const override { return LootGrant; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Bound to the grabbable's OnGrabbedBy (server-auth) -- branches owner self-retrieve (reattach via
	 *  RestoreZone, no Watts) vs opposing collect (EarnWattsAuthority limb-loot). MIRRORS the head loot-box. */
	UFUNCTION()
	void OnGrabbedBy(AActor* Grabber);

	/** Bound to the owner pawn's ULyraHealthComponent::OnDeathStarted -- vanish if uncollected. MIRRORS the head. */
	UFUNCTION()
	void OnOwnerDeathStarted(AActor* OwningActor);

	/** Bound to LootGrant->OnOwnerRetrieved -- the owner-branch action: reattach via RestoreZone(LootZone) +
	 *  destroy. The grant component never references RestoreZone (the dependency-inversion seam). */
	UFUNCTION()
	void HandleOwnerRetrieved(AActor* Retriever);

	/** VELOCITY pop (bVelChange=true), mirroring AAFLDismemberedHead: the limb gib is a light convex hull,
	 *  so the base force-pop (impulse/mass) launches it off-screen. Interpret the DA impulse as a target
	 *  velocity instead -> visible pop + tumble. */
	virtual void ApplyPopImpulse(const FVector& Impulse) override;

	/** Apply the victim appearance to the gib (runs on every client): assign PartMaterial (the MIC) to every
	 *  slot, then drive the color params on TOP via the proven AAFLCharacterPartActor::ApplySkinColor pattern
	 *  (per-slot CreateAndSetMaterialInstanceDynamic + GetColors()/GetScalars()/GetTextures()). Idempotent;
	 *  re-run on each OnRep so whichever of {PartMaterial, PartSkinColor} replicates second completes the look.
	 *  MIRRORS AAFLDismemberedHead::ApplyHeadAppearance exactly. */
	void ApplyLimbAppearance();

	UFUNCTION()
	void OnRep_PartSkinColor();

	UFUNCTION()
	void OnRep_PartMaterial();

	/** The grab substrate marker + policy (discovery + carry) -- the limb IS retrievable loot. BP child sets
	 *  GrabAbility = GA_AFL_Grab_C (same as the head loot-box BP). MIRRORS AAFLHeadLootBox::Grabbable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAFLGrabbableComponent> Grabbable;

	/** The generalized loot grant (eligibility + grant-once + Watts), shared with the head/caches (Loot Phase 1).
	 *  Configured at Initialize; OnGrabbedBy routes to it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAFLLootGrantComponent> LootGrant;

private:
	/** The limb gib mesh -- SM_AFL_RobotArm_Gib / SM_AFL_RobotLeg_Gib (origin-centered static mesh + convex
	 *  collision). EditDefaultsOnly so the per-limb BP child sets the correct mesh, no code per limb. Mirrors
	 *  the head's HeadGibMesh. Set on PartMesh in ctor when present. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> LimbGibMesh;

	/** COMBAT-LOOT: the pawn this limb was severed from (the owner-retrieve target + death source). Weak: the
	 *  pawn may die. MIRRORS AAFLHeadLootBox::OwnerPawn. */
	UPROPERTY()
	TWeakObjectPtr<APawn> OwnerPawn;

	/** COMBAT-LOOT: this limb's body zone -- the owner-retrieve calls RestoreZone(LootZone) to reattach it. */
	EAFLBodyZone LootZone = EAFLBodyZone::None;

	/** The victim's skin color, replicated so the limb reads as identity on every client + late-join.
	 *  MIRRORS AAFLDismemberedHead::HeadSkinColor. */
	UPROPERTY(ReplicatedUsing = OnRep_PartSkinColor)
	TObjectPtr<UAFLSkinColorAsset> PartSkinColor;

	/** The victim's per-skin slot-1 base material (a UMaterialInstanceConstant -> replication-safe by pointer,
	 *  NEVER the transient MID). MIRRORS AAFLDismemberedHead::HeadMaterial. */
	UPROPERTY(ReplicatedUsing = OnRep_PartMaterial)
	TObjectPtr<UMaterialInstanceConstant> PartMaterial;
};
