// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLGameplayAbility_DamageTest.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "Attributes/AFLAttributeSet_Combat.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameplayAbility_DamageTest)


// SetByCaller magnitude tags. Authored in AFLCoreTags.ini (Stage 1).
// Consumed by UAFLDamageExecCalc::Execute_Implementation step 2.
static const FName NAME_Data_Damage_Headshot  = TEXT("Data.Damage.Headshot");
static const FName NAME_Data_Damage_Weakpoint = TEXT("Data.Damage.Weakpoint");
static const FName NAME_Data_Damage_Distance  = TEXT("Data.Damage.Distance");


UAFLGameplayAbility_DamageTest::UAFLGameplayAbility_DamageTest()
{
	// Server-only activation; client side is a no-op. Matches the ExecCalc's
	// WITH_SERVER_CODE fence — there is no point activating client-side because
	// the damage write is rejected by ApplyModToAttribute on non-authority anyway.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UAFLGameplayAbility_DamageTest::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	if (!DamageEffectClass)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("UAFLGameplayAbility_DamageTest activated with no DamageEffectClass set; ending."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 1. Write BaseDamage to Source.Damage. ApplyModToAttribute server-gates internally;
	// this call is a no-op on clients (ServerOnly NetExecutionPolicy makes that
	// path unreachable, but the engine guard is the real safety net).
	ASC->ApplyModToAttribute(
		UAFLAttributeSet_Combat::GetDamageAttribute(),
		EGameplayModOp::Override,
		BaseDamage);

	// 2. Build the damage spec.
	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddInstigator(ActorInfo->OwnerActor.Get(), ActorInfo->AvatarActor.Get());

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), ContextHandle);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("UAFLGameplayAbility_DamageTest: MakeOutgoingSpec returned invalid handle; ending."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 3. Inject SetByCaller multipliers. ExecCalc reads these with default 1.0f
	// when absent — we always set them explicitly for predictable test runs.
	FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Headshot,  false), HeadshotMultiplier);
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Weakpoint, false), WeakpointMultiplier);
	Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_Data_Damage_Distance,  false), DistanceFalloff);

	// 4. Apply self-target. The ExecCalc will capture Source.Damage (still set
	// to BaseDamage from step 1), compute Shield/Health deltas, emit output
	// modifiers, and the AttributeSet's PostGameplayEffectExecute will zero
	// Source.Damage after consumption.
	ASC->ApplyGameplayEffectSpecToSelf(Spec);

	UE_LOG(LogAFLCombat, Log,
		TEXT("DamageTest applied: Base=%.1f Headshot=%.2f Weakpoint=%.2f Distance=%.2f"),
		BaseDamage, HeadshotMultiplier, WeakpointMultiplier, DistanceFalloff);

	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}
