// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Laser_Pulse.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Laser_Pulse)


UAFLAG_Laser_Pulse::UAFLAG_Laser_Pulse()
{
	// Locally-predicted, instanced-per-actor. Matches the master doc Sec. 6/7
	// contract for client-authoritative hitscan: the firing client predicts
	// activation and the trace (AFL-0106), then ships TargetData to the server.
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// AbilityTags advertise this ability's identity (granted-by-class lookup
	// in DA_AFL_AbilitySet_*, and used by ActivationOwnedTags to apply
	// State.Firing.Pulse for the lifetime of the activation).
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Laser.Pulse")));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Firing.Pulse")));

	// Cooldown tag declared in AFLCombatTags.ini (Cooldown.Weapon.Pulse). The
	// concrete Cooldown GE is wired by the AbilitySet data asset in AFL-0214.
	// Cost GE is a placeholder until heat lands in AFL-0207; left unset here.
}

void UAFLAG_Laser_Pulse::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// Cost/cooldown check failed (cooldown active, insufficient heat once
		// AFL-0207 wires it, etc.). CommitAbility cancels the prediction key
		// for us; just bail.
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PULSE: Activate"));

	// AFL-0106 lands the client-side hitscan trace here: build an
	// FAFLAbilityTargetData_Hitscan, wrap in FGameplayAbilityTargetDataHandle,
	// ship via UAbilitySystemBlueprintLibrary::ServerSetReplicatedTargetData,
	// then apply DamageEffectClass on the server when target data arrives.
	// Do NOT call GetPlayerViewPoint server-side (AFL-0215 lint).

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
