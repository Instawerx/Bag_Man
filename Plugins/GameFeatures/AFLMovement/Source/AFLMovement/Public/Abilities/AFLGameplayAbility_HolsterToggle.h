// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_HolsterToggle.generated.h"

/**
 * UAFLGameplayAbility_HolsterToggle  (Movement Overhaul — Phase 3: manual holster toggle)
 *
 * A thin input ability that toggles the MANUAL holster reason on UAFLHolsterComponent: if currently holstered
 * it removes HolsterReason.Manual (draw), else it adds it (sheath). The component (server-auth, refcounted)
 * owns the truth + the weapon reattach; this ability just owns the input. Because it's LocalPredicted the
 * server runs it too, and AddHolsterReason/RemoveHolsterReason are server-guarded, so the server's toggle is
 * authoritative and clients see the result via replication.
 *
 * InstancedPerActor, LocalPredicted, OnInputTriggered. (An equip/unequip montage is a later polish add.)
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_HolsterToggle : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_HolsterToggle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
