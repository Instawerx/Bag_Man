// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "AFLBodyZone.h"   // EAFLBodyZone (AFLCore)
#include "AFLDismemberCosmeticTarget.h"   // IAFLDismemberCosmeticTarget (AFLVFX) -- the cold-loadable cue's seam
#include "Loot/AFLPartReattachTarget.h"   // V7-2: IAFLPartReattachTarget (AFLCombat) -- the scattered-part owner-reattach seam

#include "AFLDismemberComponent.generated.h"

class UAFLDismemberZoneSet;
class USkeletalMeshComponent;   // B-2 FIX: GatherZoneMeshes() return type
struct FAFLDismemberZone;
struct FAFLOverkillMessage;
struct FGameplayTag;

/**
 * Victim-side dismemberment listener. Subscribes to Event.Damage.Overkill.AFL
 * (broadcast server-side by UAFLDamageExecCalc, carrying BoneName) and, when the
 * overkill victim is THIS component's owner, resolves the killing-blow bone to a
 * zone row in the ZoneSet and severs that zone.
 *
 * S4-INC1: DATA-DRIVEN. The former hard-coded head path (BoneName=="head", a
 * TSubclassOf<AAFLDismemberedHead>, literal +-100/+500 impulse, the HeadPop cue) is
 * generalized into the ZoneSet table: FindZoneForBone -> SeverZone(row), where the row
 * carries the severed bone, the (soft) prop class, the impulse range, the (soft) cue,
 * and the (soft) consequence GE. The head is just the Head row -- the proven head
 * behavior is preserved by authoring that row identically (PHASE B).
 *
 * SHAPE NOTE -- deliberately NOT a copy of UAFLHitConfirmComponent's filter:
 * HitConfirm is client-feedback (gates on IsLocalController, filters "we instigated").
 * This is the OPPOSITE -- the overkill broadcast is server-side and dismemberment is
 * authoritative gameplay on the VICTIM. So: register only on the server (HasAuthority),
 * NO local-controller gate, filter on Payload.Target == GetOwner() (am I the one who died?).
 *
 * AAA: the ZoneSet + every per-zone asset (prop class, consequence GE, spark FX) is a
 * SOFT reference. The component async-loads the ZoneSet, then async-loads the matched
 * row's prop/GE before spawning/applying -- no hard refs baked into the component, no
 * hard cast to any specific pawn/BP (the zone is resolved by bone -> data).
 */
UCLASS(ClassGroup=(AFL), meta=(BlueprintSpawnableComponent))
class AFLDISMEMBER_API UAFLDismemberComponent : public UActorComponent, public IAFLDismemberCosmeticTarget, public IAFLPartReattachTarget
{
	GENERATED_BODY()

public:
	UAFLDismemberComponent();

	//~IAFLDismemberCosmeticTarget -- the FName front door the AFLVFX persistent cue (AAFLCueNotify_ZoneSever)
	// calls on EVERY client. Maps the cue-tag leaf -> EAFLBodyZone, then runs the existing UNCHANGED
	// ApplyZoneHideCosmetic / ApplyZoneRestoreCosmetic (GatherZoneMeshes + Hide/UnHideBoneByName).
	virtual void ApplyZoneHideByLeaf_Implementation(FName ZoneLeaf) override;
	virtual void ApplyZoneRestoreByLeaf_Implementation(FName ZoneLeaf) override;
	//~End of IAFLDismemberCosmeticTarget

	//~IAFLPartReattachTarget (v7) -- the seam a scattered part pickup calls when its OWNER reclaims it: the FULL
	// owner-reattach (RestoreZone -- un-hide the bone + clear State.Decapitated + refill the zone HP). Server-auth.
	virtual void ReattachPart_Implementation(EAFLBodyZone Zone) override;
	//~End of IAFLPartReattachTarget

