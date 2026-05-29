// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AFLDismemberedHead.generated.h"

class UStaticMeshComponent;

/**
 * S4-05b: a detached head physics prop. Placeholder sphere (engine
 * BasicShapes/Sphere scaled to head size) that simulates physics and
 * auto-destroys after 5s. Spawned by UAFLDismemberComponent at the
 * victim's head transform on a head-overkill, after the head bone is
 * hidden (S4-05a). Real head mesh is later polish.
 */
UCLASS()
class AFLDISMEMBER_API AAFLDismemberedHead : public AActor
{
	GENERATED_BODY()

public:
	AAFLDismemberedHead();

	/** The simulating sphere -- b-ii applies the pop impulse to this. */
	UStaticMeshComponent* GetSphereMesh() const { return SphereMesh; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AFL|Dismember", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> SphereMesh;
};
