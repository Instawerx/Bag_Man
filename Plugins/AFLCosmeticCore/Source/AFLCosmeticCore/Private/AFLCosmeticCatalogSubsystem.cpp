// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLCosmeticCatalogSubsystem.h"

#include "AFLCosmeticCatalog.h"
#include "AFLCosmeticCore.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"        // GEngine / GetWorldFromContextObject
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCosmeticCatalogSubsystem)

namespace
{
	// The AssetManager primary-asset TYPE the catalog registers under (matches
	// UAFLCosmeticCatalog::GetPrimaryAssetId + the DefaultGame.ini PrimaryAssetTypesToScan entry).
	const FPrimaryAssetType AFLCosmeticCatalogType(TEXT("AFLCosmeticCatalog"));
}

void UAFLCosmeticCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadCatalog();
}

void UAFLCosmeticCatalogSubsystem::Deinitialize()
{
	Catalog = nullptr;
	Super::Deinitialize();
}

void UAFLCosmeticCatalogSubsystem::LoadCatalog()
{
	UAssetManager& AssetManager = UAssetManager::Get();

	// Resolve the one catalog asset via its primary-asset type. Synchronous load -> the manifest is tiny
	// (FName/int rows + soft refs); the cosmetic ASSETS it points at stay soft and load on demand later.
	TArray<FPrimaryAssetId> Ids;
	AssetManager.GetPrimaryAssetIdList(AFLCosmeticCatalogType, Ids);

	if (Ids.Num() == 0)
	{
		// Not fatal: if the asset isn't authored / the type isn't registered, the subsystem is simply
		// not-ready (IsReady() false) and resolves nothing. Logged so a "catalog miss" is legible.
		UE_LOG(LogAFLCosmeticCore, Warning,
			TEXT("[Catalog] No AFLCosmeticCatalog primary asset found (register PrimaryAssetTypesToScan + author DA_AFL_CosmeticCatalog)."));
		return;
	}

	if (Ids.Num() > 1)
	{
		UE_LOG(LogAFLCosmeticCore, Warning,
			TEXT("[Catalog] %d AFLCosmeticCatalog assets found; expected ONE global catalog. Using the first (%s)."),
			Ids.Num(), *Ids[0].ToString());
	}

	const FSoftObjectPath Path = AssetManager.GetPrimaryAssetPath(Ids[0]);
	Catalog = Cast<UAFLCosmeticCatalog>(Path.TryLoad());

	if (Catalog)
	{
		Catalog->BuildLookup(); // ensure the accelerator is built (PostLoad also does, belt + suspenders)
		UE_LOG(LogAFLCosmeticCore, Log, TEXT("[Catalog] Loaded %s (%d entries)."),
			*Catalog->GetName(), Catalog->Entries.Num());
	}
	else
	{
		UE_LOG(LogAFLCosmeticCore, Warning, TEXT("[Catalog] Failed to load catalog asset at %s."), *Path.ToString());
	}
}

const FAFLCatalogEntry* UAFLCosmeticCatalogSubsystem::FindEntry(FName CosmeticId) const
{
	return Catalog ? Catalog->FindEntry(CosmeticId) : nullptr;
}

UPrimaryDataAsset* UAFLCosmeticCatalogSubsystem::ResolveAsset(FName CosmeticId) const
{
	const FAFLCatalogEntry* Entry = FindEntry(CosmeticId);
	if (!Entry)
	{
		return nullptr;
	}
	// Synchronous load of the soft ref (cooked + small). LoadSynchronous returns null if the ref is unset.
	return Entry->Asset.LoadSynchronous();
}

void UAFLCosmeticCatalogSubsystem::GetEntriesByType(EAFLCosmeticType Type, TArray<const FAFLCatalogEntry*>& OutEntries) const
{
	OutEntries.Reset();
	if (!Catalog)
	{
		return;
	}
	for (const FAFLCatalogEntry& Entry : Catalog->Entries)
	{
		if (Entry.Type == Type)
		{
			OutEntries.Add(&Entry);
		}
	}
}

UAFLCosmeticCatalogSubsystem* UAFLCosmeticCatalogSubsystem::Get(const UObject* WorldContext)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UAFLCosmeticCatalogSubsystem>();
		}
	}
	return nullptr;
}
