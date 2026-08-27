// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "AFLAccessoryPartActor.generated.h"

/**
 * CC-8: the base every jewellery part Blueprint derives from, and the ONLY place the wrist handedness
 * correction lives.
 *
 * WHY A CORRECTION IS NEEDED AT ALL. accessory_wrist_l and accessory_wrist_r inherit their bone frames,
 * and those frames are MIRRORED -- measured on SK_Mannequin:
 *
 *     accessory_wrist_l : local +Y -> component (+0.65, +0.26, +0.72)   points UP
 *     accessory_wrist_r : local +Y -> component (+0.65, -0.26, -0.72)   points DOWN
 *
 * A mesh carries one orientation. Any rotation baked into it rotates both wrists identically, so a
 * correction that puts a watch face up on the left drives it further down on the right. That is
 * arithmetic, not a matter of taste, and it is why the fix cannot live in the conform.
 *
 * IT DOES NOT LIVE ON THE SOCKET EITHER. accessory_wrist_l/_r are on SK_Mannequin and shared by every
 * accessory and every wrist item added later; rotating them to suit watches moves bracelets and
 * everything after.
 *
 * SO IT LIVES HERE, at attach time, on the one side that needs it. One mesh per piece,
 * bWristEitherSide stays TRUE, and no shared content is touched.
 */
UCLASS(Abstract)
class AFLCOMBAT_API AAFLAccessoryPartActor : public AActor
{
	GENERATED_BODY()

public:
	AAFLAccessoryPartActor();

	/** The socket this part was attached to, or NAME_None. Read from the ChildActorComponent that spawned
	 *  us -- the customizer attaches THAT at the socket, and we are its child actor. */
	UFUNCTION(BlueprintPure, Category = "AFL|Accessory")
	FName GetAttachedSocketName() const;

	/**
	 * Re-read the socket and apply (or clear) the correction. Public and idempotent because the actor can
	 * arrive at its socket two ways: the customizer attaches a ChildActorComponent and spawns us inside
	 * it, but a direct SpawnActor + AttachToComponent has no ChildActorComponent at all -- and a test that
	 * attaches directly would otherwise measure an actor that never corrected, and fail a working build.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Accessory")
	void ApplyWristCorrection();

	/**
	 * STATIC PIECES ONLY. Watches and pendants have no skeleton -- measured, all six -- so they cannot
	 * take a Transform (Modify) Bone node and must be moved at the component level instead.
	 * Pulls the offset for this actor's own socket from the pawn's UAFLAccessoryIKComponent.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|AccessoryIK")
	void ApplyIKOffset();

private:
	/** The mesh's authored relative location, captured once, so the IK offset is applied to the
	 *  AUTHORED pose every time rather than accumulating on top of the previous frame's answer. */
	FVector AuthoredMeshRelativeLocation = FVector::ZeroVector;

	/** True while an IK write is waiting for the piece's attach into the pawn hierarchy to land. */
	bool bAwaitingAttach = false;
	FTimerHandle AttachRetryHandle;
	bool bAuthoredMeshLocationCaptured = false;

public:

	/** True once the correction has been applied. Lets a test assert the mechanism ran rather than
	 *  inferring it from a transform that might look right for another reason. */
	UFUNCTION(BlueprintPure, Category = "AFL|Accessory")
	bool WasWristCorrected() const { return bWristCorrected; }

	/**
	 * Which way is UP for the visible part.
	 *
	 * NOT GetActorUpVector(). The correction can no longer live on the actor root: for a child actor
	 * the engine snaps that root to its ChildActorComponent immediately after BeginPlay
	 * (ChildActorComponent.cpp CreateChildActor, SnapToTargetNotIncludingScale), so the root's relative
	 * rotation is always identity by the time anything renders and the actor's up vector answers a
	 * question about the socket rather than about the part.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Accessory")
	FVector GetPartUpVector() const;

protected:
	virtual void BeginPlay() override;

	/**
	 * Applied to this actor's root when attached at the RIGHT wrist, and nowhere else.
	 *
	 * A 180 degree ROLL is a rotation about the socket's local X, which is the bone's forward axis --
	 * i.e. the forearm. Rolling about the arm maps local +Y to -Y, which is exactly the difference
	 * between the two mirrored frames, so a face authored toward +Y ends up world-up on both wrists.
	 *
	 * Exposed rather than hard-coded so a piece whose art is authored on a different axis can correct
	 * without a code change -- but the default is the measured answer, not a guess.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Accessory")
	FRotator RightWristCorrection = FRotator(0.0f, 0.0f, 180.0f);

	/**
	 * Applied at BOTH wrists, before the per-side correction. This is the -90 the operator ruled, and it
	 * exists because the socket's own up-ish axis is +Y, not +Z:
	 *
	 *     accessory_wrist_l : local +Y -> z = +0.719   (the up-ish one)
	 *                         local +Z -> z = -0.284   (points slightly DOWN)
	 *
	 * A part inherits the socket frame, so its own +Z lands on the socket's +Z and the face reads
	 * downward. MEASURED TWICE, not assumed. With only the per-side correction the assert reported both
	 * wrists agreeing at -0.284 -- unified, and unified the wrong way. Rolling -90 then reported -0.719:
	 * the right MAGNITUDE, so the axis was correct, but the wrong SIGN -- it landed on -Y. +90 is the
	 * measured answer. Two runs, and the assert named the error each time rather than leaving it to an
	 * eye that would have called -0.284 "nearly flat" and moved on.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Accessory")
	FRotator BaseWristOrientation = FRotator(0.0f, 0.0f, 90.0f);

	/** The socket the correction applies to. Named rather than inferred: "the right one" is a fact about
	 *  content, and a rename that silently stopped correcting would be invisible. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Accessory")
	FName RightWristSocket = FName(TEXT("accessory_wrist_r"));

	/** The other wrist. Both are named so "is this a wrist?" is a lookup rather than a prefix guess. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Accessory")
	FName LeftWristSocket = FName(TEXT("accessory_wrist_l"));

	/**
	 * Applied to a part attached at the NECK, and the reason a chain hangs at all.
	 *
	 * MEASURED, on a live pawn, not assumed: at accessory_neck the socket's own axes read
	 *
	 *     forward (+X) -> world z = +0.998     i.e. +X points STRAIGHT UP
	 *     up      (+Z) -> world z =  0.000     i.e. +Z is HORIZONTAL
	 *
	 * because the socket inherits spine_03, and a UE spine bone runs +X up the spine. A chain authored
	 * to hang along its own -Z therefore inherits a frame in which -Z is sideways, and it sticks out of
	 * the chest instead of lying on it. Measured before this correction existed: the bone chain spans
	 * 18.75cm and descended 1.08cm -- about 3 degrees off horizontal.
	 *
	 * Pitch -90 maps the part's +Z onto the socket's +X, so the part's -Z lands on the socket's -X,
	 * which is down. Same discipline as the wrist: a measured default, exposed so a piece authored on
	 * another axis can differ without a code change.
	 *
	 * IT IS NOT GRAVITY. AnimDynamics was the other candidate and the socket axes rule it out -- the
	 * rest pose is already wrong before any simulation runs.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Accessory")
	FRotator BaseNeckOrientation = FRotator(-90.0f, 0.0f, 0.0f);

	/** The neck socket this correction applies to. Named, not inferred. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Accessory")
	FName NeckSocket = FName(TEXT("accessory_neck"));

private:
	UPROPERTY(Transient)
	bool bWristCorrected = false;
};
