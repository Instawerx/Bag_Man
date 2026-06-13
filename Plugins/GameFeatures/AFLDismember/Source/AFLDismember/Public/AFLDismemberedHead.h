// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLDismemberedPart.h"

#include "AFLDismemberedHead.generated.h"

class USoundBase;

/**
 * S4-05b / S4-INC1: the detached HEAD prop -- now a thin subclass of the extracted
 * AAFLDismemberedPart base. Inherits the proven decoupled-physics-prop shape (mesh
 * root, SimulatePhysics, 5s lifespan, replication, ApplyPopImpulse) and adds ONLY
 * the head-specific cosmetic: the engine-sphere default mesh + the rolling-head audio.
 *
 * The base AAFLDismemberedPart::PartMesh IS the sphere; the head ctor sets the sphere
 * mesh + BasicShapeMaterial + 0.25 scale on it. Limbs use the base directly with a
 * placeholder mesh (PHASE B), so the only head-specific code left here is the sphere
 * default + RollSound. Regression: the head must still pop + play roll audio exactly
 * as before this refactor.
 *
 * AFL-0404 (audio, landed): on BeginPlay the head plays its RollSound (Head_Roll_1)
 * attached to the simulating mesh, so the roll audio follows the tumble. The
 * electrical-POP at detach is a separate replicated GameplayCue fired by
 * UAFLDismemberComponent.
 */
UCLASS()
class AFLDISMEMBER_API AAFLDismemberedHead : public AAFLDismemberedPart
{
	GENERATED_BODY()

public:
	AAFLDismemberedHead();

	/** Back-compat accessor (callers used GetSphereMesh()); forwards to the base PartMesh. */
	UStaticMeshComponent* GetSphereMesh() const { return GetPartMesh(); }

protected:
	virtual void BeginPlay() override;

private:
	/** AFL-0404: the rolling-head audio (Head_Roll_*), played attached to the mesh on
	 *  BeginPlay so it follows the tumble. Assigned on the BP child / by the asset; the
	 *  ctor attempts a default load of Head_Roll_1 so the slice ships without BP wiring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> RollSound;
};
