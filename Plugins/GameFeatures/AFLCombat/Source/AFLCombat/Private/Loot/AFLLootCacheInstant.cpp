// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootCacheInstant.h"

#include "AFLCombat.h"
#include "Components/StaticMeshComponent.h"
#include "Loot/AFLLootGrantComponent.h"
#include "Loot/AFLOverlapCollectComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootCacheInstant)

AAFLLootCacheInstant::AAFLLootCacheInstant()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// The overlap substrate IS the collect volume + root (magnet OFF by default -> pure walk-over).
	Overlap = CreateDefaultSubobject<UAFLOverlapCollectComponent>(TEXT("Overlap"));
	SetRootComponent(Overlap);

	// Cosmetic body so a placed cache is watchable (engine cube; restyle per placement).
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Overlap);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetRelativeScale3D(FVector(0.5f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(CubeMesh.Object);
	}

	LootGrant = CreateDefaultSubobject<UAFLLootGrantComponent>(TEXT("LootGrant"));
}

void AAFLLootCacheInstant::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (LootGrant)
	{
		// Loot-Carry Phase B: INSTANT walk-over now feeds the carried-at-risk POOL (CarryToExtractEnergy),
		// not instant-bank Watts. LootWatts is the value added to the pool; it banks to Watts only on extract.
		LootGrant->Configure(EAFLLootValueModel::CarryToExtractEnergy, LootWatts, Eligibility, /*OwnerActor=*/nullptr, TEXT("cache-instant"));
	}
	if (Overlap)
	{
		Overlap->OnCollected.AddDynamic(this, &AAFLLootCacheInstant::HandleCollected);
	}
}

void AAFLLootCacheInstant::HandleCollected(AActor* Collector)
{
	// Server-auth (the substrate only fires OnCollected on authority). Grant the known Watts + consume.
	if (LootGrant)
	{
		LootGrant->TryGrant(Collector);
	}
	UE_LOG(LogAFLCombat, Display, TEXT("[AFLLoot] INSTANT cache %s collected by %s -> grant + consume"),
		*GetName(), *GetNameSafe(Collector));
	Destroy();
}
