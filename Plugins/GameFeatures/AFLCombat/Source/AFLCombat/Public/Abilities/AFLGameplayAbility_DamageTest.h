// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_DamageTest.generated.h"

class UGameplayEffect;


/**
 * UAFLGameplayAbility_DamageTest
 *
 * Sprint 1 / AFL-0104 smoke-test ability. Self-applies the AFLCombat
 * damage GameplayEffect with designer-tunable BaseDamage and SetByCaller
 * multipliers (Headshot, Weakpoint, DistanceFalloff). Exists to validate
 * the SSOT §8.3 pipeline end-to-end in PIE.
 *
 * Activation path (per Block B test plan): granted via DA_AFL_Combat_AbilitySet
 * and triggered in PIE via the AbilitySystem.Ability.Activate console command.
 * No permanent input binding.
 *
 * Damage-set approach: writes BaseDamage directly to Source.Damage via
 * UAbilitySystemComponent::ApplyModToAttribute (Override). The ExecCalc
 * captures Source.Damage and zeros it in PostGameplayEffectExecute. Approved
 * via engine-source audit: HideFromModifiers is a UI filter on the GE
 * modifier picker, not a runtime block on ApplyModToAttribute writes.
 */
UCLASS()
class AFLCOMBAT_API UAFLGameplayAbility_DamageTest : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:

	UAFLGameplayAbility_DamageTest();

protected:

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** GameplayEffect class that runs UAFLDamageExecCalc. Set on the CDO; defaults to GE_AFL_Damage_Instant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Test")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Source.Damage value written before the GE is applied. Captured by the ExecCalc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Test", meta=(ClampMin="0.0"))
	float BaseDamage = 25.0f;

	/** Multiplier applied as SetByCaller(Data.Damage.Headshot). 1.0 = no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Test", meta=(ClampMin="0.0"))
	float HeadshotMultiplier = 1.0f;

	/** Multiplier applied as SetByCaller(Data.Damage.Weakpoint). 1.0 = no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Test", meta=(ClampMin="0.0"))
	float WeakpointMultiplier = 1.0f;

	/** Multiplier applied as SetByCaller(Data.Damage.Distance). 1.0 = no falloff. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Test", meta=(ClampMin="0.0"))
	float DistanceFalloff = 1.0f;
};
