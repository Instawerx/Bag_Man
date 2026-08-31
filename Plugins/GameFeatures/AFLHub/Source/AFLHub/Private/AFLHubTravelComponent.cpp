// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHubTravelComponent.h"

#include "AFLHub.h"
#include "AFLHubDestinationVolume.h"
#include "EngineUtils.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHubTravelComponent)

UAFLHubTravelComponent::UAFLHubTravelComponent()
{
	// A Server RPC only routes on a REPLICATED component: the server creates this (the client-side
	// GFCM add is refused for replicated components -- by design), it replicates down, and the
	// client's copy carries the RPC legally. A non-replicated copy crashed ProcessRemoteFunction.
	SetIsReplicatedByDefault(true);
}

void UAFLHubTravelComponent::ServerRequestHubTravel_Implementation(FName DestinationId)
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// Re-resolve the row SERVER-SIDE from a placed door volume -- the client names a row, never an
	// action or a map. No matching enabled Travel volume on this map = refusal, loudly.
	for (TActorIterator<AAFLHubDestinationVolume> It(World); It; ++It)
	{
		FName RowId;
		EAFLHubDestinationAction Action;
		FString Payload;
		if (!It->GetTravelContract(RowId, Action, Payload) || RowId != DestinationId)
		{
			continue;
		}
		if (Action != EAFLHubDestinationAction::Travel || Payload.IsEmpty())
		{
			UE_LOG(LogAFLHub, Warning,
				TEXT("AFL_HUBTRAVEL: '%s' refused -- action=%d payload='%s' is not an enabled Travel row."),
				*DestinationId.ToString(), static_cast<int32>(Action), *Payload);
			return;
		}
		UE_LOG(LogAFLHub, Log, TEXT("AFL_HUBTRAVEL: '%s' -> ServerTravel(%s)."),
			*DestinationId.ToString(), *Payload);
		World->ServerTravel(Payload, /*bAbsolute=*/ false);
		return;
	}

	UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUBTRAVEL: no door volume for '%s' on %s -- refused."),
		*DestinationId.ToString(), *GetNameSafe(World));
}
