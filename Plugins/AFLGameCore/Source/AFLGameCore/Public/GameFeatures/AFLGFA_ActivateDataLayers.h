// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"   // EDataLayerRuntimeState (UENUM, DataLayerInstance.h:24)

#include "AFLGFA_ActivateDataLayers.generated.h"

class UDataLayerAsset;
class UGameInstance;
class UWorld;
struct FWorldContext;

/**
 * UAFLGFA_ActivateDataLayers  (district streaming -- the piece between districts existing and being playable)
 *
 * Sets the runtime state of one or more World Partition data layers when an experience activates, so a
 * district (D1 Duel / D2 Arena / D3 Team) streams in as part of loading the experience rather than needing
 * a console command. No stock GameFeatureAction does this -- see the note on AddWPContent below.
 *
 * VERIFIED BEFORE WRITING (UE 5.6 source):
 *  - UDataLayerManager::SetDataLayerRuntimeState(const UDataLayerAsset*, EDataLayerRuntimeState, bool)
 *    -> bool.  DataLayerManager.h:95 / DataLayerManager.cpp:289. Returns FALSE when the asset has no
 *    DataLayerInstance in this world -- i.e. "wrong map" is indistinguishable from success unless the
 *    return value is read, which is why every call here is logged with its result.
 *  - Server authority: AWorldDataLayers::CanChangeDataLayerRuntimeState (WorldDataLayers.cpp:118) rejects
 *    a client with ESetDataLayerRuntimeStateError::AuthoritativeFromClient (line 157), and that rejection
 *    is logged at *Verbose* (WorldDataLayers.cpp:270) which is OFF by default. A client call is therefore
 *    a SILENT no-op. We refuse on the client ourselves, loudly, rather than lean on that.
 *  - Effective state replicates outward: AWorldDataLayers::GetLifetimeReplicatedProps (WorldDataLayers.cpp
 *    :73-83) replicates RepEffectiveActiveDataLayerNames / RepEffectiveLoadedDataLayerNames (push-based),
 *    written at lines 215-216 and consumed by clients at 384/391. So the server sets, clients follow.
 *  - NOT the same as UGameFeatureAction_AddWPContent: that action creates a UContentBundleDescriptor and a
 *    FContentBundleClient and *injects new content* from the plugin (GameFeatureAction_AddWPContent.cpp:17,
 *    31, 36). It never touches runtime state. Content bundles ADD actors; data layer state TOGGLES actors
 *    the map already authored. Different mechanism, different problem.
 *
 * WHY IT SUBCLASSES THE ENGINE UGameFeatureAction rather than Lyra's UGameFeatureAction_WorldActionBase:
 * that base is UCLASS(Abstract) with no LYRAGAME_API export (GameFeatureAction_WorldActionBase.h:21-22), so
 * subclassing it outside LyraGame fails to link. UAFLGFA_WeaponSpawns hit the same wall and reproduced the
 * base's small OnStartGameInstance / GetWorldContexts plumbing locally; this action conforms to that.
 *
 * WHY THE EXPERIENCE HOOK AND NOT OnGameFeatureActivating DIRECTLY: OnGameFeatureActivating fires per
 * FEATURE, not per world, and can fire before the world's experience has finished loading. The layer must
 * be Activated before any pawn is spawned, so per world we register
 * ULyraExperienceManagerComponent::CallOrRegister_OnExperienceLoaded_HighPriority. That runs ahead of
 * ALyraGameMode's own normal-priority registration (LyraGameMode.cpp:459), whose handler restarts already
 * connected players (LyraGameMode.cpp:305-321); HandleStartingNewPlayer is gated on IsExperienceLoaded
 * (LyraGameMode.cpp:391-397). High priority therefore lands before the first pawn exists.
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Activate AFL Data Layers (District)"))
class UAFLGFA_ActivateDataLayers final : public UGameFeatureAction
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

	/**
	 * THE DISTRICT COMES FROM THE PLAY-SPACE, VIA A URL OPTION (R60, operator ruling).
	 *
	 * Districts are VENUES SORTED BY SIZE, not modes: D1 hosts a Haywire duel, a ProMod duel, a Shootout,
	 * all of it. So the district must NOT be baked into the experience — that binds size to experience and
	 * needs one asset per size × ruleset × league, the config explosion the queue architecture exists to
	 * prevent (`ssot/matchmaking.md` D2's three-layer split: QUEUE → MAP POOL → PLAY-SPACE).
	 *
	 * **THIS CONFORMS TO THE MECHANISM LYRA ALREADY USES FOR `Experience` ITSELF.** A playlist DA carries
	 * `ExtraArgs` — *"Extra arguments passed as URL options to the game"*
	 * (`LyraUserFacingExperienceDefinition.h:32`) — which `ConstructTravelURL` turns into `?Key=Value`
	 * (`CommonSessionSubsystem.cpp:268-281`), and Lyra reads its own back out of `OptionsString`
	 * (`LyraGameMode.cpp:104-108`). `Experience` is literally just an ExtraArgs entry
	 * (`LyraUserFacingExperienceDefinition.cpp:40`). We add `District` alongside it.
	 *
	 * Because `ExtraArgs` lives on the USER-FACING definition (the playlist DA) and not on the
	 * ExperienceDefinition, **two playlists can name the SAME experience and DIFFERENT districts** — which
	 * is exactly the two independent axes R60 requires.
	 */
	UPROPERTY(EditAnywhere, Category = "AFL|Districts")
	FString DistrictOptionName = TEXT("District");

	/**
	 * FALLBACK ONLY. Used when the URL carries no district option — a directly-loaded map, a PIE session, or
	 * a legacy config authored before R60.
	 *
	 * **Leave this EMPTY for anything the matchmaker launches.** An entry here is a district baked into an
	 * experience, which is the coupling R60 rejects; it is retained solely so existing configs keep working
	 * and so a map can be opened by hand without a URL.
	 */
	UPROPERTY(EditAnywhere, Category = "AFL|Districts", meta = (DisplayName = "Fallback Data Layers (no URL option)"))
	TArray<TSoftObjectPtr<UDataLayerAsset>> DataLayers;

	/** State to request on activation. Activated = streamed in and ticking; Loaded = present but not active. */
	UPROPERTY(EditAnywhere, Category = "AFL|Districts")
	EDataLayerRuntimeState TargetState = EDataLayerRuntimeState::Activated;

	/**
	 * Restore RestoreState on deactivation.
	 *
	 * In the COMMON path this is a no-op: ending an experience means travel, the world tears down, and the
	 * layer goes with it. It matters in the one case this project actually has -- three districts that are
	 * alternate configurations of ONE map (R46). If an experience is ever swapped on a live world, leaving
	 * the previous district Activated would stream two arenas at once. Cheap to guarantee, so it is.
	 */
	UPROPERTY(EditAnywhere, Category = "AFL|Districts")
	bool bRestoreStateOnDeactivate = true;

	UPROPERTY(EditAnywhere, Category = "AFL|Districts", meta = (EditCondition = "bRestoreStateOnDeactivate"))
	EDataLayerRuntimeState RestoreState = EDataLayerRuntimeState::Unloaded;

