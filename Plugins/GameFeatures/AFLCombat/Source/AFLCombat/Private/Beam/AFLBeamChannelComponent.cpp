// Copyright C12 AI Gaming. All Rights Reserved.

#include "Beam/AFLBeamChannelComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBeamChannelComponent)

namespace
{
	/**
	 * The ONE left-hand key (Block 22). Exactly this socket maps to slot 1; every other value --
	 * including NAME_None from an actor that hasn't been attached yet -- falls to slot 0.
	 *
	 * Verified against every shipped definition at authoring time: 60 of 62 WID_AFL_* attach at
	 * "weapon_r", the right cannon at "weapon_lowerarm_r", and ONLY the left cannon at the name
	 * below. So slot 1 is opt-in by data and nothing existing can drift into it.
	 */
	const FName GAFLLeftCannonAttachSocket(TEXT("weapon_lowerarm_l"));
}

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

	// Slot 1 -- same unconditional replication as slot 0, for the same reason: every client that
	// witnesses the second beam needs its endpoint to drive the local Niagara.
	DOREPLIFETIME(UAFLBeamChannelComponent, BeamImpactPointSlot1);
	DOREPLIFETIME(UAFLBeamChannelComponent, BeamMuzzleLocationSlot1);
	DOREPLIFETIME(UAFLBeamChannelComponent, bBeamActiveSlot1);
}

int32 UAFLBeamChannelComponent::ResolveBeamSlotForActor(const AActor* WeaponDisplayActor)
{
	// Single key, single comparison. Not a table -- see the header: anything unrecognised is slot 0,
	// which is what makes every existing weapon non-regressive without enumerating them.
	if (!WeaponDisplayActor)
	{
		return 0;
	}
	return (WeaponDisplayActor->GetAttachParentSocketName() == GAFLLeftCannonAttachSocket) ? 1 : 0;
}

void UAFLBeamChannelComponent::PublishImpact(const FVector& WorldImpactPoint, int32 Slot)
{
	// Slot 0 path is byte-identical to the pre-slot implementation (impact + implicit active flag).
	if (Slot == 1)
	{
		BeamImpactPointSlot1 = WorldImpactPoint;
		bBeamActiveSlot1     = true;
		return;
	}
	BeamImpactPoint = WorldImpactPoint;
	bBeamActive     = true;
}

void UAFLBeamChannelComponent::PublishMuzzle(const FVector& WorldMuzzleLocation, int32 Slot)
{
	if (Slot == 1)
	{
		BeamMuzzleLocationSlot1 = WorldMuzzleLocation;
		return;
	}
	BeamMuzzleLocation = WorldMuzzleLocation;
}

void UAFLBeamChannelComponent::SetBeamActive(bool bInActive, int32 Slot)
{
	if (Slot == 1)
	{
		bBeamActiveSlot1 = bInActive;
		return;
	}
	bBeamActive = bInActive;
}
