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

	/** True once the correction has been applied. Lets a test assert the mechanism ran rather than
	 *  inferring it from a transform that might look right for another reason. */
	UFUNCTION(BlueprintPure, Category = "AFL|Accessory")
	bool WasWristCorrected() const { return bWristCorrected; }

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

private:
	UPROPERTY(Transient)
	bool bWristCorrected = false;
};
