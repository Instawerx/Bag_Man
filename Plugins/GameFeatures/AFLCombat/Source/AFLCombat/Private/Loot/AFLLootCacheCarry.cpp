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

	// Loot-Carry Phase B: a stable CHANNEL-COLLECT target, NOT a carriable physics prop. Migrating to the
	// collect-channel means the cache is no longer picked up + carried, so it has no reason to simulate --
	// and a bouncing prop was the operator-flagged "bouncing but can't pick up" issue.
	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	// Movable (runtime-spawned) + PhysicsActor collision RETAINED -- it is QueryAndPhysics, so the QUERY half
	// still answers the grab-discovery trace + lets the channel target it -- but SIMULATION OFF: no bounce, a
	// stable target. Physics-simulation and query-collision are separable; we drop ONLY the simulation.
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(false);   // was true -- the physics-off fix (query collision retained above)
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
	// Loot-Carry Phase B: the CARRY cache is COLLECT-LOOT -- the grab ability forks to the collect-channel
	// (UAFLAG_CollectChannel) instead of hand-grabbing. The channel grants from this actor's LootGrant on
	// complete (-> the carried pool). Set in the ctor so the BP child inherits it (no .uasset edit needed).
	Grabbable->SetGrabKind(EAFLGrabKind::CollectLoot);

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
		// Loot-Carry Phase B: feed the carried-at-risk POOL (CarryToExtractEnergy), not instant-bank Watts.
		LootGrant->Configure(EAFLLootValueModel::CarryToExtractEnergy, LootWatts, Eligibility, /*OwnerActor=*/nullptr, TEXT("cache-carry"));
		// Consume on collect (Decision B collect->despawn): TryGrant fires OnLootGranted on a SUCCESSFUL grant
		// -> despawn the cache (like the INSTANT cache Destroys). Kills the "re-channel an already-collected
		// box" loop + the bouncing leftover prop.
		LootGrant->OnLootGranted.AddDynamic(this, &AAFLLootCacheCarry::HandleLootGranted);
	}
	// No OnGrabbedBy binding: this cache is CollectLoot (set in the ctor), so the grab ability forks to the
	// collect-channel BEFORE any hand-grab -- OnGrabbedBy never fires. The channel grants from LootGrant on
	// complete (UAFLAG_CollectChannel::OnChannelComplete -> TryGrant) -> HandleLootGranted despawns the cache.
}

void AAFLLootCacheCarry::HandleLootGranted(AActor* Retriever, int32 Value)
{
	// Decision B collect->despawn: the value reached the retriever's carried pool (via the collect-channel);
	// the cache's physical form despawns. Destroy is DEFERRED (end of frame) -> safe inside the TryGrant
	// OnLootGranted broadcast (LootGrant stays valid for the rest of TryGrant). Re-channel is now impossible.
	UE_LOG(LogAFLCombat, Display, TEXT("[AFLLoot] CARRY cache %s collected (+%d by %s) -> despawn"),
		*GetName(), Value, *GetNameSafe(Retriever));
	Destroy();
}
