// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"

#include "AFLGE_DashCooldown.generated.h"

/**
 * UAFLGE_DashCooldown
 *
 * Sprint 3 Dash Movement Contract — see §9.6 of Docs/BAG_MAN_MASTER_BUILD_v2.0.md.
 *
 * Duration GE (3.0s) that grants Cooldown.Movement.Dash. Sole source of dash
 * cooldown state; the GA does not own cooldown through timers. ULyraGameplayAbility's
 * CooldownGameplayEffectClass should point at this class so that CommitAbility
 * applies it and CanActivateAbility checks the cooldown tag.
 */
UCLASS()
class AFLMOVEMENT_API UAFLGE_DashCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAFLGE_DashCooldown(const FObjectInitializer& ObjectInitializer);
};
