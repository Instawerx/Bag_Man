// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"

#include "AFLGFA_WeaponSpawns.generated.h"

class UAFLWeaponSpawnTable;
class UAFLWeaponSpawnRegistry;
class UGameInstance;
class FDelegateHandle;
struct FWorldContext;

/**
 * GameFeature action that publishes the AFL weapon-spawn table into each game world's
 * UAFLWeaponSpawnRegistry on activation, and clears it on deactivation.
 *
 * Add this to the AFL experience's Action list and assign SpawnTable. On activate it builds the tag->def
 * map from the table and hands it to the world registry (which broadcasts OnReady so any already-streamed
 * AAFLWeaponSpawner markers resolve); on deactivate it clears the registry (markers revert to inert). It
 * runs on server AND client (the experience activates on both), so resolution is deterministic-local
 * everywhere.
 *
 * WHY IT SUBCLASSES THE ENGINE UGameFeatureAction (not Lyra's UGameFeatureAction_WorldActionBase): the
 * Lyra base is not LYRAGAME_API-exported (Lyra only subclasses it inside LyraGame), so subclassing it from
 * this GameFeature module fails to LINK. We reproduce its tiny OnStartGameInstance / GetWorldContexts
 * plumbing here against the engine's exported base -- self-contained, and avoids a full LyraGame relink.
 *
 * LAYER NOTE: this GameFeature module pushes DOWN into the always-loaded AFLGameCore registry -- the
 * correct direction. AFLGameCore never names this AFL table type.
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add AFL Weapon Spawns"))
class UAFLGFA_WeaponSpawns final : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~ Begin UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~ End UGameFeatureAction interface

#if WITH_EDITOR
	//~ Begin UObject interface
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
	//~ End UObject interface
#endif

	/** The AFL tag->weapon table this action publishes into the world registry. */
	UPROPERTY(EditAnywhere, Category = "AFL|Spawner")
	TObjectPtr<UAFLWeaponSpawnTable> SpawnTable;

private:
	/** OnStartGameInstance hook: apply to a world whose game instance starts after activation. */
	void HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext);

	/** Build + install the tag->def mappings into WorldContext's registry (the per-world work). */
	void RegisterIntoWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext);

	/** OnStartGameInstance delegate handles, per activation context (removed on deactivation). */
	TMap<FGameFeatureStateChangeContext, FDelegateHandle> GameInstanceStartHandles;

	/** Registries installed into, keyed by activation context, so deactivation clears exactly the same ones. */
	TMap<FGameFeatureStateChangeContext, TArray<TWeakObjectPtr<UAFLWeaponSpawnRegistry>>> RegisteredByContext;
};
