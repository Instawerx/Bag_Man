// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootCarryPickup.h"

#include "AFLCombat.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"               // C1: the per-spawn form mesh (SetStaticMesh override)
#include "Materials/MaterialInterface.h"      // PRESENTATION: the per-spawn gib material (SetMaterial on every slot)
#include "Loot/AFLLootCarryComponent.h"
#include "Loot/AFLOverlapCollectComponent.h"
#include "Loot/AFLPartReattachTarget.h"   // V7-2: the owner-reattach seam (the dismember component implements it)
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
	DOREPLIFETIME(AAFLLootCarryPickup, OwnerPlayerId);
	DOREPLIFETIME(AAFLLootCarryPickup, OriginZone);
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

void AAFLLootCarryPickup::InitPartToken(int32 InOwnerPlayerId, EAFLBodyZone InZone, int32 InValue, UStaticMesh* InMesh, UMaterialInterface* InMaterial)
{
	// AUTHORITY (the carry's SpawnPartPickup): a dismember PART pickup -- the token's identity replicates for the
	// V7-2 owner-check; the gib mesh/material apply via the proven SetVisual* path. OwnerPlayerId != INDEX_NONE
	// marks it a PART so HandleCollected routes to CollectPart (vs Collect for a fungible cube).
	if (!HasAuthority())
	{
		return;
	}
	OwnerPlayerId = InOwnerPlayerId;
	OriginZone = InZone;
	LootValue = InValue;
	SetVisualMesh(InMesh);
	SetVisualMaterial(InMaterial);
	// PRESENT-BEFORE-COLLECTIBLE (the box-not-head fix): a scattered PART spawns within ~150uu of the dropper, so its
	// overlap fires point-blank. Without an arm delay it is collectible at BeginPlay and a STATIONARY dropper absorbs
	// it the same frame -- as a DEFAULT cube, because the absorb beats InitPartToken (the +50/no-gib/no-owner pickup
	// the 04:36 log caught). The fresh head/limb get this gate from their grant's bConfigured check; the scattered
	// pickup carries no grant, so set the overlap's arm delay here (the established ~1.5s beat the head/limb use).
	// RELIES on deferred spawn (SpawnPartPickup) so BeginPlay reads this AFTER it's set. Bonus: a pickup that spawns
	// already-overlapping the dropper won't auto-collect when it later arms (BeginOverlap only fires on a NEW enter)
	// -> the part LANDS and stays for the OWNER to walk over (the V7-2 reattach), not the dropper re-absorbing it.
	if (Overlap)
	{
		Overlap->ActivationDelay = 1.5f;
	}
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
		// V7-2 UNIFORM REATTACH: if the OWNER reclaimed their OWN part (collector's player-id == the token's
		// OwnerPlayerId), REATTACH the specific zone to the owner's (the collector's) pawn -- the FULL RestoreZone
		// via the IAFLPartReattachTarget seam (no circular AFLCombat->AFLDismember dep) -- do NOT re-loot it.
		// This closes the operator's keystone: a part is owner-reattachable EVEN after it passed through an enemy's
		// pool (fresh OR scattered-after-collect). An enemy collecting it still loots it (below).
		if (OwnerPlayerId != INDEX_NONE && UAFLLootCarryComponent::ResolvePlayerId(Collector) == OwnerPlayerId)
		{
			for (UActorComponent* Comp : Collector->GetComponents())
			{
				if (Comp && Comp->GetClass()->ImplementsInterface(UAFLPartReattachTarget::StaticClass()))
				{
					IAFLPartReattachTarget::Execute_ReattachPart(Comp, OriginZone);
					UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: pickup %s REATTACHED to OWNER %s (zone=%d)"),
						*GetName(), *GetNameSafe(Collector), static_cast<int32>(OriginZone));
					Destroy();
					return;
				}
			}
		}
		// Non-owner (or no reattach target) -> COLLECT: a PART as a whole indivisible token; a CUBE to the value rail.
		if (UAFLLootCarryComponent* Carry = Collector->FindComponentByClass<UAFLLootCarryComponent>())
		{
			if (OwnerPlayerId != INDEX_NONE)
			{
				FAFLCarriedPart Token;
				Token.OwnerPlayerId = OwnerPlayerId;
				Token.OriginZone    = OriginZone;
				Token.FixedValue    = LootValue;
				Token.GibMesh       = OverrideMesh;
				Token.GibMaterial   = OverrideMaterial;
				Carry->CollectPart(Token);
			}
			else
			{
				Carry->Collect(LootValue);   // a fungible CUBE -> the value rail (unchanged)
			}
		}
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: pickup %s (+%d, %s) collected by %s"),
		*GetName(), LootValue, OwnerPlayerId != INDEX_NONE ? TEXT("part") : TEXT("cube"), *GetNameSafe(Collector));
	Destroy();
}
