// Copyright C12 AI Gaming. All Rights Reserved.

#include "HUD/AFLCameraShake_HitConfirm.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCameraShake_HitConfirm)


UAFLCameraShake_HitConfirm::UAFLCameraShake_HitConfirm(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Single-instance: a second hit-confirm before the first finishes restarts
	// the shake instead of stacking — keeps the camera quiet on rapid-fire.
	bSingleInstance = true;
}
