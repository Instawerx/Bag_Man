// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Heat_BeamTick.h"

#include "Attributes/AFLAttributeSet_Combat.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Heat_BeamTick)

// Native tag — same module-load-vs-ini-scan rationale as the rest of the
// AFLCombat GEs. UE_DEFINE_GAMEPLAY_TAG_STATIC at file scope registers
// before any CDO of this class is constructed (post-2026-05-20 CDO pattern).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Combat_HeatVentingStart, "Event.Combat.HeatVentingStart");


UGE_AFL_Heat_BeamTick::UGE_AFL_Heat_BeamTick()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Override HeatPerBeamTick meta with 4.0. AttributeSet's
	// PostGameplayEffectExecute folds this into the persistent Heat
	// attribute, clamps to MaxHeat, and grants State.Overheated when
	// Heat hits MaxHeat. 12.5 ticks @ 100ms = ~1.25s to overheat from 0.
	FGameplayModifierInfo HeatMod;
	HeatMod.Attribute = UAFLAttributeSet_Combat::GetHeatPerBeamTickAttribute();
	HeatMod.ModifierOp = EGameplayModOp::Override;
	HeatMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(4.0f));
	Modifiers.Add(HeatMod);

	// Grant Event.Combat.HeatVentingStart on application. Same in-ctor
	// CreateDefaultSubobject + GEComponents.Add pattern as the damage GEs
	// (templated AddComponent<> isn't legal during a UObject constructor).
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_Event_Combat_HeatVentingStart);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);
}
