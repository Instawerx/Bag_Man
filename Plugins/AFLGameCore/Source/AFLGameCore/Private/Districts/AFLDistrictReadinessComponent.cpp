// Copyright C12 AI Gaming. All Rights Reserved.

#include "Districts/AFLDistrictReadinessComponent.h"

#include "AFLGameCore.h"                       // LogAFLGameCore
#include "Engine/World.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDistrictReadinessComponent)

bool UAFLDistrictReadinessComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (bReadyLatched)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UDataLayerManager* Manager = World ? UDataLayerManager::GetDataLayerManager(World) : nullptr;
	if (Manager == nullptr)
	{
		bReadyLatched = true;   // not a partitioned world; there is no district to wait for
		return false;
	}

	// A district IS "a runtime data layer that is currently Activated" -- asked that way rather than through
	// UAFLGFA_ActivateDataLayers::GetActiveDistrictForWorld deliberately, because that map is written on the
	// SERVER by the activating action and would be empty on a client. Effective runtime state, by contrast,
	// replicates outward (AWorldDataLayers::GetLifetimeReplicatedProps), so this one question is answerable
	// identically on both sides -- and the client is the side that actually has a loading screen.
	const UDataLayerInstance* ActiveDistrict = nullptr;
	Manager->ForEachDataLayerInstance([&ActiveDistrict, Manager](UDataLayerInstance* It)
	{
		const UDataLayerAsset* Asset = It ? It->GetAsset() : nullptr;
		if (Asset && Asset->IsRuntime()
			&& Manager->GetDataLayerInstanceEffectiveRuntimeState(It) == EDataLayerRuntimeState::Activated)
		{
			ActiveDistrict = It;
			return false;   // stop iterating
		}
		return true;
	});

	if (ActiveDistrict == nullptr)
	{
		// No district active. NOT latched: on the server this is also the state during the brief window
		// before the experience-loaded hook activates one, and latching here would permanently disarm the
		// component before it ever had anything to guard.
		return false;
	}

	const UWorldPartitionSubsystem* Streaming = World->GetSubsystem<UWorldPartitionSubsystem>();
	if (Streaming == nullptr || Streaming->IsStreamingCompleted())
	{
		bReadyLatched = true;
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_DISTRICT: readiness LATCHED -- '%s' is fully streamed; loading screen released for the "
			     "lifetime of world '%s'."),
			*ActiveDistrict->GetDataLayerShortName(), *GetNameSafe(World));
		return false;
	}

	OutReason = FString::Printf(TEXT("AFL district '%s' still streaming"), *ActiveDistrict->GetDataLayerShortName());
	return true;
}
