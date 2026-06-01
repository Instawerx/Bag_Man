// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AFLBeamEndpointProvider.h"

#include "AFLBeamChannelComponent.generated.h"


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
	void PublishImpact(const FVector& WorldImpactPoint);

	/**
	 * Publish the current weapon MUZZLE location (world-space) -- the visible beam START.
	 * Written each tick from the ability's muzzle resolve (Pulse's proven ResolveMuzzleLocation
	 * with its weapon_r fallback, never origin). The cue reads it so the beam emits from the
	 * barrel tip (operator precision rule), instead of a synthetic eye-point. Symmetric with
	 * PublishImpact -- the second world point that crosses the gameplay/cosmetic boundary.
	 */
	void PublishMuzzle(const FVector& WorldMuzzleLocation);

	/** Mark the beam channel open (call on ActivateAbility) or closed (call on EndAbility). */
	void SetBeamActive(bool bInActive);

	/** The cue reads this each Tick to drive User."Beam End". World-space. */
	FVector GetBeamImpactPoint() const { return BeamImpactPoint; }

	/** The cue reads this each Tick for the visible beam START (the weapon muzzle). World-space. */
	FVector GetBeamMuzzleLocation() const { return BeamMuzzleLocation; }

	/** True while a beam is channeling. The cue uses it as a sanity gate; the cue's own OnActive/OnRemove are the primary lifecycle. */
	bool IsBeamActive() const { return bBeamActive; }

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
};
