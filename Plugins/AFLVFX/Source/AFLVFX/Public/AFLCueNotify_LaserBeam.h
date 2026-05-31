// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "AFLCueNotify_LaserBeam.generated.h"

class UNiagaraComponent;

/**
 * Looping cue for a continuous beam (GameplayCue.Weapon.Laser.Beam).
 *
 * Spawns the weapon's beam Niagara at the muzzle and, each frame, sets the Niagara
 * User.Beam End param to a COSMETIC-only local trace's impact point. The authoritative
 * trace + damage live in the GameplayAbility; this never applies a GameplayEffect.
 *
 * Everything it needs comes from FGameplayCueParameters::SourceObject implementing
 * IAFLLaserVisualProvider.
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
};
