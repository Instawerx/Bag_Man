// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_Roll.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_Roll)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Roll_State_Match_Warmup, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Roll_State_Match_Ended, "State.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Roll_State_Extracting, "State.Extracting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Roll_State_Movement_Climbing, "State.Movement.Climbing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Roll_State_Movement_Vaulting, "State.Movement.Vaulting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Roll_State_Carrying, "State.Carrying");

UAFLGameplayAbility_Roll::UAFLGameplayAbility_Roll(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(TAG_Roll_State_Match_Warmup);
	ActivationBlockedTags.AddTag(TAG_Roll_State_Match_Ended);
	ActivationBlockedTags.AddTag(TAG_Roll_State_Extracting);
	ActivationBlockedTags.AddTag(TAG_Roll_State_Movement_Climbing); // can't roll while climbing
	ActivationBlockedTags.AddTag(TAG_Roll_State_Movement_Vaulting); // or mid-vault
	ActivationBlockedTags.AddTag(TAG_Roll_State_Carrying);          // or while carrying
}

void UAFLGameplayAbility_Roll::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bExiting = false;
	MontageTask = nullptr;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FName Section = ResolveRollSection();
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_ROLL: activate by %s (section=%s)."), *GetNameSafe(Character), *Section.ToString());

	// Apply the roll-active GE -> grants State.Movement.Rolling (gates conflicts; drives AI/anim).
	if (RollActiveEffectClass)
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(RollActiveEffectClass, GetAbilityLevel());
		if (SpecHandle.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// RESERVED i-frames (off unless design enables): a duration GE granting State.Invulnerable for its own window.
	if (bGrantIFrames && IFrameEffectClass)
	{
		const FGameplayEffectSpecHandle IFrameSpec = MakeOutgoingGameplayEffectSpec(IFrameEffectClass, GetAbilityLevel());
		if (IFrameSpec.IsValid())
		{
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, IFrameSpec);
			UE_LOG(LogAFLMovement, Log, TEXT("AFL_ROLL: i-frames granted."));
		}
	}

	// Play the root-motion roll montage at the resolved directional section.
	if (RollMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, RollMontage, /*Rate*/ 1.0f, /*StartSection*/ Section, /*bStopWhenAbilityEnds*/ true);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UAFLGameplayAbility_Roll::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UAFLGameplayAbility_Roll::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UAFLGameplayAbility_Roll::OnMontageInterruptedOrCancelled);
			MontageTask->OnCancelled.AddDynamic(this, &UAFLGameplayAbility_Roll::OnMontageInterruptedOrCancelled);
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		UE_LOG(LogAFLMovement, Log, TEXT("AFL_ROLL: no RollMontage set (placeholder)."));
		ExitRoll(TEXT("no-montage"), /*bCancelled*/ true);
	}
}

FName UAFLGameplayAbility_Roll::ResolveRollSection() const
{
	const ACharacter* Character = GetCurrentActorInfo() ? Cast<ACharacter>(GetCurrentActorInfo()->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		return ForwardSection;
	}

	FVector Input = Character->GetLastMovementInputVector();
	Input.Z = 0.0f;
	if (Input.SizeSquared() < DirectionDeadzone * DirectionDeadzone)
	{
		return ForwardSection; // no clear directional intent -> forward dive
	}
	Input.Normalize();

	FVector Fwd = Character->GetActorForwardVector();
	Fwd.Z = 0.0f;
	Fwd.Normalize();
	FVector Right = Character->GetActorRightVector();
	Right.Z = 0.0f;
	Right.Normalize();

	const float FwdDot = FVector::DotProduct(Input, Fwd);
	const float RightDot = FVector::DotProduct(Input, Right);

	// Dominant axis picks the 4-way section; sign picks the direction. Facing is preserved (combat dodge).
	if (FMath::Abs(FwdDot) >= FMath::Abs(RightDot))
	{
		return (FwdDot >= 0.0f) ? ForwardSection : BackwardSection;
	}
	return (RightDot >= 0.0f) ? RightSection : LeftSection;
}

void UAFLGameplayAbility_Roll::OnMontageCompleted()
{
	ExitRoll(TEXT("complete"), /*bCancelled*/ false);
}

void UAFLGameplayAbility_Roll::OnMontageInterruptedOrCancelled()
{
	ExitRoll(TEXT("interrupted"), /*bCancelled*/ true);
}

void UAFLGameplayAbility_Roll::ExitRoll(const TCHAR* Reason, bool bCancelled)
{
	if (bExiting)
	{
		return;
	}
	bExiting = true;
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_ROLL: exit (reason=%s)."), Reason);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		/*bReplicateEndAbility*/ true, bCancelled);
}

void UAFLGameplayAbility_Roll::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Remove the roll-active GE so State.Movement.Rolling clears. (The i-frame GE, if any, is duration-based
	// and auto-removes on its own window -- we do NOT force-clear it here so its i-frames aren't cut short.)
	if (RollActiveEffectClass && ActorInfo)
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(RollActiveEffectClass, ASC);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
