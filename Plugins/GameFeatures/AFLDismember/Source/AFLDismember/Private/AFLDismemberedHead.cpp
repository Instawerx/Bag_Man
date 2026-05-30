// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberedHead.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
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

	// Default engine shape material so the prop renders shaded rather than
	// default-grey unmeshed. Real head mesh + material is later polish.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MatFinder.Succeeded())
	{
		SphereMesh->SetMaterial(0, MatFinder.Object);
	}

	SphereMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	SphereMesh->SetSimulatePhysics(true);
	SphereMesh->SetRelativeScale3D(FVector(0.25f));   // 100cm engine sphere -> ~25cm head

	InitialLifeSpan = 5.0f;            // native auto-destroy

	// Pre-init direct-set on bReplicates (protected) -- matches engine ctor
	// convention (SkeletalMeshActor / DefaultPawn / GameStateBase /
	// LevelScriptActor) and clears the "SetReplicates called on
	// non-initialized actor" warning. bReplicateMovement is PRIVATE (it has
	// a RepNotify), so its setter is the only access path; SetReplicateMovement
	// does NOT emit the non-initialized-actor warning even in a ctor.
	bReplicates = true;                 // production-ready; harmless in single-PIE
	SetReplicateMovement(true);
	bAlwaysRelevant = true;
}
