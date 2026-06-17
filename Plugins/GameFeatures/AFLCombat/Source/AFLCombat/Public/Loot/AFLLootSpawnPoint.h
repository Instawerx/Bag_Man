// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "AFLLootSpawnPoint.generated.h"

class UBillboardComponent;

/**
 * AAFLLootSpawnPoint -- a designer-placed loot spawn marker (Loot Phase 2 spawn-loop). A bare tagged
 * transform with NO logic: at match start UAFLLootDirectorComponent finds these by class, matches each
 * one's SpawnPointTag against a FAFLLootCacheDef::SpawnPointTag, and spawns that def's CacheClass at this
 * marker's transform (the proven AAFLLootCacheInstant / BP_AFL_LootCacheCarry, which already replicate).
 *
 * The tag is an FGameplayTag (not an FName actor Tag) -- it MATCHES the config's FGameplayTag SpawnPointTag,
 * gets the editor tag-picker + validation + hierarchy, and avoids the FName-string silent-no-spawn surface
 * the loot system documents. Place these away from player starts, reachable + distributed. Editor billboard
 * for placement visibility (editor-only -- billboards don't render in-game).
 */
UCLASS()
class AFLCOMBAT_API AAFLLootSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	AAFLLootSpawnPoint();

	/** The director matches this against FAFLLootCacheDef::SpawnPointTag (MatchesTag) to spawn that def's
	 *  cache class at this marker. Gameplay-tag = the editor picker + validation (no typo'd FName strings). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Loot")
	FGameplayTag SpawnPointTag;

	FGameplayTag GetSpawnPointTag() const { return SpawnPointTag; }

#if WITH_EDITORONLY_DATA
private:
	/** Editor-only sprite so the designer can see + select the marker in the viewport. */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Billboard;
#endif
};
