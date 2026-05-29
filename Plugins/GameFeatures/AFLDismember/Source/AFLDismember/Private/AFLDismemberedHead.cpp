// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberedHead.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAFLDismemberedHead::AAFLDismemberedHead()
{
	PrimaryActorTick.bCanEverTick = false;

	SphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SphereMesh"));
	RootComponent = SphereMesh;

	// trap #2: static + Succeeded() + full /Package.Object path
	// (mirrors AAFLLagTestDummy.cpp's MannyFinder pattern).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		SphereMesh->SetStaticMesh(SphereFinder.Object);
	}

	SphereMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	SphereMesh->SetSimulatePhysics(true);
	SphereMesh->SetRelativeScale3D(FVector(0.25f));   // 100cm engine sphere -> ~25cm head

	InitialLifeSpan = 5.0f;            // native auto-destroy

	SetReplicates(true);               // production-ready; harmless in single-PIE
	SetReplicateMovement(true);
	bAlwaysRelevant = true;
}
