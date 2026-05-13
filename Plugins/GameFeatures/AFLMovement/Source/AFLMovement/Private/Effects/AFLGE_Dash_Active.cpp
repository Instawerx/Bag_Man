// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_Dash_Active.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_Dash_Active)

UAFLGE_Dash_Active::UAFLGE_Dash_Active(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.12f));
}
