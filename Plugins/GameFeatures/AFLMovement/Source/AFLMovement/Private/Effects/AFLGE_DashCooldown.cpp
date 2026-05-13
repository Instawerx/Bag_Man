// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_DashCooldown.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_DashCooldown)

UAFLGE_DashCooldown::UAFLGE_DashCooldown(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.0f));
}
