// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootCacheCarry.h"

#include "AFLCombat.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/AFLGrabbableComponent.h"   // the grab substrate type (AFLMovement)
#include "Loot/AFLLootGrantComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootCacheCarry)

AAFLLootCacheCarry::AAFLLootCacheCarry()
{
	PrimaryActorTick.bCanEverTick = false;

	// Physics-prop posture MIRRORS the proven head loot-box (PhysicsActor + simulate + snapshot replication)
	// -- the grab attaches it to hand_r, release re-enables physics. Collision so the grab discovery finds it.
	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	// Movable so a MAP-PLACED instance can simulate (StaticMeshComponent defaults to Static, which blocks
	// physics). Mirrors the runtime-spawned loot-box, which is Movable by default.
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetRelativeScale3D(FVector(0.4f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	// The grab substrate. GrabAbility is set by the BP child (BP_AFL_LootCacheCarry) as an inherited-component
	// default -- MIRRORING the proven AAFLHeadLootBox / BP_AFL_HeadLootBox (Grabbable.GrabAbility = GA_AFL_Grab_C).
	// NOT an FClassFinder here: GA_AFL_Grab lives in the AFLMovement GameFeature, not mounted at this AFLCombat
	// CDO-ctor -> a finder fails (the cross-GameFeature load-order trap). A BP default resolves post-mount + is
	// redirector-safe (the within-Lyra fix; decision C's pure-C++ preference is what produced the defect).
	Grabbable = CreateDefaultSubobject<UAFLGrabbableComponent>(TEXT("Grabbable"));

	LootGrant = CreateDefaultSubobject<UAFLLootGrantComponent>(TEXT("LootGrant"));
}

void AAFLLootCacheCarry::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (LootGrant)
	{
		LootGrant->Configure(EAFLLootValueModel::Watts, LootWatts, Eligibility, /*OwnerActor=*/nullptr, TEXT("cache-carry"));
	}
	if (Grabbable)
	{
		Grabbable->OnGrabbedBy.AddDynamic(this, &AAFLLootCacheCarry::HandleGrabbed);
	}
}

void AAFLLootCacheCarry::HandleGrabbed(AActor* Grabber)
{
	// Server-auth (the grab broadcast is server-side; TryGrant also guards authority). Grant on grab; the
	// grant-once guard makes a re-grab a no-op (the looted cache stays a carriable prop, like the loot-box
	// enemy path). No owner -> Anyone/Team eligibility (no reattach branch).
	if (LootGrant)
	{
		LootGrant->TryGrant(Grabber);
	}
	UE_LOG(LogAFLCombat, Display, TEXT("[AFLLoot] CARRY cache %s grabbed by %s -> grant"),
		*GetName(), *GetNameSafe(Grabber));
}
