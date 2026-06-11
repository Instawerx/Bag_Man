// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Throw.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/AFLGameplayAbility_Grab.h"
#include "Effects/GE_AFL_ThrowRecovery.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/AFLInteractionComponent.h"
#include "NativeGameplayTags.h"

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

	// Aim = controller view direction, FULL 3D (pitch throws up-slope / down-slope). Never
	// GetPlayerViewPoint (AFL lint rail). SERVER-SIDE VALIDATION: RESOLVED BY ARCHITECTURE (2-client
	// cycle 1, D-F4): no claimed dir ever ships -- each LocalPredicted instance recomputes from ITS OWN
	// ControlRotation, and the authoritative impulse (ReleaseActor physics, authority-gated) uses the
	// SERVER instance's recompute. The client's dir drives only its local cosmetic prediction. The
	// residual trust surface is ControlRotation replication itself (shared with ALL aiming); rotation-
	// rate sanity folds into the shared AFL-0213 per-pawn budget when that lands.
	const APlayerController* PC = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	const FVector AimDir = PC ? PC->GetControlRotation().Vector()
	                          : (Avatar ? Avatar->GetActorForwardVector() : FVector::ForwardVector);

	AActor* Thrown = Interaction->GetCarriedActor();
	Interaction->ReleaseActor(EAFLReleaseMode::Throw, AimDir);
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_THROW: threw %s along %s."),
		*GetNameSafe(Thrown), *AimDir.ToCompactString());

	// THROW RECOVERY (PIE-caught, rounds 2+3): the press/hold that threw must not also fire the weapon.
	// Input-side scoping is unreliable -- IA_Weapon_Fire's trigger completes ONE FRAME after the press even
	// while the button stays held (the climb WaitInputRelease trap's root cause), so an InputReleased-keyed
	// lifetime collapses instantly and the WhileInputActive beam channels two frames after the throw. The
	// robust gate is TIME-based and GAS-canonical: a 0.4s Duration GE granting State.Weapon.ThrowRecovery,
	// which all three fire abilities block on (alongside State.Carrying). Applied BEFORE the grab teardown
	// so there is no uncovered frame.
	{
		const FGameplayEffectSpecHandle RecoverySpec =
			MakeOutgoingGameplayEffectSpec(UGE_AFL_ThrowRecovery::StaticClass(), GetAbilityLevel());
		if (RecoverySpec.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, RecoverySpec);
		}
	}

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

	// Fire-and-forget: the recovery GE above owns the post-throw gate (no timers, no input dependence),
	// so the ability ends immediately and cleanly.
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}
