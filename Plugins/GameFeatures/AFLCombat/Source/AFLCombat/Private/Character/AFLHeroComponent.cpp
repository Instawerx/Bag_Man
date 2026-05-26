// Copyright C12 AI Gaming. All Rights Reserved.

#include "Character/AFLHeroComponent.h"

#include "AFLCombat.h"
#include "Equipment/LyraEquipmentDefinition.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFeatures/GameFeatureAction_AddInputContextMapping.h"
#include "GameFramework/Pawn.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHeroComponent)


// NAME_ActorFeatureName static definition removed 2026-05-25 as part of
// AFL-0304-Bi diagnosis — see header comment. Component now inherits
// ULyraHeroComponent's "Hero" feature name.


UAFLHeroComponent::UAFLHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Inherit Lyra's tick/replication defaults; the AFL layer only adds an
	// equipment lifecycle on top of the existing hero plumbing.

	// AFL-0304-Bi: ULyraHeroComponent::InitializePlayerInput calls
	// Subsystem->ClearAllMappings() then iterates DefaultInputMappings to
	// repopulate the EnhancedInput subsystem. The Game Feature's
	// AddInputContextMapping action runs BEFORE InitializePlayerInput on the
	// player-pawn path, so any LAS-registered IMCs get wiped and only this
	// array's entries survive. Empty array == no input bindings, which is
	// exactly the dead-WASD symptom diagnosed via live debugger 2026-05-26
	// (DefaultInputMappings.ArrayNum == 0 at the iteration site).
	//
	// Populate in priority order matching the LAS registration:
	//   - IMC_AFL_Movement @ Priority 0  (AFL-specific Dash + movement bindings)
	//   - IMC_Default      @ Priority -1 (Lyra stock WASD/Look/Jump/Crouch fallback)
	// bRegisterWithSettings=true mirrors the GameFeatureAction default so the
	// player-mappable-key UI sees both contexts.
	{
		FInputMappingContextAndPriority AFLMovement;
		AFLMovement.InputMapping = TSoftObjectPtr<UInputMappingContext>(
			FSoftObjectPath(TEXT("/AFLMovement/Input/IMC_AFL_Movement.IMC_AFL_Movement")));
		AFLMovement.Priority = 0;
		AFLMovement.bRegisterWithSettings = true;
		DefaultInputMappings.Add(AFLMovement);

		FInputMappingContextAndPriority LyraDefault;
		LyraDefault.InputMapping = TSoftObjectPtr<UInputMappingContext>(
			FSoftObjectPath(TEXT("/Game/Input/Mappings/IMC_Default.IMC_Default")));
		LyraDefault.Priority = -1;
		LyraDefault.bRegisterWithSettings = true;
		DefaultInputMappings.Add(LyraDefault);
	}
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
