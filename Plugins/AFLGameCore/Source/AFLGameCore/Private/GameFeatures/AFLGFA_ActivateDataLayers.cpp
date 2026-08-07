// Copyright C12 AI Gaming. All Rights Reserved.

#include "GameFeatures/AFLGFA_ActivateDataLayers.h"

#include "AFLGameCore.h"                                   // LogAFLGameCore
#include "Engine/Engine.h"                                 // GEngine->GetWorldContexts()
#include "Engine/GameInstance.h"
#include "Engine/World.h"                                  // FWorldDelegates, FWorldContext
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameModeBase.h"                    // AuthorityGameMode->OptionsString (the ?District= read)
#include "Kismet/GameplayStatics.h"                        // ParseOption -- same mechanism Lyra uses for ?Experience=
#include "GameModes/LyraExperienceManagerComponent.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGFA_ActivateDataLayers)

#define LOCTEXT_NAMESPACE "AFLDistricts"

TMap<TWeakObjectPtr<const UWorld>, TWeakObjectPtr<const UDataLayerAsset>>
	UAFLGFA_ActivateDataLayers::ActiveDistrictByWorld;

const UDataLayerAsset* UAFLGFA_ActivateDataLayers::GetActiveDistrictForWorld(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<const UDataLayerAsset>* Found = ActiveDistrictByWorld.Find(World))
	{
		return Found->Get();
	}
	return nullptr;
}

const TCHAR* UAFLGFA_ActivateDataLayers::StateName(EDataLayerRuntimeState State)
{
	switch (State)
	{
	case EDataLayerRuntimeState::Unloaded:  return TEXT("Unloaded");
	case EDataLayerRuntimeState::Loaded:    return TEXT("Loaded");
	case EDataLayerRuntimeState::Activated: return TEXT("Activated");
	default:                                return TEXT("<unknown>");
	}
}

void UAFLGFA_ActivateDataLayers::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFL_DISTRICT: action activating -- district option='?%s=', %d fallback layer(s), target=%s"),
		*DistrictOptionName, DataLayers.Num(), StateName(TargetState));

	// An empty fallback list is the NORMAL, CORRECT shape under R60 -- the district arrives per match as a
	// URL option, so warning here would fire on every properly configured action and train people to ignore
	// the one case that matters. That case is "no option AND no fallback", which cannot be known until a
	// world exists; ResolveLayersForWorld warns there, where it is actually true.
	if (DistrictOptionName.IsEmpty() && DataLayers.IsEmpty())
	{
		// Silent misconfiguration is the failure this project keeps paying for: an action that does
		// nothing looks exactly like an action that worked.
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_DISTRICT: no DistrictOptionName AND no fallback DataLayers -- this action has no way to "
			     "learn which district to activate and will do NOTHING in every world."));
	}

	// Reproduces UGameFeatureAction_WorldActionBase (unexported from LyraGame): hook future game instances,
	// then apply to any world already up.
	GameInstanceStartHandles.FindOrAdd(Context) = FWorldDelegates::OnStartGameInstance.AddUObject(
		this, &UAFLGFA_ActivateDataLayers::HandleGameInstanceStart, FGameFeatureStateChangeContext(Context));

	int32 Matched = 0;
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (Context.ShouldApplyToWorldContext(WorldContext))
		{
			++Matched;
			HookWorld(WorldContext, Context);
		}
	}
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_DISTRICT: %d existing world context(s) matched."), Matched);
}

void UAFLGFA_ActivateDataLayers::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	if (FDelegateHandle* Found = GameInstanceStartHandles.Find(Context))
	{
		FWorldDelegates::OnStartGameInstance.Remove(*Found);
		GameInstanceStartHandles.Remove(Context);
	}

	if (TArray<TWeakObjectPtr<UWorld>>* Worlds = AppliedWorlds.Find(Context))
	{
		for (const TWeakObjectPtr<UWorld>& WeakWorld : *Worlds)
		{
			// If the world already went away this is genuinely nothing to do -- the normal travel path.
			// The restore exists for the live-world experience swap; see the header.
			if (bRestoreStateOnDeactivate && WeakWorld.IsValid())
			{
				ApplyStateToWorld(WeakWorld, RestoreState, TEXT("DEACTIVATE"));
			}
		}
		AppliedWorlds.Remove(Context);
	}
}

