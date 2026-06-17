// Copyright C12 AI Gaming. All Rights Reserved.

#include "Energy/AFLEnergyPickup.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/StaticMeshComponent.h"
#include "Effects/GE_AFL_EnergyGain_Small.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Loot/AFLOverlapCollectComponent.h"   // the proven overlap+magnet substrate -- now the root
#include "NativeGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLEnergyPickup)

// Per-file Unity-safe suffixes; FName values are the canonical tag strings. (Status.Death moved into the
// substrate's IsViableCollector -- the dead-pawn skip is no longer this actor's concern.)
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Energy_Gain_Pickup, "Data.Energy.Gain");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Energy_Collected_Pickup, "Event.Energy.Collected");


AAFLEnergyPickup::AAFLEnergyPickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The actor no longer ticks -- the magnet lives in CollectSphere (UAFLOverlapCollectComponent::TickComponent).
	PrimaryActorTick.bCanEverTick = false;

	// Spawned-actor replication posture (F5 rule): every runtime spawn replicates from birth, and the
	// magnetic pull (now the component's) rides replicated movement to clients.
	bReplicates = true;
	SetReplicatingMovement(true);

	// AFL-1403 economy: tight relevancy (pickups only matter near), modest update rate while pulling, dormant
	// at rest (BeginPlay sets DormantAll; the component's magnet wakes/sleeps the owner).
	SetNetUpdateFrequency(20.0f);
	SetNetCullDistanceSquared(4000.0f * 4000.0f);

	// Prim-as-root (movable-asset rule): the collect sphere IS the root -- now the PROVEN
	// UAFLOverlapCollectComponent (overlap + server magnet + one-shot guard + viable-collector, extracted from
	// this pickup). Mirrors AAFLLootCacheInstant's composition. The component's ctor sets radius 60 / QueryOnly /
	// ECC_Pawn overlap (identical to the old CollectSphere); the energy pickup keeps its 500uu magnet pull.
	CollectSphere = CreateDefaultSubobject<UAFLOverlapCollectComponent>(TEXT("CollectSphere"));
	CollectSphere->MagnetRadius = 500.0f;
	CollectSphere->PullInterpSpeed = 4.0f;
	SetRootComponent(CollectSphere);

	// Visible default body so C++-spawned pickups (harness bursts) are watchable; tier BPs restyle.
	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollectSphere);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetRelativeScale3D(FVector(0.18f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMesh.Object);
	}
}

void AAFLEnergyPickup::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// The substrate fires OnCollected once (server-auth, viable collector) -> our energy-specific grant.
		if (CollectSphere)
		{
			CollectSphere->OnCollected.AddDynamic(this, &AAFLEnergyPickup::HandleCollected);
		}
		// Rest-state dormancy until the component's magnet acquires (AFL-1403); the component wakes/sleeps the owner.
		SetNetDormancy(DORM_DormantAll);
	}
}

void AAFLEnergyPickup::HandleCollected(AActor* Collector)
{
	// Server-auth: the substrate only fires OnCollected on authority, exactly once, for a viable collector --
	// the generic overlap/magnet/one-shot-guard/dead-pawn-skip now lives in UAFLOverlapCollectComponent. This is
	// the energy-SPECIFIC consumer, LIFTED VERBATIM from the old OnCollectOverlap body.
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Collector);
	if (!ASC)
	{
		return;
	}

	// The rail: the attribute moves ONLY through the gain GE (SetByCaller magnitude = this pickup's value).
	// Clamping + the threshold broadcast live on the attribute set.
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Collector, this);
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UGE_AFL_EnergyGain_Small::StaticClass(), 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Energy_Gain_Pickup, EnergyValue);
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	// Collection feedback surface (HUD/score/FX listeners; the proven message pattern).
	if (UWorld* World = GetWorld())
	{
		FAFLEnergyCollectedMessage Message;
		Message.Collector = Collector;
		Message.EnergyValue = EnergyValue;
		Message.Location = GetActorLocation();
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(TAG_Event_Energy_Collected_Pickup, Message);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ENERGY: %s collected by %s (+%.0f)."),
		*GetName(), *GetNameSafe(Collector), EnergyValue);

	Destroy();
}
