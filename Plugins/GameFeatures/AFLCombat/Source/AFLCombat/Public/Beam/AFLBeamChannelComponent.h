// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AFLBeamEndpointProvider.h"

#include "AFLBeamChannelComponent.generated.h"

class AActor;


/**
 * UAFLBeamChannelComponent
 *
 * AFL-0208: the published-value bridge between a sustained-beam GameplayAbility
 * (the authoritative-trace half) and its cosmetic looping GameplayCue (the
 * beam-visual half). It carries exactly one thing across the gameplay/cosmetic
 * boundary, per the laser-system doctrine: a world-space impact point plus an
 * "is the beam live" flag.
 *
 * Why a component (not a UPROPERTY on the pawn): it stays off the Lyra hero base
 * (upstream-mergeable, doctrine-clean), it owns its own replication + lifecycle,
 * and the cue reaches it generically via MyTarget->FindComponentByClass<>(). It
 * is GENERIC -- "the current beam impact + active flag for this pawn's active
 * beam" -- so every laser weapon (Prism today, weapons 3-12 tomorrow) grants it,
 * publishes to it, and its cue reads it. One reusable piece, not a Prism one-off.
 *
 * Data flow (no Niagara here, no attributes here -- just the crossing point):
 *   - Ability (UAFLAG_Laser_Beam) ensures this component exists on its avatar in
 *     ActivateAbility (idempotent ensure, same shape as the Heat_Decay ensure),
 *     then each authoritative tick writes the confirmed Hit.ImpactPoint via
 *     PublishImpact() and sets bBeamActive true on activate / false on end.
 *   - The looping cue (AAFLCueNotify_LaserBeam) captures this component in
 *     OnActive (MyTarget = the ASC's avatar pawn) and each Tick reads
 *     GetBeamImpactPoint() to drive the Niagara User."Beam End".
 *
 * Replication: BeamImpactPoint + bBeamActive are Replicated and written on BOTH
 * the locally-controlled side (immediate, zero-latency local beam) and the
 * authority (so the values replicate to simulated proxies -- other clients see
 * the beam track correctly). The whole reason the cue architecture exists is
 * multiplayer correctness, so this replicates from the start; an owner-only
 * value would force a rework at the 2-client gate.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLBeamChannelComponent : public UActorComponent, public IAFLBeamEndpointProvider
{
	GENERATED_BODY()

public:
	UAFLBeamChannelComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * TWO BEAM SLOTS (Block 22). Slot 0 is the single-beam path every weapon has always used; slot 1
	 * exists so a dual arm-cannon pawn can draw a second beam from its own maw instead of both cannons
	 * fighting over one set of published values.
	 *
	 * ⚠ Deliberately TWO FLAT SCALAR SETS, not a struct or a struct array. AFLCombat is a GameFeature
	 * module: a new net-serialized struct here desyncs FNetSerializeScriptStructCache between server
	 * and client and drops connections — and single-client testing never shows it.
	 */
	static constexpr int32 NumBeamSlots = 2;

	/**
	 * THE slot key, and the only one. Reads the weapon display actor's attach socket:
	 *
	 *     socket == "weapon_lowerarm_l"  ->  slot 1
	 *     EVERYTHING else                ->  slot 0
	 *
	 * Not a lookup table — anything unrecognised (including an actor that isn't attached yet, whose
	 * socket reads NAME_None) falls to slot 0. That is what makes the change non-regressive by
	 * construction: all 60 shipped WID_AFL_* weapons attach at "weapon_r", and so does the RIGHT
	 * cannon's own definition, so every one of them is slot 0 without needing to be told.
	 *
	 * The socket comes from FLyraEquipmentActorToSpawn::AttachSocket, applied by Lyra at
	 * ULyraEquipmentInstance::SpawnEquipmentActors. It is editor-authored data and rides actor
	 * attachment replication, so server, owning client and simulated proxy all derive the SAME slot
	 * with nothing extra replicated. Both the publishing ability and the reading visual component
	 * call THIS function — one rule, no chance of the two sides disagreeing.
	 */
	static int32 ResolveBeamSlotForActor(const AActor* WeaponDisplayActor);

	//~ IAFLBeamEndpointProvider — the cue reads the beam through this contract (no concrete-type dep).
	virtual FVector GetBeamImpactPoint_Implementation() const override { return BeamImpactPoint; }
	virtual FVector GetBeamMuzzleLocation_Implementation() const override { return BeamMuzzleLocation; }
	virtual bool    IsBeamActive_Implementation() const override       { return bBeamActive; }
	//~ End IAFLBeamEndpointProvider

	/**
	 * Publish the current authoritative beam endpoint (world-space). Called by the
	 * beam ability each tick on the locally-controlled side (immediate) and on the
	 * authority (replicates). Also flips bBeamActive true -- a live publish implies
	 * the beam is firing this frame.
	 */
	void PublishImpact(const FVector& WorldImpactPoint, int32 Slot = 0);

	/**
	 * Publish the current weapon MUZZLE location (world-space) -- the visible beam START.
	 * Written each tick from the ability's muzzle resolve (Pulse's proven ResolveMuzzleLocation
	 * with its weapon_r fallback, never origin). The cue reads it so the beam emits from the
	 * barrel tip (operator precision rule), instead of a synthetic eye-point. Symmetric with
	 * PublishImpact -- the second world point that crosses the gameplay/cosmetic boundary.
	 */
	void PublishMuzzle(const FVector& WorldMuzzleLocation, int32 Slot = 0);

	/** Mark the beam channel open (call on ActivateAbility) or closed (call on EndAbility). */
	void SetBeamActive(bool bInActive, int32 Slot = 0);

	/** The cue reads this each Tick to drive User."Beam End". World-space. Slot 0 — unchanged. */
	FVector GetBeamImpactPoint() const { return BeamImpactPoint; }

	/** The cue reads this each Tick for the visible beam START (the weapon muzzle). World-space. Slot 0 — unchanged. */
	FVector GetBeamMuzzleLocation() const { return BeamMuzzleLocation; }

	/** True while a beam is channeling. The cue uses it as a sanity gate; the cue's own OnActive/OnRemove are the primary lifecycle. Slot 0 — unchanged. */
	bool IsBeamActive() const { return bBeamActive; }

	// Slot-aware readers. The argument-less versions above are kept EXACTLY as they were so the
	// IAFLBeamEndpointProvider thunks and AAFLCueNotify_LaserBeam keep reading slot 0 with no edit --
	// the interface in AFLVFX is untouched by this change. Out-of-range slots clamp to 0 rather than
	// returning garbage: a bad slot should draw the primary beam, never nothing.
	FVector GetBeamImpactPoint(int32 Slot) const    { return Slot == 1 ? FVector(BeamImpactPointSlot1)    : FVector(BeamImpactPoint); }
	FVector GetBeamMuzzleLocation(int32 Slot) const { return Slot == 1 ? FVector(BeamMuzzleLocationSlot1) : FVector(BeamMuzzleLocation); }
	bool    IsBeamActive(int32 Slot) const          { return Slot == 1 ? bBeamActiveSlot1                 : bBeamActive; }

