// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "AFLPulseTuningData.generated.h"


/**
 * UAFLPulseTuningData
 *
 * AFL-0209: data-driven recoil + spread tuning for UAFLAG_Laser_Pulse. The
 * ability holds a TObjectPtr<UAFLPulseTuningData> on its CDO (DA_AFLPulseTuning
 * by default); designers can swap the asset at runtime via AFL.Combat.LoadTuning
 * to A/B test feel without touching the damage pipeline.
 *
 * No behavior change to damage: spread perturbs the trace direction (and the
 * replicated ClaimedAimDirection on FAFLAbilityTargetData_Hitscan), recoil is
 * owning-client-only via AddPitchInput/AddYawInput. The server never reads
 * these values directly; AFL-0211 lag-comp re-trace inherits the perturbed
 * direction through the target data.
 *
 * Hot-swap rule: the SECONDARY per-knob cheats (AFL.Combat.SetSpread /
 * AFL.Combat.SetRecoil) operate on a TRANSIENT DUPLICATE installed on the
 * ability instance, NOT on the source asset. The PRIMARY cheat
 * (AFL.Combat.LoadTuning) StaticLoadObject's a different asset and assigns
 * it directly; that's still a non-mutating swap because the loaded asset is
 * the source-of-truth for itself.
 */
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLPulseTuningData : public UDataAsset
{
	GENERATED_BODY()

public:

	// ─── Spread ───────────────────────────────────────────────────────────

	/** Cone half-angle at rest, degrees. The minimum spread the trace ever uses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Pulse Tuning|Spread", meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float BaseSpreadDegrees = 0.5f;

	/** Bloom cap. Sustained fire converges here, not above. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Pulse Tuning|Spread", meta=(ClampMin="0.0", UIMin="0.0", UIMax="30.0"))
	float MaxSpreadDegrees = 4.0f;

	/** Added to current spread on each shot (then clamped to Max). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Pulse Tuning|Spread", meta=(ClampMin="0.0", UIMin="0.0", UIMax="10.0"))
	float SpreadPerShotDegrees = 0.6f;

	/** Recovery rate toward Base when not firing, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Pulse Tuning|Spread", meta=(ClampMin="0.0", UIMin="0.0", UIMax="60.0"))
	float SpreadRecoveryDegPerSec = 8.0f;

	// ─── Recoil (owning-client-only, cosmetic) ────────────────────────────

	/** Degrees of pitch kick per shot. Applied as AddPitchInput(-RecoilPitchPerShot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Pulse Tuning|Recoil", meta=(ClampMin="0.0", UIMin="0.0", UIMax="5.0"))
	float RecoilPitchPerShot = 0.4f;

	/** Random horizontal yaw, ±this many degrees, per shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Pulse Tuning|Recoil", meta=(ClampMin="0.0", UIMin="0.0", UIMax="3.0"))
	float RecoilYawJitterDegrees = 0.15f;

	/**
	 * Reserved for downstream control-rotation interpolation toward the rest
	 * position. Not consumed by the ability today — the kick is a one-shot
	 * AddPitchInput / AddYawInput per fire and the input chain resolves
	 * recovery naturally. Kept on the DA so the designer-facing surface
	 * matches the brief and future passes (camera-driven recovery, mouse
	 * smoothing) can wire it without a schema change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Pulse Tuning|Recoil", meta=(ClampMin="0.0", UIMin="0.0", UIMax="60.0"))
	float RecoilRecoverySpeed = 6.0f;
};
