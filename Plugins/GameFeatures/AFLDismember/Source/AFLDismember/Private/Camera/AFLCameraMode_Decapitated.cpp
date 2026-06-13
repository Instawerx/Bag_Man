// Copyright C12 AI Gaming. All Rights Reserved.

#include "Camera/AFLCameraMode_Decapitated.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCameraMode_Decapitated)

UAFLCameraMode_Decapitated::UAFLCameraMode_Decapitated()
{
	// A touch wider than the default to telegraph the state change; final value is a BP-child
	// knob. BlendTime stays the base 0.5s EaseOut so the reframe eases in (not a hard snap).
	FieldOfView = 90.0f;
}

void UAFLCameraMode_Decapitated::UpdateView(float DeltaTime)
{
	// Derive from the base pivot (target actor location + its control rotation) so we FOLLOW a
	// roaming pawn -- this is the difference from the fixed-arena TopDown mode we are modeled on.
	FVector PivotLocation = GetPivotLocation();
	FRotator PivotRotation = GetPivotRotation();

	// FOV/angle ONLY (locked model). Nudge the framing: tilt DOWN toward the headless body and
	// drop the viewpoint ~a head's height. Ragdoll-loosening / wobble = deferred (AFL-0410).
	// UE pitch convention: NEGATIVE = look down -> SUBTRACT the (positive) PitchOffset. (PIE B-2:
	// the first cut ADDED it and the camera looked UP -- the sign was inverted.)
	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch - PitchOffset, ViewPitchMin, ViewPitchMax);
	PivotLocation.Z += PivotZOffset;

	View.Location = PivotLocation;
	View.Rotation = PivotRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;
}
