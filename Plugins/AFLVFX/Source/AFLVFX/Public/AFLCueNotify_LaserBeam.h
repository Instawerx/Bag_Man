// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "AFLCueNotify_LaserBeam.generated.h"

class UNiagaraComponent;
class UActorComponent;
class APawn;

/**
 * Looping cue for a continuous beam (GameplayCue.Weapon.Laser.Beam).
 *
 * THIN, PUBLISHED-VALUE cue (AFL-0208). It does NO tracing and NO aim math. The
 * authoritative GameplayAbility (UAFLAG_Laser_Beam) traces, computes the impact,
 * and PUBLISHES it into the firing pawn's UAFLBeamChannelComponent (a replicated
 * world-space point). This cue samples that point each frame and drives the
 * Niagara User."Beam End" -- the only thing that crosses the gameplay/cosmetic
 * boundary is a world-space position and a colour, exactly per the laser doctrine.
 *
 * This replaces the earlier FAT self-tracing cue, which ran its own per-tick
 * LineTrace + camera read + component re-orient. That parallel trace lived in the
 * cosmetic layer, decoupled from the ability's authoritative one, and was the
 * source of the inversion / freeze / lifetime drift. A cue must never run its own
 * trace (single-player BP_Laser pattern); it plays what gameplay hands it.
 *
 * Per-frame: read UAFLBeamChannelComponent::GetBeamImpactPoint() (replicated, so
 * it is correct on the firing client, the host, AND simulated proxies) and set
 * User."Beam End". The imported beam systems have Absolute Beam End enabled, so
 * the endpoint is world-space and the component needs no rotation -- we only
 * anchor the START at the pawn's view location (a cheap read, not a trace).
 *
 * The beam system + colour come from FGameplayCueParameters::SourceObject
 * (IAFLLaserVisualProvider). The cue actor is pooled/recycled by the GC manager,
 * so OnRemove nulls every captured pointer for safe reuse.
 */
UCLASS()
class AFLVFX_API AAFLCueNotify_LaserBeam : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AAFLCueNotify_LaserBeam();

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	/** The spawned beam component we drive. Nulled in OnRemove (pool-safe). */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BeamNC = nullptr;

	/**
	 * The firing pawn's beam-channel bridge -- the component implementing
	 * IAFLBeamEndpointProvider, captured from MyTarget in OnActive. Its replicated impact
	 * point is the ONLY endpoint source; the cue never traces. Typed as UActorComponent so
	 * AFLVFX takes no concrete-type dependency on AFLCombat -- we find it by interface and
	 * read it through the interface thunk. Weak: the pawn may die mid-beam.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UActorComponent> BeamChannel;

	/** The pawn whose view anchors the beam START fallback (a cheap view read; NOT a trace). */
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> FiringPawn;

	/** The weapon (IAFLLaserVisualProvider) we read look/colour from. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> VisualProvider;

	/** Niagara user param names — match the marketplace systems (User.Beam End / User.Color). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName BeamEndParam = TEXT("Beam End");

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName ColorParam = TEXT("Color");

	/**
	 * Distance down the view forward at which the beam's visible START is anchored. Keeps
	 * the start just ahead of the camera so the beam reads as leaving the weapon, not the
	 * lens. The END is the published impact -- never a hardcoded length. Point-blank clamp:
	 * if the impact is nearer than this, the start pulls back so the beam never reverses.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BeamVisualOriginDistance = 80.0f;
};
