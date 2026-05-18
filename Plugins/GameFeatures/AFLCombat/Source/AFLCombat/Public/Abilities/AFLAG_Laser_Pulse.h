// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLAG_Laser_Pulse.generated.h"

class UGameplayEffect;


/**
 * UAFLAG_Laser_Pulse
 *
 * Foundation for the Pulse weapon (AFL-0104). Locally-predicted, single-shot
 * laser ability bound to InputTag.Weapon.Fire (binding lands in AFL-0107).
 *
 * Scope of this task: declare the class, policies, BP-facing DamageEffectClass,
 * and an ActivateAbility skeleton that commits cost/cooldown and logs
 * `AFL_PULSE: Activate`. The hitscan trace lands in AFL-0106; the damage GE
 * lands in AFL-0105.
 */
UCLASS()
class AFLCOMBAT_API UAFLAG_Laser_Pulse : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:

	UAFLAG_Laser_Pulse();

protected:

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/**
	 * GameplayEffect that applies Pulse damage on hit. Wired by AFL-0105 to
	 * GE_AFL_Damage_Pulse. Left nullptr at this stage — ActivateAbility's
	 * trace dispatch (AFL-0106) is responsible for spec creation and apply.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Pulse")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
};
