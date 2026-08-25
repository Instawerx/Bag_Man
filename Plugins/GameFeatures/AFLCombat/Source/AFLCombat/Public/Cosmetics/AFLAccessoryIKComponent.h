// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AFLAccessoryIKComponent.generated.h"

class ACharacter;
class USkeletalMeshComponent;

/**
 * Per-slot offset from the authored socket to the VISIBLE surface, in world space.
 *
 * Zero means "already on the surface", NOT "no answer". A probe that fails to find the shell reports
 * bValid=false for that slot and leaves the offset at zero, because a silently-zero offset and a
 * measured-zero offset are the state pair this programme has been burned by repeatedly -- an
 * unpopulated field once read as a rendering defect that did not exist.
 */
USTRUCT(BlueprintType)
struct FAFLAccessoryIKTargets
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") FVector LeftWristOffset  = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") FVector RightWristOffset = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") FVector NeckOffset       = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") FVector PendantOffset    = FVector::ZeroVector;

	/** Did the probe actually reach the visible shell for this slot? */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") bool bLeftWristValid  = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") bool bRightWristValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") bool bNeckValid       = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") bool bPendantValid    = false;
};

/**
 * DYNAMIC ACCESSORY FIT, because the body is modular and a constant cannot fit a variable.
 *
 * The sockets live on the shared SK_Mannequin; the silhouette is whatever body part is equipped. A
 * hardcoded offset or scale is therefore correct for exactly one body and wrong for every other one --
 * which is what a static-offset attempt demonstrated: the pieces either sat inside the armour shell or
 * floated clear of the arm, and nothing in between.
 *
 * This probes the EQUIPPED surface each evaluation and publishes a per-slot offset. Consumers are
 * deliberately two, because the pieces are not one kind of thing (measured, all ten):
 *
 *   SKELETAL  2 chains (5 bones), 2 bracelets (4 bones) -> AnimGraph, Transform (Modify) Bone
 *   STATIC    2 watches, 4 pendants (no skeleton at all) -> mesh component SetRelativeLocation
 *
 * The static half cannot take an AnimBP: those assets have no bones to modify. And the component write
 * must target the MESH, never the actor root -- the engine snaps a child actor's root to its parent
 * component on the frame after BeginPlay, which is the measured reason ApplyWristCorrection writes the
 * mesh.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLAccessoryIKComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLAccessoryIKComponent();

	/**
	 * How far outside the socket to begin the inward probe.
	 *
	 * MEASURED BURIAL DEPTHS, against CharacterMesh0 with the capsule excluded: neck 11.22 cm,
	 * pendant 10.38 cm, wrists 3.00 / 3.85 cm. The probe must START OUTSIDE the shell or it begins
	 * inside the geometry it is trying to find, so this is comfortably above the deepest observed
	 * case rather than tuned to it.
	 */
	UPROPERTY(EditAnywhere, Category = "AFL|AccessoryIK")
	float TraceStartDistance = 45.0f;

	/** Published offsets. Read by the accessory AnimBPs and by the static-piece applier. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK")
	FAFLAccessoryIKTargets CurrentOffsets;

	/** Re-probe every slot against the currently equipped surfaces. */
	UFUNCTION(BlueprintCallable, Category = "AFL|AccessoryIK")
	void EvaluateAccessoryOffsets(ACharacter* OwnerChar);

	/** Offset for one accessory socket, or zero with bOutValid=false if the shell was not reached. */
	UFUNCTION(BlueprintCallable, Category = "AFL|AccessoryIK")
	FVector GetOffsetForSocket(FName SocketName, bool& bOutValid) const;

private:
	/**
	 * Probe from outside the shell back to the socket and return the surface delta.
	 *
	 * OCCLUSION-SAFE, and every clause here is a defect that was measured rather than imagined:
	 *   - the pawn's own mesh is IGNORED. SKM_Manny_Invis carries collision and draws nothing; traces
	 *     aimed at the sockets stopped on it and reported a boundary that is never rendered.
	 *   - the capsule is IGNORED, for the same reason -- neck and pendant probes stopped on
	 *     CollisionCylinder, which produced an occlusion reading that proved nothing.
	 *   - the hit must be ON SearchSurfaceMesh. Anything else is the wrong surface, and returning it
	 *     would pin the piece to scenery or to another character.
	 *   - complex collision, because a physics-asset approximation of an armoured shell is not the
	 *     silhouette the eye judges.
	 */
	FVector ProbeSurface(const USkeletalMeshComponent* SocketSourceMesh,
	                     const USkeletalMeshComponent* SearchSurfaceMesh,
	                     FName SocketName,
	                     bool& bOutValid) const;
};
