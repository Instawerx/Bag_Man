// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberedPart.h"

#include "Components/StaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberedPart)

AAFLDismemberedPart::AAFLDismemberedPart()
{
	PrimaryActorTick.bCanEverTick = false;

	PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PartMesh"));
	RootComponent = PartMesh;

	// Decoupled physics prop: simulate + collide as a physics actor. The concrete
	// mesh/material/scale is set by the subclass ctor (head = sphere) or the BP child
	// (limbs = placeholder), so the base sets only the physics shape, not the visual.
	PartMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	PartMesh->SetSimulatePhysics(true);

	InitialLifeSpan = 5.0f;            // native auto-destroy

	// Pre-init direct-set on bReplicates (protected) -- matches engine ctor convention
	// (SkeletalMeshActor / DefaultPawn / GameStateBase / LevelScriptActor) and clears the
	// "SetReplicates called on non-initialized actor" warning. bReplicateMovement is PRIVATE
	// (it has a RepNotify), so its setter is the only access path; SetReplicateMovement does
	// NOT emit the non-initialized-actor warning even in a ctor. Snapshot replication is
	// cosmetically adequate (deterministic physics replication is AFL-0408, deferred).
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;
}

void AAFLDismemberedPart::ApplyPopImpulse(const FVector& Impulse)
{
	if (PartMesh)
	{
		PartMesh->AddImpulse(Impulse, NAME_None, /*bVelChange=*/false);
	}
}
