// Copyright C12 AI Gaming. All Rights Reserved.

#include "Health/AFLHealthPickup.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/StaticMeshComponent.h"
#include "Effects/AFLGE_OverloadRestore.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "Inventory/LyraInventoryManagerComponent.h"
#include "Loot/AFLOverlapCollectComponent.h"   // the proven overlap substrate -- the root (no magnet here)
#include "NativeGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHealthPickup)

// Per-file Unity-safe suffixes; FName values are the canonical tag strings (Data.Health.Restore is the same
// SetByCaller rail UAFLGE_OverloadRestore reads; Event.Health.Collected mirrors Event.Energy.Collected).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Health_Restore_Pickup, "Data.Health.Restore");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Health_Collected_Pickup, "Event.Health.Collected");


AAFLHealthPickup::AAFLHealthPickup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Mirror AAFLEnergyPickup's spawned-actor posture (F5 rule). No tick (this pickup has no magnet -- the
	// magnet is the only thing that ticked on the energy pickup). Replicates from birth + replicated movement;
	// tight relevancy + rest-dormancy so placed/spawned packs only matter near.
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(20.0f);
	SetNetCullDistanceSquared(4000.0f * 4000.0f);

	// Prim-as-root: the PROVEN UAFLOverlapCollectComponent is the root + collect volume. DIVERGENCE from the
	// energy pickup: MagnetRadius left at the component default (0 = pure walk-over) -- a health pack is a
	// walk-over/walk-into pickup, not a vacuum-up currency drop (the substrate explicitly supports this mode).
	CollectSphere = CreateDefaultSubobject<UAFLOverlapCollectComponent>(TEXT("CollectSphere"));
	SetRootComponent(CollectSphere);

	// Visible default body so C++-spawned/placed pickups are watchable; the reskin BP restyles to the health mesh.
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

void AAFLHealthPickup::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// The substrate fires OnCollected once (server-auth, viable collector) -> our health-specific grant.
		if (CollectSphere)
		{
			CollectSphere->OnCollected.AddDynamic(this, &AAFLHealthPickup::HandleCollected);
		}
		// Rest-state dormancy (mirror the energy pickup). No magnet here, so the pack simply waits for overlap;
		// the overlap event wakes it via the standard replication path.
		SetNetDormancy(DORM_DormantAll);
	}
}

void AAFLHealthPickup::HandleCollected(AActor* Collector)
{
	// Server-auth, exactly once, viable collector -- the substrate guarantees (overlap/one-shot/dead-skip all in
	// the component). Branch on the mode (ruling (1)); mirror the energy pickup's early-return-without-Destroy on
	// a missing grant target (a viable collector always has an ASC; the carriable path additionally needs a
	// controller-inventory, unreachable for a real pawn but guarded).
	if (bInstantHeal)
	{
		// MED-STATION: the rail -- the attribute moves ONLY through the GE (SetByCaller magnitude = HealAmount);
		// clamping + overheal-cap live on UAFLAttributeSet_Combat. Same shape as the energy gain grant.
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Collector);
		if (!ASC)
		{
			return;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(Collector, this);
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(UAFLGE_OverloadRestore::StaticClass(), 1.0f, Context);
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_Health_Restore_Pickup, HealAmount);
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
	else
	{
		// CARRIABLE: add the consumable to the collector's inventory. The InventoryManager lives on the
		// CONTROLLER (LAS_ShooterGame_StandardComponents adds it there) -- the same AddItemDefinition path as
		// UAFLAG_GrantLoadout. AddItemDefinition is UE_API-exported -> safe from C++.
		if (!HealItemDef)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_HEALTH: %s has no HealItemDef; nothing to grant."), *GetName());
			return;
		}
		AController* Controller = nullptr;
		if (APawn* Pawn = Cast<APawn>(Collector))
		{
			Controller = Pawn->GetController();
		}
		if (!Controller)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_HEALTH: collector %s has no controller; cannot grant item."),
				*GetNameSafe(Collector));
			return;
		}
		ULyraInventoryManagerComponent* Inventory = Controller->FindComponentByClass<ULyraInventoryManagerComponent>();
		if (!Inventory)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("AFL_HEALTH: controller %s missing ULyraInventoryManagerComponent."),
				*GetNameSafe(Controller));
			return;
		}
		ULyraInventoryItemInstance* Instance = Inventory->AddItemDefinition(HealItemDef, /*StackCount=*/1);
		if (Instance)
		{
			// Optional QuickBar auto-slot (BP child; the C++-unexportable slotting path). No-op by C++ default.
			OnConsumableGranted(Collector, Instance);
		}
	}

	// Collection feedback surface (HUD/FX listeners; the proven message pattern). HealAmount is only meaningful
	// for the instant heal; the carriable grant reports 0 (an item was added, nothing healed yet).
	if (UWorld* World = GetWorld())
	{
		FAFLHealthCollectedMessage Message;
		Message.Collector = Collector;
		Message.HealAmount = bInstantHeal ? HealAmount : 0.0f;
		Message.bInstantHeal = bInstantHeal;
		Message.Location = GetActorLocation();
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(TAG_Event_Health_Collected_Pickup, Message);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_HEALTH: %s collected by %s (%s)."),
		*GetName(), *GetNameSafe(Collector), bInstantHeal ? TEXT("instant heal") : TEXT("item granted"));

	Destroy();
}
