// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDisplayRack.h"

#include "AFLDisplayPedestal.h"
#include "AFLHub.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDisplayRack)

AAFLDisplayRack::AAFLDisplayRack()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // client-local retail (s4): the server never hosts shopping
}

void AAFLDisplayRack::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	Restock();
}

void AAFLDisplayRack::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (AAFLDisplayPedestal* P : Stocked)
	{
		if (IsValid(P))
		{
			P->Destroy();
		}
	}
	Stocked.Reset();
	Super::EndPlay(EndPlayReason);
}

void AAFLDisplayRack::Restock()
{
	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(GetWorld());
	if (!Catalog)
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_RETAIL: rack '%s' found no catalog subsystem."), *GetName());
		return;
	}
	TArray<FAFLCatalogEntry> Sellable;
	Catalog->GetPurchasableEntries(Sellable);

	TArray<FName> Stock;
	for (const FAFLCatalogEntry& E : Sellable)
	{
		if (E.CosmeticId.ToString().StartsWith(NamespacePrefix, ESearchCase::IgnoreCase))
		{
			Stock.Add(E.CosmeticId);
			if (Stock.Num() >= SlotCount)
			{
				break;
			}
		}
	}
	if (Stock.Num() == 0)
	{
		UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: rack '%s' has no sellable stock for '%s' (surface stays empty, never fakes)."),
			*GetName(), *NamespacePrefix);
		return;
	}

	UClass* UsePedestal = PedestalClass ? *PedestalClass : AAFLDisplayPedestal::StaticClass();
	const FVector Right = GetActorRightVector();
	const float RowHalf = 0.5f * SlotSpacing * (Stock.Num() - 1);
	for (int32 i = 0; i < Stock.Num(); ++i)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Loc = GetActorLocation() + Right * (SlotSpacing * i - RowHalf);
		AAFLDisplayPedestal* P = GetWorld()->SpawnActor<AAFLDisplayPedestal>(UsePedestal, Loc, GetActorRotation(), Params);
		if (P)
		{
			P->CosmeticId = Stock[i];
			Stocked.Add(P);
		}
	}
	UE_LOG(LogAFLHub, Log, TEXT("AFL_RETAIL: rack '%s' stocked %d/%d from '%s'."),
		*GetName(), Stocked.Num(), SlotCount, *NamespacePrefix);
}
