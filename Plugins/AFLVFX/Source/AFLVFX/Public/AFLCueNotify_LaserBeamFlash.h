// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "AFLCueNotify_LaserBeamFlash.generated.h"

class UNiagaraComponent;

/**
 * Burst cue for a hitscan beam flash (GameplayCue.Weapon.Laser.BeamFlash).
 *
 * Pulse/hitscan weapons have no looping beam -- just a one-frame-ish flash from the
 * muzzle to the confirmed impact. On execute: spawn the weapon's beam Niagara at the
 * muzzle, set User.Beam End = Params.Location (the predicted/confirmed impact), set
 * User.Color from the weapon, then auto-destroy after FlashLifetime (~0.08s).
 *
 * Everything read from cue params / IAFLLaserVisualProvider (off SourceObject) at
 * execute time -- no tick. The looping sibling is AAFLCueNotify_LaserBeam; this is its
 * discrete-shot cousin.
 *
 * DISCOVERY: parent class only. Firing requires a tagged GCN_AFL_*.uasset parented to
 * it under a scanned GameplayCueNotifyPaths folder (/Game/GameplayCues/).
 */
UCLASS()
class AFLVFX_API AAFLCueNotify_LaserBeamFlash : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AAFLCueNotify_LaserBeamFlash();

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	/** Seconds the flash lives before it deactivates + auto-destroys. ~0.06-0.10s reads as a flash. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser", meta = (ClampMin = "0.02", UIMin = "0.02"))
	float FlashLifetime = 0.08f;

	/** Niagara user-param names -- match the marketplace systems (User.Beam End / User.Color). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName BeamEndParam = TEXT("Beam End");

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Laser")
	FName ColorParam = TEXT("Color");

private:
	/** Timer-driven teardown of a spawned flash component. */
	void DeactivateFlash(UNiagaraComponent* NC);
};
