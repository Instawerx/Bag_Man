// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AFLDismemberedPart.generated.h"

class UStaticMeshComponent;

/**
 * S4-INC1: a generic detached body-part physics prop -- the decoupled-prop base
 * EXTRACTED from the proven AAFLDismemberedHead. A UStaticMeshComponent root that
 * simulates physics, auto-destroys after a lifespan, and replicates (snapshot).
 * Spawned by UAFLDismemberComponent::SeverZone at the severed-bone transform; the
 * pop impulse is applied via ApplyPopImpulse.
 *
 * Limbs (LeftArm/RightArm/LeftLeg/RightLeg/Torso) use this base directly with a
 * placeholder mesh set on the BP child / asset in PHASE B. The head subclass
 * (AAFLDismemberedHead) adds only the sphere-mesh default + the roll audio.
 *
 * The shared logic lives here so every zone gets the same proven physics/replication
 * shape; only the cosmetic (mesh + audio) differs per zone.
 */
UCLASS()
class AFLDISMEMBER_API AAFLDismemberedPart : public AActor
{
	GENERATED_BODY()

public:
	AAFLDismemberedPart();

	/** The simulating mesh -- UAFLDismemberComponent applies the pop impulse to this. */
	UStaticMeshComponent* GetPartMesh() const { return PartMesh; }

	/**
	 * Apply the randomized pop impulse (bVelChange=false, NAME_None) to the part mesh.
	 * Called once by UAFLDismemberComponent right after spawn. Centralizes the proven
	 * head pop so every zone pops identically. VIRTUAL: the head/limb subclasses override to
	 * pop with bVelChange=true (velocity). PRESENTATION PASS: the optional Angular impulse
	 * adds an end-over-end TUMBLE (a linear-only pop through the COM is zero torque -> a slide).
	 */
	virtual void ApplyPopImpulse(const FVector& Linear, const FVector& Angular = FVector::ZeroVector);

protected:
	/** The decoupled physics prop mesh. Default mesh/material set by subclass or BP child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PartMesh;
};
