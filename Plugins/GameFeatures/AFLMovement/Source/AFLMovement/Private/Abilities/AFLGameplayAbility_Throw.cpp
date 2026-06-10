// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Throw.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/AFLGameplayAbility_Grab.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/AFLInteractionComponent.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Throw)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Throw_State_Carrying, "State.Carrying");

UAFLGameplayAbility_Throw::UAFLGameplayAbility_Throw(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Match the grab GA's lifecycle shape.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

	// The whole gate: no carry, no throw. While carrying, this ability owns the shared input; after the
	// throw the tag clears (grab GA teardown removes the carry GE) and the input returns to the weapon.
	ActivationRequiredTags.AddTag(TAG_Throw_State_Carrying);

	// Hold the gate OURSELVES for the activation's lifetime: the throw tears the carry GE down
	// mid-input-pass, and without this the pulse ability (same LMB press, same frame, later in Lyra's
	// ProcessAbilityInput loop) sees the gate vanish and fires from the very press that threw -- the
	// PIE-observed "threw AND fired". With the owned tag + the one-tick deferred end below, the carry
	// block holds through the whole frame regardless of spec iteration order.
	ActivationOwnedTags.AddTag(TAG_Throw_State_Carrying);
}

void UAFLGameplayAbility_Throw::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAFLInteractionComponent* Interaction = Avatar ? Avatar->FindComponentByClass<UAFLInteractionComponent>() : nullptr;
	if (!Interaction || !Interaction->IsCarrying())
	{
		// State.Carrying gated activation, but belt-and-braces: the carry can end the same frame.
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_THROW: no carried actor -> cancel."));
		CancelAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateCancelAbility*/ true);
		return;
	}

	// Aim = controller view direction, FULL 3D (pitch throws up-slope / down-slope). Ability-side CLIENT
	// aim -- never GetPlayerViewPoint (AFL lint rail). Server-side validation of the claimed throw ray is a
	// NAMED FUTURE item (netcode pass; the hitscan claimed-ray pattern is the template).
	const APlayerController* PC = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	const FVector AimDir = PC ? PC->GetControlRotation().Vector()
	                          : (Avatar ? Avatar->GetActorForwardVector() : FVector::ForwardVector);

	AActor* Thrown = Interaction->GetCarriedActor();
	Interaction->ReleaseActor(EAFLReleaseMode::Throw, AimDir);
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_THROW: threw %s along %s."),
		*GetNameSafe(Thrown), *AimDir.ToCompactString());

	// Tear the carry down through the grab ability's single EndAbility funnel (hold-pose stop +
	// State.Carrying GE removal + its lifetime). Cancel by CLASS -- deterministic, no dependence on
	// BP-authored AbilityTags and no new event tag. The grab's own EndAbility ReleaseActor() call no-ops:
	// CarriedActor was cleared by the release above (the !Target early-return IS the double-release guard,
	// and the rifle restore already ran inside this throw's full release path).
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.IsActive() && Spec.Ability->IsA<UAFLGameplayAbility_Grab>())
			{
				ASC->CancelAbilityHandle(Spec.Handle);
			}
		}
	}

	// End NEXT TICK, not now: our ActivationOwnedTags keep State.Carrying alive through the remainder of
	// THIS frame's input pass (the carry GE is already gone via the grab teardown above), so the fire
	// abilities' carry-block still holds for the same press that threw. The one-frame extra lifetime is
	// invisible; a fresh LMB press after the throw fires the weapon normally (the tag drops with us).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
			[this]()
			{
				EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
					/*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
			}));
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
	}
}
