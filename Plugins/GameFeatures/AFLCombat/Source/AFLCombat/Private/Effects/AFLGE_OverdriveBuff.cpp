// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_OverdriveBuff.h"

#include "Attributes/AFLAttributeSet_Energy.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_OverdriveBuff)

// Native tags -- ctor-safe (module-init registration precedes CDO construction; per-file suffixes).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Energy_Overdrive_BuffGE, "State.Energy.Overdrive");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_Energy_Overdrive_BuffGE, "GameplayCue.Energy.Overdrive");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Energy_Drain_BuffGE, "Data.Energy.Drain");


UAFLGE_OverdriveBuff::UAFLGE_OverdriveBuff()
{
	// Infinite: lifetime is owned by UAFLOverdriveComponent's handle (exit at energy 0 or death) --
	// the carrier-vulnerability precedent.
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// State tag (the ThrowRecovery ctor pattern).
	UTargetTagsGameplayEffectComponent* TargetTagsComp =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer Granted;
	Granted.Added.AddTag(TAG_State_Energy_Overdrive_BuffGE);
	TargetTagsComp->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTagsComp);

	// Looping body cue: WhileActive/OnRemove ride the GE lifetime (the looping-cue doctrine --
	// OnRemove is the one cleanup site).
	GameplayCues.Add(FGameplayEffectCue(TAG_Cue_Energy_Overdrive_BuffGE, 0.0f, 0.0f));

	// Periodic drain: -N CarriedEnergy per second, magnitude via SetByCaller (the component sets
	// the negative value at apply from afl.Energy.DrainPerSecond). Same rail as every energy write.
	Period = FScalableFloat(1.0f);
	bExecutePeriodicEffectOnApplication = false; // first drain tick lands 1s in, not at apply

	FGameplayModifierInfo DrainMod;
	DrainMod.Attribute = UAFLAttributeSet_Energy::GetCarriedEnergyAttribute();
	DrainMod.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat DrainSetByCaller;
	DrainSetByCaller.DataTag = TAG_Data_Energy_Drain_BuffGE;
	DrainMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(DrainSetByCaller);
	Modifiers.Add(DrainMod);
}
