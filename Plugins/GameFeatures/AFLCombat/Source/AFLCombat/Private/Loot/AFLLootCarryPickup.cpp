// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootCarryPickup.h"

#include "AFLCombat.h"
#include "Components/StaticMeshComponent.h"
#include "Loot/AFLLootCarryComponent.h"
#include "Loot/AFLOverlapCollectComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootCarryPickup)

AAFLLootCarryPickup::AAFLLootCarryPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// The overlap substrate IS the collect volume + root (magnet OFF by default -> pure walk-over).
	Overlap = CreateDefaultSubobject<UAFLOverlapCollectComponent>(TEXT("Overlap"));
	SetRootComponent(Overlap);

	// Cosmetic body so a placed / scattered pickup is watchable (engine cube).
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Overlap);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetRelativeScale3D(FVector(0.3f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void AAFLLootCarryPickup::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && Overlap)
	{
		Overlap->OnCollected.AddDynamic(this, &AAFLLootCarryPickup::HandleCollected);
	}
}

void AAFLLootCarryPickup::HandleCollected(AActor* Collector)
{
	// Server-auth (the substrate only fires OnCollected on authority). Add LootValue to the collector's
	// carried pool -- ANYONE can collect (incl. the original dropper): the risk/recovery loop closes here.
	if (Collector)
	{
		if (UAFLLootCarryComponent* Carry = Collector->FindComponentByClass<UAFLLootCarryComponent>())
		{
			Carry->Collect(LootValue);
		}
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: pickup %s (+%d) collected by %s"),
		*GetName(), LootValue, *GetNameSafe(Collector));
	Destroy();
}