void UAFLGFA_ActivateDataLayers::HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext)
{
	if (GameInstance == nullptr)
	{
		return;
	}
	if (FWorldContext* WorldContext = GameInstance->GetWorldContext())
	{
		if (ChangeContext.ShouldApplyToWorldContext(*WorldContext))
		{
			HookWorld(*WorldContext, ChangeContext);
		}
	}
}

void UAFLGFA_ActivateDataLayers::HookWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if ((World == nullptr) || !World->IsGameWorld())
	{
		return;   // editor/preview worlds have no runtime data layer state to set
	}

	// SERVER ONLY, refused HERE rather than by the engine. AWorldDataLayers::CanChangeDataLayerRuntimeState
	// rejects a client (WorldDataLayers.cpp:157) but logs it at Verbose (line 270), which is off by default --
	// so relying on the engine's refusal means a client-side misconfiguration produces no visible signal at
	// all. Refuse loudly instead.
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLGameCore, Verbose,
			TEXT("AFL_DISTRICT: world '%s' is a CLIENT -- skipping. Data layer state is server-authoritative "
			     "and replicates outward; the client will follow the server."), *GetNameSafe(World));
		return;
	}

	AppliedWorlds.FindOrAdd(ChangeContext).AddUnique(World);

	AGameStateBase* GameState = World->GetGameState();
	ULyraExperienceManagerComponent* ExperienceComponent = GameState
		? GameState->FindComponentByClass<ULyraExperienceManagerComponent>()
		: nullptr;

	TWeakObjectPtr<UWorld> WeakWorld(World);

	if (ExperienceComponent == nullptr)
	{
		// No experience component (a non-Lyra game world, or one still assembling). Setting state now is
		// better than not setting it -- but say so, because the ordering guarantee below does not apply.
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_DISTRICT: world '%s' has no ULyraExperienceManagerComponent -- applying immediately. "
			     "The before-first-spawn ordering guarantee does NOT hold on this path."), *GetNameSafe(World));
		ApplyStateToWorld(WeakWorld, TargetState, TEXT("IMMEDIATE"));
		return;
	}

	// HIGH priority: runs before ALyraGameMode's own normal-priority handler (LyraGameMode.cpp:459), which
	// is what restarts already-connected players (LyraGameMode.cpp:305). CallOrRegister fires immediately if
	// the experience has already loaded, so this is correct either way.
	ExperienceComponent->CallOrRegister_OnExperienceLoaded_HighPriority(
		FOnLyraExperienceLoaded::FDelegate::CreateWeakLambda(this,
			[this, WeakWorld](const ULyraExperienceDefinition* /*Experience*/)
			{
				ApplyStateToWorld(WeakWorld, TargetState, TEXT("EXPERIENCE-LOADED"));
			}));
}

