// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "WeaponSpawns/AFLGFA_WeaponSpawns.h"

#include "WeaponSpawns/AFLWeaponSpawnTable.h"
#include "WeaponSpawns/AFLWeaponSpawnRegistry.h"
#include "AFLCombat.h"                       // LogAFLCombat
#include "Engine/Engine.h"                   // GEngine->GetWorldContexts()
#include "Engine/GameInstance.h"
#include "Engine/World.h"                    // FWorldDelegates, FWorldContext
#include "Equipment/LyraPickupDefinition.h"  // ULyraWeaponPickupDefinition

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGFA_WeaponSpawns)

#define LOCTEXT_NAMESPACE "AFLWeaponSpawns"

void UAFLGFA_WeaponSpawns::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	// Reproduces UGameFeatureAction_WorldActionBase (which is not exported from LyraGame): hook future game
	// instances, and apply to any already-initialized worlds now.
	UE_LOG(LogAFLCombat, Log, TEXT("UAFLGFA_WeaponSpawns::OnGameFeatureActivating (SpawnTable=%s)"), *GetNameSafe(SpawnTable));

	GameInstanceStartHandles.FindOrAdd(Context) = FWorldDelegates::OnStartGameInstance.AddUObject(this,
		&UAFLGFA_WeaponSpawns::HandleGameInstanceStart, FGameFeatureStateChangeContext(Context));

	int32 NumApplied = 0;
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (Context.ShouldApplyToWorldContext(WorldContext))
		{
			++NumApplied;
			RegisterIntoWorld(WorldContext, Context);
		}
	}
	UE_LOG(LogAFLCombat, Log, TEXT("UAFLGFA_WeaponSpawns::OnGameFeatureActivating -> %d existing world context(s) matched."), NumApplied);
}

void UAFLGFA_WeaponSpawns::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	// Unhook the game-instance-start delegate for this context.
	if (FDelegateHandle* FoundHandle = GameInstanceStartHandles.Find(Context))
	{
		FWorldDelegates::OnStartGameInstance.Remove(*FoundHandle);
		GameInstanceStartHandles.Remove(Context);
	}

	// Clear the registries we installed for this context (markers revert to inert on OnCleared).
	if (TArray<TWeakObjectPtr<UAFLWeaponSpawnRegistry>>* Registries = RegisteredByContext.Find(Context))
	{
		for (const TWeakObjectPtr<UAFLWeaponSpawnRegistry>& WeakRegistry : *Registries)
		{
			if (UAFLWeaponSpawnRegistry* Registry = WeakRegistry.Get())
			{
				Registry->Clear();
			}
		}
		RegisteredByContext.Remove(Context);
	}
}

void UAFLGFA_WeaponSpawns::HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext)
{
	if (FWorldContext* WorldContext = GameInstance->GetWorldContext())
	{
		if (ChangeContext.ShouldApplyToWorldContext(*WorldContext))
		{
			RegisterIntoWorld(*WorldContext, ChangeContext);
		}
	}
}

void UAFLGFA_WeaponSpawns::RegisterIntoWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	UE_LOG(LogAFLCombat, Log, TEXT("UAFLGFA_WeaponSpawns::RegisterIntoWorld world='%s' isGameWorld=%d"),
		*GetNameSafe(World), (World && World->IsGameWorld()) ? 1 : 0);
	if ((World == nullptr) || !World->IsGameWorld())
	{
		return;
	}

	if (!SpawnTable)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("UAFLGFA_WeaponSpawns: no SpawnTable assigned; AFL weapon markers in '%s' will stay inert."),
			*World->GetName());
		return;
	}

	UAFLWeaponSpawnRegistry* Registry = World->GetSubsystem<UAFLWeaponSpawnRegistry>();
	if (!Registry)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("UAFLGFA_WeaponSpawns::RegisterIntoWorld: no UAFLWeaponSpawnRegistry subsystem in world '%s'!"), *World->GetName());
		return;
	}

	// Build the engine-typed map from the AFL table and hand it DOWN to the always-loaded registry.
	TMap<FGameplayTag, TObjectPtr<ULyraWeaponPickupDefinition>> Mappings;
	Mappings.Reserve(SpawnTable->Entries.Num());
	for (const FAFLWeaponSpawnEntry& Entry : SpawnTable->Entries)
	{
		if (!Entry.IntentTag.IsValid() || !Entry.PickupData)
		{
			// A malformed row is a data error the designer must see -- warn, don't silently drop.
			UE_LOG(LogAFLCombat, Warning,
				TEXT("UAFLGFA_WeaponSpawns: skipping invalid entry in '%s' (tag '%s', pickup '%s')."),
				*GetNameSafe(SpawnTable), *Entry.IntentTag.ToString(), *GetNameSafe(Entry.PickupData));
			continue;
		}
		Mappings.Add(Entry.IntentTag, Entry.PickupData);
	}

	UE_LOG(LogAFLCombat, Log, TEXT("UAFLGFA_WeaponSpawns::RegisterIntoWorld '%s': handing %d mapping(s) to the registry."),
		*World->GetName(), Mappings.Num());
	Registry->RegisterMappings(Mappings);
	RegisteredByContext.FindOrAdd(ChangeContext).Add(Registry);
}

#if WITH_EDITOR
EDataValidationResult UAFLGFA_WeaponSpawns::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (!SpawnTable)
	{
		Context.AddError(LOCTEXT("NoSpawnTable", "Add AFL Weapon Spawns: SpawnTable is not assigned -- no markers will resolve."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