private:
	/** OnStartGameInstance hook, for a world whose game instance starts after this feature activated. */
	void HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext);

	/** Per-world entry: validates the world, then defers the actual set to experience-loaded. */
	void HookWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext);

	/** Does the work. Server-only; logs every layer with requested state, return value and effective state. */
	void ApplyStateToWorld(TWeakObjectPtr<UWorld> WeakWorld, EDataLayerRuntimeState InState, const TCHAR* Phase);

	/**
	 * Resolve which layers this WORLD should drive: the `?District=` option if present, else the fallback.
	 *
	 * Resolution is by NAME against the world's own UDataLayerManager, so the action holds no district list
	 * and cannot go stale against a map. Deliberately deterministic — the deactivate path re-resolves rather
	 * than caching, and gets the same answer for the same world.
	 */
	TArray<const UDataLayerAsset*> ResolveLayersForWorld(UWorld* World, class UDataLayerManager* Manager, const TCHAR* Phase) const;

	static const TCHAR* StateName(EDataLayerRuntimeState State);

	/** OnStartGameInstance handles, per activation context (removed on deactivation). */
	TMap<FGameFeatureStateChangeContext, FDelegateHandle> GameInstanceStartHandles;

	/** Worlds this context applied to, so deactivation restores exactly those and no others. */
	TMap<FGameFeatureStateChangeContext, TArray<TWeakObjectPtr<UWorld>>> AppliedWorlds;
};
