// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberedHead.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberedHead)

AAFLDismemberedHead::AAFLDismemberedHead()
{
	// The base AAFLDismemberedPart already built PartMesh (root) + set the physics
	// shape (PhysicsActor profile, SimulatePhysics), the 5s lifespan, and replication.
	// The head only sets the head-specific COSMETIC on that inherited mesh.
	UStaticMeshComponent* Mesh = GetPartMesh();

	// trap #2: static + Succeeded() + full /Package.Object path
	// (mirrors AAFLLagTestDummy.cpp's MannyFinder pattern).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded() && Mesh)
	{
		Mesh->SetStaticMesh(SphereFinder.Object);
	}

	// Default engine shape material so the prop renders shaded rather than
	// default-grey unmeshed. Real head mesh + material is later polish.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded() && Mesh)
	{
		Mesh->SetMaterial(0, MatFinder.Object);
	}

	if (Mesh)
	{
		Mesh->SetRelativeScale3D(FVector(0.25f));   // 100cm engine sphere -> ~25cm head
	}

	// AFL-0404: default-load the rolling-head audio so the slice ships without BP wiring
	// (BP child / asset may override RollSound). Same static-FObjectFinder + Succeeded()
	// pattern. The imported USoundWave lives at /Game/Effects/Audio_Sound_Effects/Head_Roll_1.
	static ConstructorHelpers::FObjectFinder<USoundBase> RollSoundFinder(
		TEXT("/Game/Effects/Audio_Sound_Effects/Head_Roll_1.Head_Roll_1"));
	if (RollSoundFinder.Succeeded())
	{
		RollSound = RollSoundFinder.Object;
	}
}

void AAFLDismemberedHead::BeginPlay()
{
	Super::BeginPlay();

	// AFL-0404: play the rolling-head audio ATTACHED to the simulating mesh, so the
	// sound follows the head as it tumbles (vs a fixed-location one-shot). SpawnSoundAttached
	// auto-cleans when the actor is destroyed at InitialLifeSpan. The electrical-POP at
	// detach is the separate replicated GameplayCue fired by UAFLDismemberComponent.
	if (RollSound)
	{
		if (UStaticMeshComponent* Mesh = GetPartMesh())
		{
			UGameplayStatics::SpawnSoundAttached(RollSound, Mesh);
		}
	}
}
