// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootCarryPickup.h"

#include "AFLCombat.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"               // C1: the per-spawn form mesh (SetStaticMesh override)
#include "Loot/AFLLootCarryComponent.h"
#include "Loot/AFLOverlapCollectComponent.h"
#include "Net/UnrealNetwork.h"               // C1: DOREPLIFETIME(OverrideMesh) -- the scattered form replicates
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

void AAFLLootCarryPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAFLLootCarryPickup, OverrideMesh);
}

void AAFLLootCarryPickup::SetVisualMesh(UStaticMesh* InMesh)
{
	// Authority sets the replicated form mesh + applies locally now (OnRep won't fire on the server); clients
	// apply it from OnRep_VisualMesh. The scattered limb gib then reads correctly on every machine.
	if (!HasAuthority())
	{
		return;
	}
	OverrideMesh = InMesh;
	ApplyVisualMesh();
}

void AAFLLootCarryPickup::OnRep_VisualMesh()
{
	ApplyVisualMesh();
}

void AAFLLootCarryPickup::ApplyVisualMesh()
{
	if (OverrideMesh && VisualMesh)
	{
		VisualMesh->SetStaticMesh(OverrideMesh);
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
