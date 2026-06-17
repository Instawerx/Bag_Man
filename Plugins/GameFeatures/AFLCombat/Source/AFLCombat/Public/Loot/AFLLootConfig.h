// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "Loot/AFLLootTypes.h"

#include "AFLLootConfig.generated.h"

class AActor;

/**
 * One arena-cache definition (Phase 3 consumer). Carries the per-cache RETRIEVAL-MODE flag (#2), the
 * value model + KNOWN value (the deterministic economy contents -- never a roll), the eligibility (Q1),
 * and where it spawns. The CacheClass actor is built in Phase 3; this schema is ready for it now.
 */
USTRUCT(BlueprintType)
struct FAFLLootCacheDef
{
	GENERATED_BODY()

	/** Per-cache retrieval mode (#2): Carry (grab, carry-to-extract) or Instant (overlap supply). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	EAFLLootRetrievalMode RetrievalMode = EAFLLootRetrievalMode::Instant;

	/** What it grants (ECONOMY Watts/energy this phase; GameplayResource = Phase 5). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	EAFLLootValueModel ValueModel = EAFLLootValueModel::Watts;

	/** The KNOWN value (Watts or energy). Deterministic -- the config IS the known-value source. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	int32 Value = 0;

	/** Who may loot it (Q1): Anyone / TeamOnly (a team's own cache). EnemyOnly is dismember-specific. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	EAFLLootEligibility Eligibility = EAFLLootEligibility::Anyone;

	/** The map spawn point tag this cache spawns at (the director matches TargetPoints tagged Loot.Spawn.*). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	FGameplayTag SpawnPointTag;

	/** The cache actor to spawn (soft -- async-load, no hard ref). The actor class is built in Phase 3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	TSoftClassPtr<AActor> CacheClass;
};

/**
 * One resource-node definition (Phase 4 consumer -- HARVEST-over-time, Q2). A regenerating, contestable
 * point that yields energy/Watts per second while harvested. Deterministic yield. Schema ready now.
 */
USTRUCT(BlueprintType)
struct FAFLLootNodeDef
{
	GENERATED_BODY()

	/** Known yield per second while harvested (deterministic). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	int32 YieldPerSecond = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	EAFLLootValueModel ValueModel = EAFLLootValueModel::CarryToExtractEnergy;

	/** Seconds to regenerate after depletion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	float RegenSeconds = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	FGameplayTag SpawnPointTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	TSoftClassPtr<AActor> NodeClass;
};

/**
 * UAFLLootConfig (DA_AFL_LootConfig) -- the per-mode loot manifest the UAFLLootDirectorComponent reads at
 * match start. The "every map + game-mode baseline": a new arena inherits loot by pointing the director at
 * one of these + tagging spawn points. ALL values deterministic (the asset is the known-value source --
 * IRONICS_ECONOMY_SPEC.md NO-RANDOMIZED-ACQUISITION). The cache/node arrays are the Phase 3/4 schema; this
 * phase ships the asset + the director reading it (caches/nodes spawn in their phases).
 */
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLLootConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Arena caches to spawn (Phase 3). Each carries its own mode/value/eligibility (#2/Q1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	TArray<FAFLLootCacheDef> Caches;

	/** Resource nodes to spawn (Phase 4 -- HARVEST, Q2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	TArray<FAFLLootNodeDef> Nodes;

	/** Documentation hook: drop-on-death value = the EXISTING energy ring + dismember loot (Q1/#1 -- no new
	 *  bounty). True (default) asserts the baseline uses that confirmed model; this asset adds NO new on-death
	 *  economy injection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	bool bOnDeathValueIsEnergyRingPlusDismember = true;
};
