// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AFLDismemberedHead.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;

/**
 * S4-05b: a detached head physics prop. Placeholder sphere (engine
 * BasicShapes/Sphere scaled to head size) that simulates physics and
 * auto-destroys after 5s. Spawned by UAFLDismemberComponent at the
 * victim's head transform on a head-overkill, after the head bone is
 * hidden (S4-05a). Real head mesh is later polish.
 *
 * AFL-0404 (deferred audio, now landed): on BeginPlay the head plays its
 * RollSound (Head_Roll_*) attached to the simulating sphere, so the rolling
 * audio follows the head as it tumbles. The electrical-POP at detach is a
 * separate one-shot GameplayCue fired by UAFLDismemberComponent (replicated).
 */
UCLASS()
class AFLDISMEMBER_API AAFLDismemberedHead : public AActor
{
	GENERATED_BODY()

public:
	AAFLDismemberedHead();

	/** The simulating sphere -- b-ii applies the pop impulse to this. */
	UStaticMeshComponent* GetSphereMesh() const { return SphereMesh; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AFL|Dismember", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> SphereMesh;

	/** AFL-0404: the rolling-head audio (Head_Roll_*), played attached to the sphere on
	 *  BeginPlay so it follows the tumble. Assigned on the BP child / by the asset; the
	 *  ctor attempts a default load of Head_Roll_1 so the slice ships without BP wiring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Dismember", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USoundBase> RollSound;
};
