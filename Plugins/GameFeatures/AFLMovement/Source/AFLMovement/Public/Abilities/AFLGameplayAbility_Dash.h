// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "ScalableFloat.h"

#include "AFLGameplayAbility_Dash.generated.h"

class UGameplayEffect;

/**
 * UAFLGameplayAbility_Dash
 *
 * Sprint 3 Dash Movement Contract — see §9.6 of Docs/BAG_MAN_MASTER_BUILD_v2.0.md.
 *
 * The dash gameplay ability. Owns activation, prediction, cooldown commit, and
 * the LaunchCharacter impulse. Applies UAFLGE_Dash_Active for its 0.12s window;
 * UAFLCharacterMovementComponent listens to State.Movement.Dashing (granted by
 * that GE) to swap friction/air-control.
 *
 * Sprint 3 base does NOT grant State.Invulnerable. The i-frame tag is reserved
 * in AFLCoreTags.ini but not enabled until the toggle is formally authorized
 * (see GE_AFL_Dash_Active and the §9.6 invulnerability rule LOCKED).
 *
 * Native defaults (locked):
 *   InstancingPolicy   = InstancedPerActor
 *   NetExecutionPolicy = LocalPredicted
 *   DashImpulse        = 1500.0 (FScalableFloat for future curve scaling)
 *
 * BP-configured (via GA_AFL_Dash child):
 *   CooldownGameplayEffectClass -> GE_AFL_DashCooldown_C
 *   DashActiveEffectClass       -> GE_AFL_Dash_Active_C
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_Dash : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_Dash(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UGameplayAbility interface
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	//~End of UGameplayAbility interface

	/** Launch impulse magnitude applied along the dash direction (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dash")
	FScalableFloat DashImpulse;

	/** Duration GE applied on activation — grants State.Movement.Dashing for 0.12s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dash")
	TSubclassOf<UGameplayEffect> DashActiveEffectClass;
};
