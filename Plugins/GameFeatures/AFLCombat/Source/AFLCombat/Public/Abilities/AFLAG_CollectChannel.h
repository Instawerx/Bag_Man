// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Abilities/AFLGameplayAbility_Channel.h"

#include "AFLAG_CollectChannel.generated.h"

/**
 * UAFLAG_CollectChannel  (Loot-Carry Phase B -- the CARRY collect-channel)
 *
 * A short, interruptible stand-and-channel that collects a CARRY loot cache into the carried pool --
 * the "friction, not hand-occupy" CARRY mechanic (v3 decision 3). Subclasses the proven channel base
 * (UAFLGameplayAbility_Channel); the only specialization is the payoff: on complete, the channeled
 * cache's UAFLLootGrantComponent::TryGrant pays the channeler (with the cache configured
 * CarryToExtractEnergy -> the value lands in UAFLLootCarryComponent's pool, not the wallet).
 *
 * TRIGGER (Decision D): GameplayEvent. The grab ability (UAFLGameplayAbility_Grab), on a CollectLoot
 * grabbable, sends Event.Loot.CollectChannel with Target = the cache -> this ability triggers + channels.
 */
UCLASS()
class AFLCOMBAT_API UAFLAG_CollectChannel : public UAFLGameplayAbility_Channel
{
	GENERATED_BODY()

public:
	UAFLAG_CollectChannel();

protected:
	virtual void OnChannelComplete(const AActor* TargetActor) override;
};