void UAFLGFA_ActivateDataLayers::ApplyStateToWorld(TWeakObjectPtr<UWorld> WeakWorld, EDataLayerRuntimeState InState, const TCHAR* Phase)
{
	UWorld* World = WeakWorld.Get();
	if (World == nullptr)
	{
		return;
	}
	if (World->GetNetMode() == NM_Client)
	{
		return;   // re-checked: the world may have become a client between hook and fire
	}

	UDataLayerManager* Manager = UDataLayerManager::GetDataLayerManager(World);
	if (Manager == nullptr)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_DISTRICT[%s]: world '%s' has NO UDataLayerManager -- is this map World Partition? "
			     "No layer was changed."), Phase, *GetNameSafe(World));
		return;
	}

	for (const UDataLayerAsset* Asset : ResolveLayersForWorld(World, Manager, Phase))
	{
		if (Asset == nullptr)
		{
			continue;   // already reported by the resolver
		}

		// An EDITOR data layer has no runtime existence -- the state change is dropped and logged only at
		// Verbose. Catch it here rather than let it read as a working activation.
		if (!Asset->IsRuntime())
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_DISTRICT[%s]: '%s' is an EDITOR data layer. Runtime state cannot be set on it; "
				     "nothing will stream. Only Runtime layers belong on this action."),
				Phase, *Asset->GetName());
			continue;
		}

		const UDataLayerInstance* Instance = Manager->GetDataLayerInstanceFromAsset(Asset);
		if (Instance == nullptr)
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_DISTRICT[%s]: '%s' has no DataLayerInstance in world '%s' -- the asset exists but "
				     "THIS MAP does not reference it. SetDataLayerRuntimeState would return false "
				     "(DataLayerManager.cpp:295)."), Phase, *Asset->GetName(), *GetNameSafe(World));
			continue;
		}

		const EDataLayerRuntimeState Before = Manager->GetDataLayerInstanceEffectiveRuntimeState(Instance);
		const bool bAccepted = Manager->SetDataLayerRuntimeState(Asset, InState);
		const EDataLayerRuntimeState After = Manager->GetDataLayerInstanceEffectiveRuntimeState(Instance);

		// Publish (or retract) the active district for this world. Recorded on ACCEPTANCE, not on effective
		// state: the spawn gate exists precisely because "accepted" precedes "streamed in", and it needs to
		// know what to wait FOR while that gap is open.
		if (bAccepted)
		{
			if (InState == EDataLayerRuntimeState::Activated)
			{
				ActiveDistrictByWorld.Add(World, Asset);
			}
			else
			{
				ActiveDistrictByWorld.Remove(World);
			}
		}

		// Requested / accepted / effective are logged SEPARATELY and deliberately. "Accepted but nothing
		// streamed" and "refused outright" are different bugs, and a single combined line hides which one
		// you have. Effective state is also asynchronous -- it commonly still reads the old value on this
		// frame and settles a moment later, which is expected, not a failure.
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_DISTRICT[%s]: layer='%s' requested=%s accepted=%s effectiveBefore=%s effectiveAfter=%s "
			     "world='%s' netmode=%d"),
			Phase, *Asset->GetName(), StateName(InState), bAccepted ? TEXT("true") : TEXT("FALSE"),
			StateName(Before), StateName(After), *GetNameSafe(World), (int32)World->GetNetMode());

		if (!bAccepted)
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_DISTRICT[%s]: SetDataLayerRuntimeState REFUSED for '%s'. The district will not "
				     "exist and nothing on screen will say so."), Phase, *Asset->GetName());
		}
	}
}

