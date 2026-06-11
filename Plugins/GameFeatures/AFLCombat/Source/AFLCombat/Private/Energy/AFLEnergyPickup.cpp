// Copyright C12 AI Gaming. All Rights Reserved.

#include "Energy/AFLEnergyPickup.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Effects/GE_AFL_EnergyGain_Small.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLEnergyPickup)

// Per-file Unity-safe suffixes; FName values are the canonical tag strings.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Energy_Gain_Pickup, "Data.Energy.Gain");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Energy_Collected_Pickup, "Event.Energy.Collected");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Status_Death_Pickup, "Status.Death");


AAFLEnergyPickup::AAFLEnergyPickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// Spawned-actor replication posture (F5 rule): every runtime spawn replicates from birth, and
	// the magnetic pull rides replicated movement to clients.
	bReplicates = true;
	SetReplicatingMovement(true);

	// AFL-1403 economy: tight relevancy (pickups only matter near), modest update rate while
	// pulling, dormant at rest (BeginPlay sets DormantAll; the magnet tick wakes it).
	SetNetUpdateFrequency(20.0f);
	SetNetCullDistanceSquared(4000.0f * 4000.0f);

	// Prim-as-root (movable-asset rule): the collect sphere IS the root.
	CollectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollectSphere"));
	CollectSphere->InitSphereRadius(60.0f);
	CollectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollectSphere->SetGenerateOverlapEvents(true);
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
		CollectSphere->OnComponentBeginOverlap.AddDynamic(this, &AAFLEnergyPickup::OnCollectOverlap);
		// Rest-state dormancy until the magnet acquires (AFL-1403).
		SetNetDormancy(DORM_DormantAll);
	}
	else
	{
		// Clients never tick the magnet; replicated movement drives them.
		SetActorTickEnabled(false);
	}
}

bool AAFLEnergyPickup::IsViableCollector(const AActor* Candidate) const
{
	if (!Candidate || !Candidate->IsA<APawn>())
	{
		return false;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const_cast<AActor*>(Candidate));
	if (!ASC)
	{
		return false;
	}
	// Dead pawns are invisible to the magnet AND the collect -- a death burst must never re-collect
	// into its own corpse (Status.Death parent query matches Dying/Dead children).
	if (ASC->HasMatchingGameplayTag(TAG_Status_Death_Pickup))
	{
		return false;
	}
	return true;
}

void AAFLEnergyPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bCollected)
	{
		return;
	}

	// Server-authoritative magnetic pull: nearest viable pawn within MagnetRadius.
	const FVector MyLoc = GetActorLocation();
	AActor* Target = nullptr;
	float BestDistSq = MagnetRadius * MagnetRadius;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const float DistSq = static_cast<float>(FVector::DistSquared(It->GetActorLocation(), MyLoc));
		if (DistSq < BestDistSq && IsViableCollector(*It))
		{
			BestDistSq = DistSq;
			Target = *It;
		}
	}

	if (Target)
	{
		if (!bMagnetAwake)
		{
			// Magnetize-wake (AFL-1403): leave dormancy so the pull replicates.
			bMagnetAwake = true;
			SetNetDormancy(DORM_Awake);
			FlushNetDormancy();
		}
		const FVector TargetLoc = Target->GetActorLocation();
		SetActorLocation(FMath::VInterpTo(MyLoc, TargetLoc, DeltaSeconds, PullInterpSpeed));
	}
	else if (bMagnetAwake)
	{
		bMagnetAwake = false;
		SetNetDormancy(DORM_DormantAll);
	}
}

void AAFLEnergyPickup::OnCollectOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (!HasAuthority() || bCollected || !IsViableCollector(OtherActor))
	{
		return;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	if (!ASC)
	{
		return;
	}
	bCollected = true;

	// The rail: the attribute moves ONLY through the gain GE (SetByCaller magnitude = this pickup's
	// value). Clamping + the threshold broadcast live on the attribute set.
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(OtherActor, this);
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
		Message.Collector = OtherActor;
		Message.EnergyValue = EnergyValue;
		Message.Location = GetActorLocation();
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(TAG_Event_Energy_Collected_Pickup, Message);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_ENERGY: %s collected by %s (+%.0f)."),
		*GetName(), *GetNameSafe(OtherActor), EnergyValue);

	Destroy();
}
