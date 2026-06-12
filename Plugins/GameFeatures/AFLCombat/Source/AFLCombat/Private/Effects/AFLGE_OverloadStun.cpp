// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_OverloadStun.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_OverloadStun)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Overloaded_StunGE, "State.Overloaded");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Gameplay_MovementStopped_StunGE, "Gameplay.MovementStopped");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Carrying_Vulnerable_StunGE, "State.Carrying.Vulnerable");

namespace
{
	constexpr float OverloadStunSeconds = 1.0f;
}

UAFLGE_OverloadStun::UAFLGE_OverloadStun()
{
	// Fixed ~1s window. SetByCaller-free: a constant duration on the CDO (a BP child could override).
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(OverloadStunSeconds));

	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_State_Overloaded_StunGE);
	Granted.Added.AddTag(TAG_Gameplay_MovementStopped_StunGE);
	Granted.Added.AddTag(TAG_State_Carrying_Vulnerable_StunGE);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
