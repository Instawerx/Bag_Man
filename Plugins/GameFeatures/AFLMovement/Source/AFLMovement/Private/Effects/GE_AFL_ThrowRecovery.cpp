// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_ThrowRecovery.h"

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_ThrowRecovery)

// Native tag -- same module-load-vs-ini-scan rationale as the rest of the AFL tags.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Weapon_ThrowRecovery_GE, "State.Weapon.ThrowRecovery");


UGE_AFL_ThrowRecovery::UGE_AFL_ThrowRecovery()
{
	// 0.4s covers the observed throw->fire gap (~2 frames) with margin, while keeping a deliberate
	// hold-to-fire after a throw feeling responsive. Mirror of the GE_AFL_Cooldown_* ctor pattern.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.4f));

	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_State_Weapon_ThrowRecovery_GE);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
