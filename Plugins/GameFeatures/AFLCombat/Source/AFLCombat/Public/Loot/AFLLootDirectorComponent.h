// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/GameStateComponent.h"

#include "AFLLootDirectorComponent.generated.h"

class UAFLLootConfig;

/**
 * UAFLLootDirectorComponent -- the per-map loot baseline (Loot Phase 2). A server-only GameStateComponent
 * that arrives via the experience AddComponents row, EXACTLY like UAFLMatchPhaseComponent / the Lyra
 * scoring components (the proven experience-granted-GameStateComponent residency pattern -- it must be
 * resident at experience-spawn, the H3 lesson). On match start it reads the active UAFLLootConfig and
 * (in later phases) spawns the configured caches/nodes at tagged map spawn points.
 *
 * THIS PHASE (Phase 2) ships the MECHANISM: the director attaches at experience-spawn + loads + reads its
 * config + logs the baseline (the ✅ = it is resident + reads the config). Cache spawning = Phase 3
 * (UAFLLootGrantComponent + the cache actor); node spawning = Phase 4 (HARVEST). A new arena inherits loot
 * by: an experience that grants this director + a DA_AFL_LootConfig + tagged spawn points. Zero C++ per map.
 *
 * ALL config values are deterministic (IRONICS_ECONOMY_SPEC.md NO-RANDOMIZED-ACQUISITION).
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLLootDirectorComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UAFLLootDirectorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	/** The per-mode loot manifest (DA_AFL_LootConfig). Soft -- async-load, no hard ref. The experience or a
	 *  BP child of this component sets it; a new arena points here to inherit its loot baseline. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	TSoftObjectPtr<UAFLLootConfig> LootConfig;

private:
	/** Server-auth: load the config (if set) + log the baseline (caches/nodes counts). Phase 2 = read + log;
	 *  the spawn loop lands in Phase 3/4 with the cache/node actors. */
	void InitializeLootBaseline();
};
