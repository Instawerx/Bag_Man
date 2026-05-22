// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Camera/CameraShakeBase.h"

#include "AFLCameraShake_HitConfirm.generated.h"


/**
 * UAFLCameraShake_HitConfirm
 *
 * Camera shake played on the FIRING client when UAFLDamageExecCalc confirms
 * the shot landed for non-zero effective damage. Triggered by
 * UAFLHitConfirmComponent via PlayerController::ClientStartCameraShake.
 *
 * Intentionally a thin C++ shell — the actual amplitude/duration profile is
 * authored by Design on a BP child (BP_AFL_CameraShake_HitConfirm) in S5. The
 * task brief calls for ~80ms / small amplitude; those numbers belong on the
 * BP CDO, not in code. Single-instance scaling is sufficient for the hitscan
 * pulse feedback (we never need to stack two concurrent confirms).
 */
UCLASS(meta=(BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLCameraShake_HitConfirm : public UCameraShakeBase
{
	GENERATED_BODY()

public:

	UAFLCameraShake_HitConfirm(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
