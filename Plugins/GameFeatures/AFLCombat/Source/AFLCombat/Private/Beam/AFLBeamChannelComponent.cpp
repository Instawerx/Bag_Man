// Copyright C12 AI Gaming. All Rights Reserved.

#include "Beam/AFLBeamChannelComponent.h"

#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBeamChannelComponent)

UAFLBeamChannelComponent::UAFLBeamChannelComponent()
{
	// No tick: the component is a passive data carrier. The ability writes it
	// (timer-driven) and the cue reads it (actor-tick-driven). Replication does
	// the rest. Replicated so simulated proxies receive the endpoint.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAFLBeamChannelComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate to ALL clients (no COND_OwnerOnly): every client that witnesses
	// the looping beam cue needs the endpoint to drive its local Niagara. This is
	// the cosmetic-correctness the cue architecture exists for.
	DOREPLIFETIME(UAFLBeamChannelComponent, BeamImpactPoint);
	DOREPLIFETIME(UAFLBeamChannelComponent, BeamMuzzleLocation);
	DOREPLIFETIME(UAFLBeamChannelComponent, bBeamActive);
}

void UAFLBeamChannelComponent::PublishImpact(const FVector& WorldImpactPoint)
{
	BeamImpactPoint = WorldImpactPoint;
	bBeamActive     = true;
}

void UAFLBeamChannelComponent::PublishMuzzle(const FVector& WorldMuzzleLocation)
{
	BeamMuzzleLocation = WorldMuzzleLocation;
}

void UAFLBeamChannelComponent::SetBeamActive(bool bInActive)
{
	bBeamActive = bInActive;
}
