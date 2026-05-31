// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AFLLaserVisualData.generated.h"

class UNiagaraSystem;

/**
 * UAFLLaserVisualData
 *
 * The canonical, per-weapon visual backend for the AFL laser/beam system. A weapon
 * instance implements IAFLLaserVisualProvider by FORWARDING each getter to its assigned
 * UAFLLaserVisualData. Adding a new laser weapon's look is then pure data: author one
 * DA_AFL_LaserVisual_<Weapon> and assign it -- no C++ per weapon, no per-weapon Blueprint
 * graph surgery. (See afl-laser-beam-system: integration-architecture.md / the cue contract.)
 *
 * Look layering:
 *   - BASE look = the imported NS_AFL_* system's own User-param defaults (authored in the
 *     Niagara editor -- the rich color/intensity surface). This is what shows by default.
 *   - RUNTIME override (skins / entitlements) = BeamColorOverride with A > 0. The cues only
 *     push it to the Niagara when A > 0; A == 0 (the default) means "leave the editor
 *     default alone." This A-channel sentinel is the marketplace/skin hook -- a skin is a
 *     runtime color (and later a swapped BeamSystem) through the same provider.
 */
UCLASS(BlueprintType)
class AFLVFX_API UAFLLaserVisualData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Beam Niagara system (imported NS_AFL_Laser_* / NS_AFL_OrbLaser_*). The visible beam. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Laser")
	TObjectPtr<UNiagaraSystem> BeamSystem = nullptr;

	/** Optional muzzle/charge orb system (NS_AFL_OrbLaser_Center_*), or null for straight beams. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Laser")
	TObjectPtr<UNiagaraSystem> MuzzleSystem = nullptr;

	/**
	 * RUNTIME color override (skin / entitlement). Default A == 0 means "no override --
	 * use the NS editor default." A designer sets A > 0 ONLY to force a skin/entitlement
	 * tint; the cues gate on A > 0 before driving User.Color.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Laser", meta = (HDR = true))
	FLinearColor BeamColorOverride = FLinearColor(0.f, 0.f, 0.f, 0.f);

	/** Socket on the weapon mesh a muzzle-attached cue emits from (default "Muzzle"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Laser")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Cosmetic max trace range, cm (NOT the damage range). Matches the verified beam default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Laser", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float CosmeticRange = 8000.f;

	/** Distance down the aim ray the beam's visible start is anchored, cm (verified default 80). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Laser", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BeamVisualOriginDistance = 80.f;
};