protected:
	/**
	 * The current beam endpoint in WORLD space. FVector_NetQuantize: a beam endpoint
	 * is a cosmetic world position; sub-centimetre precision is wasted bandwidth, and
	 * NetQuantize (1 unit) matches what the marketplace systems consume for User."Beam End".
	 */
	UPROPERTY(Replicated, Transient)
	FVector_NetQuantize BeamImpactPoint = FVector::ZeroVector;

	/**
	 * The weapon MUZZLE in WORLD space -- the visible beam START. Same FVector_NetQuantize
	 * rationale as the endpoint. Resolved gameplay-side (Pulse's muzzle resolve + fallback)
	 * so the cosmetic cue stays a pure consumer.
	 */
	UPROPERTY(Replicated, Transient)
	FVector_NetQuantize BeamMuzzleLocation = FVector::ZeroVector;

	/** True while the owning pawn has a beam channel open. */
	UPROPERTY(Replicated, Transient)
	bool bBeamActive = false;

	// ---------------------------------------------------------------------------------------------
	// SLOT 1 — the second simultaneous beam (dual arm-cannon LEFT side). Same three values, same
	// types, same unconditional replication as slot 0 above; flat scalars rather than an array so no
	// new net-serialized struct enters this GameFeature module.
	//
	// On a single-beam pawn these stay at their zero defaults and replicate as zeros. That waste is
	// accepted deliberately: conditional replication to avoid it would add a second, subtler failure
	// mode (a slot that silently stops updating for some connections) to buy back a few bytes.
	// ---------------------------------------------------------------------------------------------

	/** Slot-1 beam endpoint, world-space. Zero until a slot-1 weapon publishes. */
	UPROPERTY(Replicated, Transient)
	FVector_NetQuantize BeamImpactPointSlot1 = FVector::ZeroVector;

	/** Slot-1 muzzle, world-space — the second visible beam START. */
	UPROPERTY(Replicated, Transient)
	FVector_NetQuantize BeamMuzzleLocationSlot1 = FVector::ZeroVector;

	/** True while a slot-1 beam is channeling. */
	UPROPERTY(Replicated, Transient)
	bool bBeamActiveSlot1 = false;
};