	/** The data-driven zone table (soft, async-loaded at sever time). Authored in PHASE B. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Dismember")
	TSoftObjectPtr<UAFLDismemberZoneSet> ZoneSet;

	/**
	 * S4-INC3 PHASE B-1: the faithful inverse of SeverZone -- the shared RECOVERY method both
	 * reattach paths call (head self-retrieve now; limb loot-pack P2). Server-authority:
	 *   1. UnHideBoneByName(SeveredBone) -- reverse HideBoneByName (reveals the bone + restores
	 *      its physics body). Bone resolved from the loaded ZoneSet row for Zone.
	 *   2. Remove the zone tag: State.Dismembered.<Zone> (limbs) / State.Decapitated (head).
	 *   3. Restore the zone-HP to its seed constant (RestoreGE applied to the owner ASC).
	 * (Head) clearing State.Decapitated auto-pops the decap camera mode (tag-driven).
	 * The UnHide has the same NetMulticast consideration as the hide (AFL-0408 -- deferred for
	 * real net play; fine in single-PIE server==client).
	 */
	UFUNCTION(BlueprintCallable, Category="AFL|Dismember")
	void RestoreZone(EAFLBodyZone Zone);

	/**
	 * S4-INC4 (AFL-0408): the ALL-CLIENT cosmetic half, called by UAFLCueNotify_ZoneSever on the
	 * persistent GameplayCue.Combat.Dismember.State.<Zone> cue. ApplyZoneHideCosmetic hides the zone
	 * bone on the full visible mesh set (GatherZoneMeshes + HideBoneByName PBO_Term -- the B-2 logic,
	 * moved here); ApplyZoneRestoreCosmetic un-hides it. These run on EVERY client (no authority guard --
	 * the SEVER is server-auth via AddGameplayCue; the RENDER is all-client). The bone is resolved from
	 * the loaded ZoneSet for the zone. Net-mode-tagged logging proves the hide ran client-side.
	 */
	void ApplyZoneHideCosmetic(EAFLBodyZone Zone);
	void ApplyZoneRestoreCosmetic(EAFLBodyZone Zone);

	/** GE that re-seeds the zone-HP to full on reattach (soft, async-loaded). Authored PHASE B-2
	 *  (the same Override values GE_AFL_Combat_InitData seeds -- data-driven, not hardcoded twice). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Dismember")
	TSoftClassPtr<class UGameplayEffect> ZoneRestoreEffect;

	/** The head loot-box spawned on decapitation (soft, async-loaded). BP child of AAFLHeadLootBox.
	 *  Initialize'd with the owner pawn so self-retrieve reattaches + owner-death vanishes it (PHASE B-2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Dismember")
	TSoftClassPtr<class AAFLHeadLootBox> HeadLootBoxClass;

	/** GE granting State.Decapitated on head-sever (soft, async-loaded). The head's consequence GE
	 *  (the row's ConsequenceGE stays empty for the head -- this is granted directly by OnSever). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Dismember")
	TSoftClassPtr<class UGameplayEffect> DecapitatedEffect;

	/** S4 TUMBLE TIER 2: the head loot-box pop, as a TARGET VELOCITY in cm/s (the head's ApplyPopImpulse uses
	 *  bVelChange=TRUE -- mass/scale-independent; a force-pop launches the tiny head-sized sphere off-screen).
	 *  The loot path applies NO pop otherwise (the head just free-falls + settles). Lateral-dominant so the
	 *  head ROLLS ~1-1.5m before resting (150 cm/s); a modest vertical pop arcs it off the neck (120 cm/s).
	 *  EditDefaultsOnly -> PIE-tunable, no rebuild. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Dismember")
	float HeadPopLateralImpulse = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Dismember")
	float HeadPopVerticalImpulse = 120.f;

	/** PRESENTATION (dismember pass): the gib's end-over-end TUMBLE, as a target ANGULAR velocity in rad/s
	 *  (AddAngularImpulseInRadians bVelChange=TRUE -- mass/scale-independent). A linear-only pop through the COM
	 *  is zero torque -> the gib SLIDES; this spins it about the axis perpendicular to travel so it ROLLS. Modest
	 *  = a clean tumble, not a frenzy. Shared by the head + limb pops. EditDefaultsOnly -> PIE-tunable, no rebuild. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Dismember")
	float PopAngularImpulse = 6.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnOverkill(FGameplayTag Channel, const FAFLOverkillMessage& Payload);

	/**
	 * S4-INC3 PHASE B-2: the LIVE sever consumer. Listens to Event.Dismember.Sever.AFL (broadcast by
	 * UAFLDamageExecCalc when a zone's dedicated HP depletes) and, when the victim is THIS owner, runs
	 * the cosmetic detach (SeverZone) for the resolved zone -- AND, for the HEAD, grants State.Decapitated
	 * and spawns the head loot-box (Initialize'd with the owner). This is the trigger the head now uses:
	 * decap no longer overkills (B-1 made the head consume the hit, EffectiveDamage=0), so OnOverkill never
	 * fires for a head -- OnSever is the only path that pops the head. Limbs keep OnOverkill this increment
	 * (a head never overkills, so there is no double-sever for the proven case).
	 */
	void OnSever(FGameplayTag Channel, const struct FAFLDismemberSeverMessage& Payload);

