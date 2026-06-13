// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Camera/LyraCameraMode.h"

#include "AFLCameraMode_Decapitated.generated.h"

/**
 * S4-INC3 PHASE B-1: the decapitation camera mode.
 *
 * A survivable head-loss reframing -- the player keeps playing after the head pops, so
 * the camera does NOT death-cam: it just NUDGES the existing third-person framing to sell
 * "you lost your head." FOV/angle ONLY (the locked model). Ragdoll-loosening / wobble /
 * post-process is a DEFERRED polish slice (AFL-0410) and is deliberately NOT here.
 *
 * It derives the pivot from the base ULyraCameraMode (GetPivotLocation/GetPivotRotation =
 * the target actor + its control rotation), so it follows a roaming pawn correctly -- it is
 * NOT a fixed-arena camera like ULyraCameraMode_TopDownArenaCamera (which it is modeled on
 * structurally). On top of that pivot it applies three data-driven offsets:
 *   - PitchOffset   : tilt the view down a few degrees (look at the headless body).
 *   - PivotZOffset  : drop the eye height ~a head's worth (the viewpoint sits lower).
 *   - FieldOfView   : a slightly wider FOV (the EditDefaults knob on the base, set in the BP child).
 *
 * Abstract + Blueprintable (same override the TopDownArena mode uses over the NotBlueprintable
 * base) so the shipped mode is a BP child (CM_AFL_Decapitated) with the knobs tuned as DATA --
 * Lyra-canonical, no magic numbers baked in C++.
 *
 * PUSH (PHASE B-2, not wired here): a GameplayAbility with ActivationOwnedTags=State.Decapitated
 * calls SetCameraMode(this) on activation (ability-priority in ULyraHeroComponent::DetermineCameraMode).
 * When RestoreZone(Head) clears State.Decapitated the ability ends -> ClearCameraMode -> the
 * default third-person mode blends back. So this class never references the tag itself; the tag
 * drives the ability, the ability drives this mode.
 */
UCLASS(Abstract, Blueprintable)
class AFLDISMEMBER_API UAFLCameraMode_Decapitated : public ULyraCameraMode
{
	GENERATED_BODY()

public:
	UAFLCameraMode_Decapitated();

protected:
	//~ULyraCameraMode interface
	virtual void UpdateView(float DeltaTime) override;
	//~End of ULyraCameraMode interface

	/** Downward pitch added to the pivot rotation (degrees). Positive = look further down. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Decapitated")
	float PitchOffset = 8.0f;

	/** Vertical drop applied to the pivot location (cm). Negative = lower the viewpoint (~head height). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Decapitated")
	float PivotZOffset = -25.0f;
};
