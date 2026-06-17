// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootDirectorComponent.h"

#include "AFLCombat.h"                  // LogAFLCombat
#include "Loot/AFLLootConfig.h"
#include "Loot/AFLLootSpawnPoint.h"     // the tagged markers the spawn-loop queries
#include "Engine/World.h"               // SpawnActor + FActorSpawnParameters
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"     // GetAllActorsOfClass

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootDirectorComponent)

UAFLLootDirectorComponent::UAFLLootDirectorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	// Loot Phase 3 -- bind the default config so the experience-granted RAW director loads it (closes the
	// "NO LootConfig set" Phase-2 gap; a BP child / experience override can still repoint it). BeginPlay's
	// LoadSynchronous reads it + logs the cache-def count. Soft path -> no ctor hard-load.
	LootConfig = TSoftObjectPtr<UAFLLootConfig>(FSoftObjectPath(TEXT("/Game/BagMan/Loot/DA_AFL_LootConfig.DA_AFL_LootConfig")));
}

void UAFLLootDirectorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server-only -- loot spawning is authoritative gameplay (mirrors UAFLMatchPhaseComponent's authority gate).
	// This log ALSO proves the residency: if it prints at experience-spawn, the director attached correctly
	// (the H3 lesson -- experience-granted GameStateComponents must be resident at spawn).
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	InitializeLootBaseline();
}

void UAFLLootDirectorComponent::InitializeLootBaseline()
{
	// Phase 2 = the MECHANISM: load the config + log the baseline. The cache/node SPAWN loop lands in
	// Phase 3/4 (with the cache actor + the HARVEST component). Reading the config here proves the per-map
	// baseline inheritance works (director resident + config wired).
	if (LootConfig.IsNull())
	{
		UE_LOG(LogAFLCombat, Display,
			TEXT("[AFLLoot] LootDirector attached on %s -- NO LootConfig set (no baseline loot this map)"),
			*GetNameSafe(GetOwner()));
		return;
	}

	const UAFLLootConfig* Config = LootConfig.LoadSynchronous();
	if (!Config)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLLoot] LootDirector attached on %s -- LootConfig %s failed to load"),
			*GetNameSafe(GetOwner()), *LootConfig.ToString());
		return;
	}

	UE_LOG(LogAFLCombat, Display,
		TEXT("[AFLLoot] LootDirector attached on %s -- config=%s baseline: %d cache(s), %d node(s). "
			 "On-death value = energy ring + dismember (%s)."),
		*GetNameSafe(GetOwner()), *Config->GetName(), Config->Caches.Num(), Config->Nodes.Num(),
		Config->bOnDeathValueIsEnergyRingPlusDismember ? TEXT("confirmed") : TEXT("OVERRIDDEN"));

	// Phase-2 spawn-loop: spawn the configured caches at the tagged markers. Additive + inert for defs with an
	// unset SpawnPointTag, so the current (hand-placed-cache) config spawns nothing here -- no double-spawn.
	SpawnConfiguredCaches(Config);
}

void UAFLLootDirectorComponent::SpawnConfiguredCaches(const UAFLLootConfig* Config)
{
	UWorld* World = GetWorld();
	if (!World || !Config)
	{
		return;
	}

	// All designer-placed markers, gathered once (server-auth -- runs only on the authority path via BeginPlay).
	TArray<AActor*> AllPoints;
	UGameplayStatics::GetAllActorsOfClass(World, AAFLLootSpawnPoint::StaticClass(), AllPoints);

	for (const FAFLLootCacheDef& Def : Config->Caches)
	{
		// Unset tag = not marker-spawned (hand-placed / future); skip so the loop never double-spawns over a
		// hand-placed cache. The current config's defs are unset -> this loop is inert until Phase-3 setup.
		if (!Def.SpawnPointTag.IsValid())
		{
			continue;
		}
		UClass* CacheClass = Def.CacheClass.LoadSynchronous();
		if (!CacheClass)
		{
			UE_LOG(LogAFLCombat, Warning, TEXT("[AFLLoot] spawn-loop: def tag=%s has no CacheClass -- skipped"),
				*Def.SpawnPointTag.ToString());
			continue;
		}

		int32 Spawned = 0;
		for (AActor* PointActor : AllPoints)
		{
			const AAFLLootSpawnPoint* Point = Cast<AAFLLootSpawnPoint>(PointActor);
			if (!Point || !Point->GetSpawnPointTag().MatchesTag(Def.SpawnPointTag))
			{
				continue;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (World->SpawnActor<AActor>(CacheClass, Point->GetActorTransform(), Params))
			{
				++Spawned;
			}
		}

		UE_LOG(LogAFLCombat, Display, TEXT("[AFLLoot] spawn-loop: spawned %d %s at tag=%s marker(s)"),
			Spawned, *CacheClass->GetName(), *Def.SpawnPointTag.ToString());
	}
}
