// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Heat_Decay.h"

#include "Attributes/AFLAttributeSet_Combat.h"
#include "GameplayEffectAttributeCaptureDefinition.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Heat_Decay)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Combat_CoolingGate_Decay, "State.Combat.CoolingGate");


UGE_AFL_Heat_Decay::UGE_AFL_Heat_Decay()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 10 Hz tick. The modifier list executes per period — not on the initial
	// apply — so the very first tick lands 100ms after the GE is granted.
	Period = FScalableFloat(0.1f);
	bExecutePeriodicEffectOnApplication = false;

	// Modifier: Heat += (-0.1 * HeatDecayRate). HeatDecayRate is captured on
	// the target (self-applied, so Target == Source) and is NOT snapshotted —
	// designer-tuned changes via SetCombatAttribute / GE author propagate live.
	FAttributeBasedFloat HeatDelta;
	HeatDelta.Coefficient                = FScalableFloat(-0.1f);
	HeatDelta.PreMultiplyAdditiveValue   = FScalableFloat(0.0f);
	HeatDelta.PostMultiplyAdditiveValue  = FScalableFloat(0.0f);
	HeatDelta.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(
		UAFLAttributeSet_Combat::GetHeatDecayRateAttribute(),
		EGameplayEffectAttributeCaptureSource::Target,
		/*bSnapshot=*/false);
	HeatDelta.AttributeCalculationType = EAttributeBasedFloatCalculationType::AttributeMagnitude;

	FGameplayModifierInfo HeatMod;
	HeatMod.Attribute    = UAFLAttributeSet_Combat::GetHeatAttribute();
	HeatMod.ModifierOp   = EGameplayModOp::Additive;
	HeatMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(HeatDelta);
	Modifiers.Add(HeatMod);

	// OngoingTagRequirements.IgnoreTags: while the source carries
	// State.Combat.CoolingGate the GE is paused — periodic executions are
	// suppressed without removing the active GE. The CoolingGate GE has a
	// 0.5s duration; the moment it expires this GE resumes its 100ms cadence.
	UTargetTagRequirementsGameplayEffectComponent* TagReqComp =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TagRequirementsComponent"));
	TagReqComp->OngoingTagRequirements.IgnoreTags.AddTag(TAG_State_Combat_CoolingGate_Decay);
	GEComponents.Add(TagReqComp);
}