TArray<const UDataLayerAsset*> UAFLGFA_ActivateDataLayers::ResolveLayersForWorld(
	UWorld* World, UDataLayerManager* Manager, const TCHAR* Phase) const
{
	TArray<const UDataLayerAsset*> Out;

	// --- 1. THE PLAY-SPACE'S OWN ANSWER: the ?District= URL option (R60). ---
	//
	// Read from the GameMode's OptionsString, which is where ConstructTravelURL's ?Key=Value pairs land and
	// where Lyra reads its own ?Experience= (LyraGameMode.cpp:104-108). Server-only by construction: this is
	// only ever reached past the NM_Client guard, and only an authority world has an AuthorityGameMode.
	FString DistrictName;
	if (!DistrictOptionName.IsEmpty())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			if (const AGameModeBase* GameMode = GameState->AuthorityGameMode)
			{
				DistrictName = UGameplayStatics::ParseOption(GameMode->OptionsString, DistrictOptionName);
			}
		}
	}

	if (!DistrictName.IsEmpty())
	{
		// Resolve by NAME against THIS world's manager. The action holds no district list, so it cannot go
		// stale against a map, and the same experience can drive any district.
		//
		// ⚠ THE NAME WE MATCH IS THE **ASSET SHORT NAME** ("District_Duel"), NOT THE INSTANCE NAME.
		// This was originally written as GetDataLayerInstanceFromName and was DEAD ON ARRIVAL: for an
		// asset-based layer the instance's own FName is a fresh GUID --
		//   UDataLayerInstanceWithAsset::MakeName -> FName(Format("DataLayer_{0}", FGuid::NewGuid()))
		//   (DataLayerInstanceWithAsset.cpp:51-54)
		// -- and AWorldDataLayers::GetDataLayerInstance compares ONLY against GetDataLayerFName()
		// (WorldDataLayers.cpp:1013-1022). So every ?District= lookup returned nullptr, in every world.
		// GetDataLayerShortName() is the engine accessor that returns the asset's name
		// (DataLayerInstanceWithAsset.h:49), which is the thing a playlist DA can reasonably carry.
		// One pass, matching EITHER the short name ("District_Duel") or a full asset path
		// ("/Game/Maps/DataLayers/L_ShantyTown/District_Duel[.District_Duel]"). Short name is the intended
		// form; the path is accepted so a pasted reference is not a mystery failure.
		//
		// Both accessors are public on UDataLayerInstance (DataLayerInstance.h:130,133). The manager's own
		// GetDataLayerInstanceFromAssetName would be the obvious call and is PRIVATE (DataLayerManager.h:144)
		// -- hence doing the comparison here rather than delegating.
		const UDataLayerInstance* Instance = nullptr;
		int32 NameMatches = 0;
		Manager->ForEachDataLayerInstance([&DistrictName, &Instance, &NameMatches](UDataLayerInstance* It)
		{
			if (It == nullptr)
			{
				return true;
			}
			const FString Full = It->GetDataLayerFullName();
			FString FullNoObject = Full;
			int32 Dot = INDEX_NONE;
			if (Full.FindChar(TEXT('.'), Dot))
			{
				FullNoObject = Full.Left(Dot);
			}

			if (It->GetDataLayerShortName().Equals(DistrictName, ESearchCase::IgnoreCase)
				|| Full.Equals(DistrictName, ESearchCase::IgnoreCase)
				|| FullNoObject.Equals(DistrictName, ESearchCase::IgnoreCase))
			{
				++NameMatches;
				if (Instance == nullptr) { Instance = It; }
			}
			return true;
		});

		// Ambiguity is a map-authoring error, not a runtime one, but it must not resolve arbitrarily: two
		// assets in different folders can share a leaf name. Take none rather than a coin flip.
		if (NameMatches > 1)
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_DISTRICT[%s]: ?%s=%s is AMBIGUOUS in world '%s' -- %d data layers match that name. "
				     "Refusing to guess. Rename the assets or pass a full asset path."),
				Phase, *DistrictOptionName, *DistrictName, *GetNameSafe(World), NameMatches);
			return Out;
		}

		// Last: a literal instance name. Only non-asset (private/deprecated) instances have a usable one,
		// but matching it costs nothing and keeps legacy maps working.
		if (Instance == nullptr)
		{
			Instance = Manager->GetDataLayerInstanceFromName(FName(*DistrictName));
		}

		const UDataLayerAsset* Asset = Instance ? Instance->GetAsset() : nullptr;

		if (Asset != nullptr)
		{
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_DISTRICT[%s]: district from URL ?%s=%s -> layer '%s' (world '%s')."),
				Phase, *DistrictOptionName, *DistrictName, *Asset->GetName(), *GetNameSafe(World));
			Out.Add(Asset);
			return Out;
		}

		// THE OPTION WAS GIVEN AND DID NOT RESOLVE. This is the dangerous case: falling back silently would
		// stream the WRONG district (or none) while the log looked ordinary, and the queue promised a size.
		// Refuse the fallback and say exactly what was asked for and what the map actually has.
		// SHORT names, deliberately: this list exists to tell a reader what to put in ExtraArgs, and it is
		// matched by the resolver above. GetDataLayerFName() here would print a column of
		// "DataLayer_<guid>" -- true, useless, and the reason the original lookup bug was invisible.
		FString Available;
		Manager->ForEachDataLayerInstance([&Available](UDataLayerInstance* It)
		{
			if (It) { Available += (Available.IsEmpty() ? TEXT("") : TEXT(", ")); Available += It->GetDataLayerShortName(); }
			return true;
		});
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_DISTRICT[%s]: ?%s=%s named a data layer that world '%s' does NOT have%s. "
			     "NOT falling back -- the wrong district is worse than none. Available: [%s]"),
			Phase, *DistrictOptionName, *DistrictName, *GetNameSafe(World),
			Instance ? TEXT(" (instance found but it carries no asset)") : TEXT(""), *Available);
		return Out;
	}

	// --- 2. FALLBACK: the authored list. A hand-opened map, PIE, or a pre-R60 config. ---
	if (DataLayers.IsEmpty())
	{
		// Log, NOT Warning. This action lives on the AFLCore GameFeatureData, so it runs in EVERY world --
		// ARCANEON, NANOWATT, Duel_01 and the front end included -- and only district maps carry the option.
		// "No district here" is therefore the ordinary case for most maps, and a warning on the ordinary
		// case is how a log becomes unreadable and a real warning becomes invisible.
		//
		// The failure that DOES matter -- an option given that does not resolve -- is an Error above, and
		// deliberately refuses the fallback.
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AFL_DISTRICT[%s]: no ?%s= option and no fallback layers -- nothing to activate in world '%s' "
			     "(expected on a map without districts)."),
			Phase, *DistrictOptionName, *GetNameSafe(World));
		return Out;
	}

	for (const TSoftObjectPtr<UDataLayerAsset>& SoftAsset : DataLayers)
	{
		if (SoftAsset.IsNull())
		{
			UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_DISTRICT[%s]: a null DataLayer entry was skipped."), Phase);
			continue;
		}
		const UDataLayerAsset* Asset = SoftAsset.LoadSynchronous();
		if (Asset == nullptr)
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_DISTRICT[%s]: could not load DataLayerAsset '%s'."), Phase, *SoftAsset.ToString());
			continue;
		}
		Out.Add(Asset);
	}

	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_DISTRICT[%s]: no ?%s= option -- using %d authored fallback layer(s) in world '%s'."),
		Phase, *DistrictOptionName, Out.Num(), *GetNameSafe(World));
	return Out;
}

