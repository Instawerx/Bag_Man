// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootCarryPickup.h"

#include "AFLCombat.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"               // C1: the per-spawn form mesh (SetStaticMesh override)
#include "Materials/MaterialInterface.h"      // PRESENTATION: the per-spawn gib material (SetMaterial on every slot)
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
	DOREPLIFETIME(AAFLLootCarryPickup, OverrideMaterial);
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

void AAFLLootCarryPickup::SetVisualMaterial(UMaterialInterface* InMaterial)
{
	// Authority sets the replicated form material + applies locally now (OnRep won't fire on the server); clients
	// apply from OnRep_VisualMaterial. The scattered gib then reads as the victim's skin on every machine.
	if (!HasAuthority())
	{
		return;
	}
	OverrideMaterial = InMaterial;
	ApplyVisualMesh();
}

void AAFLLootCarryPickup::OnRep_VisualMaterial()
{
	ApplyVisualMesh();
}

void AAFLLootCarryPickup::ApplyVisualMesh()
{
	if (OverrideMesh && VisualMesh)
	{
		VisualMesh->SetStaticMesh(OverrideMesh);
		// E1 (BUG-1): gibs are real-limb-sized (~18-27cm). Render at NATIVE scale -- the ctor's 0.3 token-scale
		// (sized for the loot CUBE) shrinks a head to ~6cm, lost among the 30cm cubes ("boxes not heads"). The
		// cube default keeps 0.3: with no override this branch never runs, so the cube is untouched.
		VisualMesh->SetRelativeScale3D(FVector(1.0f));
		// Instrument (the box-not-head diagnosis hook): confirms the gib mesh LANDED on the scattered pickup, on
		// each machine (auth=1 = the server apply via SetVisualMesh; auth=0 = the client OnRep apply). If this
		// fires but the operator still sees a cube -> SetStaticMesh isn't taking; if it never fires client-side
		// -> OverrideMesh isn't replicating.
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: pickup %s applied gib form %s at scale 1.0 (auth=%d)"),
			*GetName(), *GetNameSafe(OverrideMesh), HasAuthority() ? 1 : 0);
	}

	// PRESENTATION (material): skin the scattered gib with the victim's slot-1 MIC on EVERY slot (mirrors the fresh
	// gib's both-slots assign) so it reads as WHOSE limb/head it is. Independent of the mesh branch -- applies on the
	// server (SetVisualMaterial) + each client (OnRep_VisualMaterial). Null override -> the mesh keeps its own material.
	if (OverrideMaterial && VisualMesh)
	{
		const int32 NumSlots = VisualMesh->GetNumMaterials();
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			VisualMesh->SetMaterial(Slot, OverrideMaterial);
		}
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
