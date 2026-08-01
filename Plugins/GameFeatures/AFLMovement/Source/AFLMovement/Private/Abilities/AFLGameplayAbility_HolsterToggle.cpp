// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_HolsterToggle.h"

#include "AFLMovement.h"
#include "GameFramework/Character.h"
#include "Interaction/AFLHolsterComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_HolsterToggle)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_HolsterReason_Manual, "HolsterReason.Manual");

UAFLGameplayAbility_HolsterToggle::UAFLGameplayAbility_HolsterToggle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UAFLGameplayAbility_HolsterToggle::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAFLHolsterComponent* Holster = Character ? Character->FindComponentByClass<UAFLHolsterComponent>() : nullptr;
	if (Holster)
	{
		// Toggle the MANUAL reason. Server-guarded inside the component -> the server toggle is authoritative;
		// the owning-client call no-ops and the result arrives via replication (OnRep_Holstered).
		if (Holster->IsHolstered())
		{
			Holster->RemoveHolsterReason(TAG_HolsterReason_Manual);
		}
		else
		{
			Holster->AddHolsterReason(TAG_HolsterReason_Manual);
		}
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}
