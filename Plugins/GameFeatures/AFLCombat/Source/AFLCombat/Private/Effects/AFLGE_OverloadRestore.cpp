// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_OverloadRestore.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"   // CONVERGENCE: heal the Lyra set via its Healing meta
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_OverloadRestore)

// Native tag -- CDO-ctor-safe (module init precedes CDO construction; the Pulse.cpp rationale).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Health_Restore_GE, "Data.Health.Restore");


UAFLGE_OverloadRestore::UAFLGE_OverloadRestore()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// CONVERGENCE: heal via ULyraHealthSet's HEALING meta-attribute (Lyra maps Healing -> +Health, clamped, in
	// PostGameplayEffectExecute -- LyraHealthSet.cpp:151-156). Magnitude supplied per application via
	// SetByCaller(Data.Health.Restore). Serves all three heal consumers: the overload restore-to-floor, the world
	// health pickup, and the loadout/consumable heal ability. (Was: an Additive modifier on the now-retired AFL Health.)
	FGameplayModifierInfo Mod;
	Mod.Attribute = ULyraHealthSet::GetHealingAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAG_Data_Health_Restore_GE;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(Mod);
}