#if WITH_EDITOR
EDataValidationResult UAFLGFA_ActivateDataLayers::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// AN EMPTY LIST IS NOW THE INTENDED SHAPE (R60): the district arrives as a ?District= URL option from the
	// playlist DA's ExtraArgs, and an entry here would be a district baked into an experience. Only the case
	// where NEITHER source can supply anything is an error.
	if (DataLayers.IsEmpty() && DistrictOptionName.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoLayerSource",
			"No fallback DataLayers AND no DistrictOptionName -- this action has no way to learn which "
			"district to activate, so it would do nothing. Set the option name (normally 'District') or, "
			"for a hand-opened map only, add a fallback layer."));
		Result = EDataValidationResult::Invalid;
	}
	else if (!DataLayers.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("BakedDistrict",
			"Fallback DataLayers are set. These are used ONLY when the URL carries no ?District= option. A "
			"matchmade session supplies the district per match (R60) -- a baked district binds size to "
			"experience, which needs one asset per size x ruleset x league."));
	}

	int32 Index = 0;
	for (const TSoftObjectPtr<UDataLayerAsset>& SoftAsset : DataLayers)
	{
		if (SoftAsset.IsNull())
		{
			Context.AddError(FText::Format(
				LOCTEXT("NullDataLayer", "DataLayers[{0}] is empty."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
		++Index;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
