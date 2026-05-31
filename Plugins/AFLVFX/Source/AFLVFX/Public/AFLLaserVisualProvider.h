// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AFLLaserVisualProvider.generated.h"

class UNiagaraSystem;
class USceneComponent;

/**
 * A weapon implements this so the laser GameplayCues know what it looks like.
 * The cue reads it off FGameplayCueParameters::SourceObject. Look is DATA, not code:
 * back these with a DA_AFL_LaserVisual_* data asset on the weapon instance.
 *
 * BlueprintNativeEvent so either a C++ ULyraRangedWeaponInstance subclass OR a Blueprint
 * weapon can implement it. Call from the cue via the generated thunks, e.g.
 *   UNiagaraSystem* Sys = IAFLLaserVisualProvider::Execute_GetBeamSystem(Provider);
 *
 * (Supersedes the pure-virtual sketch in references/integration-architecture.md — this
 *  BlueprintNativeEvent form is the canonical one; it adds GetMuzzleMeshComponent so the
 *  cue can attach to the weapon mesh without guessing the equipment internals.)
 */
UINTERFACE(BlueprintType, MinimalAPI)
class UAFLLaserVisualProvider : public UInterface
{
	GENERATED_BODY()
};

class AFLVFX_API IAFLLaserVisualProvider
{
	GENERATED_BODY()

public:
	/** Beam Niagara system (e.g. NS_AFL_Laser_Twist). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Laser")
	UNiagaraSystem* GetBeamSystem() const;

	/** Optional muzzle/charge orb system (NS_AFL_OrbLaser_Center_*), or nullptr. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Laser")
	UNiagaraSystem* GetMuzzleSystem() const;

	/** Beam tint -> drives the Niagara User.Color param. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Laser")
	FLinearColor GetBeamColor() const;

	/** Socket on the weapon mesh the beam emits from (default "Muzzle"). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Laser")
	FName GetMuzzleSocketName() const;

	/** COSMETIC max range for the cue's local end-point trace. NOT the damage range. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Laser")
	float GetCosmeticRange() const;

	/** The weapon mesh component the muzzle socket lives on (cue attaches the beam here). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AFL|Laser")
	USceneComponent* GetMuzzleMeshComponent() const;
};
