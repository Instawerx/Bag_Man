// Copyright C12 AI Gaming. All Rights Reserved.

#include "Character/AFLHeroComponent.h"

#include "AFLCombat.h"
#include "Equipment/LyraEquipmentDefinition.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHeroComponent)


const FName UAFLHeroComponent::NAME_ActorFeatureName("AFLHero");


UAFLHeroComponent::UAFLHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Inherit Lyra's tick/replication defaults; the AFL layer only adds an
	// equipment lifecycle on top of the existing hero plumbing.
}

void UAFLHeroComponent::BeginPlay()
{
	Super::BeginPlay();

	// Equipping is authority-only. ULyraEquipmentManagerComponent replicates
	// the FFastArraySerializer list, so simulated clients see the result
	// through their own equipment manager without any code path on this side.
	const APawn* OwningPawn = GetPawn<APawn>();
	if (OwningPawn && OwningPawn->HasAuthority())
	{
		ApplyDefaultEquipment();
	}
}

void UAFLHeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Mirror BeginPlay's authority gate so simulated proxies do not no-op
	// against an empty applied-list and log a noisy unequip every shutdown.
	const APawn* OwningPawn = GetPawn<APawn>();
	if (OwningPawn && OwningPawn->HasAuthority())
	{
		UnequipAll();
	}

	Super::EndPlay(EndPlayReason);
}

void UAFLHeroComponent::ApplyDefaultEquipment()
{
	if (DefaultEquipmentDefinitions.Num() == 0)
	{
		return;
	}

	ULyraEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	if (!EquipmentManager)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFLHeroComponent: pawn %s has no ULyraEquipmentManagerComponent; skipping ApplyDefaultEquipment."),
			*GetNameSafe(GetOwner()));
		return;
	}

	AppliedEquipmentInstances.Reserve(DefaultEquipmentDefinitions.Num());

	for (const TSubclassOf<ULyraEquipmentDefinition>& Definition : DefaultEquipmentDefinitions)
	{
		if (!Definition)
		{
			continue;
		}

		// EquipItem authors a FLyraAppliedEquipmentEntry and grants the entry's
		// AbilitySet to the pawn's ASC inside Lyra's own code. The AFL layer
		// never grants abilities to the ASC directly (AFL-0215 hard rail; all
		// grants come from ULyraPawnData ability sets via Lyra's EquipmentManager).
		ULyraEquipmentInstance* Instance = EquipmentManager->EquipItem(Definition);
		if (Instance)
		{
			AppliedEquipmentInstances.Add(Instance);
		}
	}
}

void UAFLHeroComponent::UnequipAll()
{
	if (AppliedEquipmentInstances.Num() == 0)
	{
		return;
	}

	ULyraEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	if (!EquipmentManager)
	{
		// Manager already torn down (pawn destroyed mid-game-feature-deactivation);
		// the equipment list dies with it, nothing to do.
		AppliedEquipmentInstances.Reset();
		return;
	}

	for (const TWeakObjectPtr<ULyraEquipmentInstance>& WeakInstance : AppliedEquipmentInstances)
	{
		if (ULyraEquipmentInstance* Instance = WeakInstance.Get())
		{
			EquipmentManager->UnequipItem(Instance);
		}
	}

	AppliedEquipmentInstances.Reset();
}

ULyraEquipmentManagerComponent* UAFLHeroComponent::FindEquipmentManager() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<ULyraEquipmentManagerComponent>() : nullptr;
}
