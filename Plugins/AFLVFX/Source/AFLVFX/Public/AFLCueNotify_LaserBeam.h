// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "AFLCueNotify_LaserBeam.generated.h"

class UNiagaraComponent;
class APawn;

/**
 * Looping cue for a continuous beam (GameplayCue.Weapon.Laser.Beam).
 *
 * Each frame: derive the firing pawn's AIM RAY (camera-based, same source as the
 * authoritative trace), do a COSMETIC-only trace along it, position the beam Niagara
 * at the aim-ray START (BeamVisualOriginDistance down the ray, point-blank-clamped),
 * and drive User.Beam End to the trace impact. The authoritative trace + damage live
 * in the GameplayAbility; this never applies a GameplayEffect.
 *
 * AIM-RAY, NOT MUZZLE: the start rides the camera ray (ported verbatim from the
 * PIE-verified UAFLAG_Laser_Beam) so the beam stays glued to the crosshair in 3rd
 * person -- a muzzle-anchored start skews it up/right. Imported beam systems stretch
 * from the component origin to User.Beam End, so the component's world position IS the
 * visible start; we set it to the aim-ray start each tick.
 *
 * Everything else comes from FGameplayCueParameters::SourceObject implementing
 * IAFLLaserVisualProvider (the beam system + color).
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
	/** The spawned beam component we drive. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BeamNC = nullptr;

	/**
	 * The firing pawn, captured from the cue params in OnActive. The cue ACTOR's own
	 * GetInstigator() is null (the GameplayCueManager doesn't set it), so the aim ray
	 * must come from this -- resolved from FGameplayCueParameters (Instigator / the
	 * EffectCauser / the cue Target), NOT GetInstigator(). This was the (0,0,0) spawn bug.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> FiringPawn;

	/** The weapon (IAFLLaserVisualProvider) we read look/socket/range from. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> VisualProvider;

	/** Niagara user param names — match the marketplace systems (User.Beam End / User.Color). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName BeamEndParam = TEXT("Beam End");

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName ColorParam = TEXT("Color");

	/** Channel for the cosmetic end-point trace (visibility, not the damage channel). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	TEnumAsByte<ECollisionChannel> CosmeticTraceChannel = ECC_Visibility;

	/**
	 * Distance down the AIM RAY (from the firing pawn's camera) at which the beam's
	 * visible START is positioned. Ported verbatim from the PIE-verified
	 * UAFLAG_Laser_Beam (BeamVisualOriginDistance, 80cm). Point-blank clamp: if the
	 * impact is nearer than this, the start pulls back to the midpoint so the beam never
	 * renders reversed. (Imported beam systems stretch from the component origin to
	 * User.Beam End, so "start" = where we place the component each tick.)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BeamVisualOriginDistance = 80.0f;

	/** Max cosmetic trace distance from the camera, cm. Matches the verified beam ability default. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float MaxRange = 8000.0f;

private:
	/**
	 * Resolve the firing pawn's view (origin + direction) for the aim ray -- verbatim
	 * the verified ability: instigator pawn view, overridden by PlayerCameraManager when
	 * available. False if no firing pawn (skip the tick update rather than aim from origin).
	 */
	bool ResolveAimRay(FVector& OutViewLocation, FVector& OutAimDirection) const;
};