	/** Async-load the ZoneSet, resolve HitBone -> row, then SeverZone(row). Server-only.
	 *  bIsHeadSever: also grant State.Decapitated + spawn the head loot-box once the row resolves. */
	void ResolveAndSever(FName HitBone, bool bIsHeadSever = false, const FVector& HitDirection = FVector::ZeroVector);

	/**
	 * Sever one zone (server-authority). Generalizes the proven head path:
	 *   1. HideBoneByName(Row.SeveredBone, PBO_Term) on the owner's skeletal mesh
	 *   2. spawn Row.PropClass (async-loaded) at the severed-bone world transform
	 *   3. ApplyPopImpulse (randomized within Row.ImpulseXYRange + ImpulseZ)
	 *   4. fire Row.CueTag (cosmetic, replicated via the victim ASC) if set
	 *   5. apply Row.ConsequenceGE (async-loaded) to the victim ASC if set
	 * Row is copied (not pointer-held) because the async continuations outlive the
	 * FindZoneForBone scan's array-pointer lifetime.
	 */
	void SeverZone(const FAFLDismemberZone& Row, const FVector& HitDirection = FVector::ZeroVector);

	/**
	 * S4-INC3 PHASE B-2 FIX: every skeletal mesh a zone-bone hide/un-hide must touch. The hero's own
	 * CharacterMesh0 is the anim driver + the prop-transform source, but the VISIBLE robot body is a
	 * separate CopyPose-driven CharacterPart child actor (NOT a leader-pose follower) -- hiding only
	 * CharacterMesh0 never touches the visible mesh, so the head stayed on. Returns CharacterMesh0 +
	 * every ULyraPawnComponent_CharacterParts part mesh. On non-cosmetic actors (the dummy, no parts
	 * component) the set is just CharacterMesh0 -- the proven dummy behavior is preserved. PBO_Term is
	 * a post-anim render state, so it survives the per-frame CopyPose (the standard dismember approach).
	 */
	TArray<USkeletalMeshComponent*> GatherZoneMeshes() const;

	/** Grant the State.Decapitated consequence GE (DecapitatedEffect, async-loaded) to the owner ASC. */
	void ApplyDecapitatedEffect();

	/** A head-sever spawns + Initializes the loot-box once the loot-box class resolves. Server-only.
	 *  HeadLootWatts = the head zone row's LootWatts (the COMBAT-LOOT value an ENEMY collector is granted). */
	void SpawnHeadLootBox(int32 HeadLootWatts, const FVector& HitDirection = FVector::ZeroVector);

	FGameplayMessageListenerHandle ListenerHandle;        // Event.Damage.Overkill.AFL (limbs)
	FGameplayMessageListenerHandle SeverListenerHandle;   // Event.Dismember.Sever.AFL (head, B-2)
};
