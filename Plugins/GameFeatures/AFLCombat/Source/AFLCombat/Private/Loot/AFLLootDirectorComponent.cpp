// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootDirectorComponent.h"

#include "AFLCombat.h"                  // LogAFLCombat
#include "Loot/AFLLootConfig.h"
#include "GameFramework/Actor.h"

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
			 "On-death value = energy ring + dismember (%s). Spawn loop lands Phase 3/4."),
		*GetNameSafe(GetOwner()), *Config->GetName(), Config->Caches.Num(), Config->Nodes.Num(),
		Config->bOnDeathValueIsEnergyRingPlusDismember ? TEXT("confirmed") : TEXT("OVERRIDDEN"));
}
