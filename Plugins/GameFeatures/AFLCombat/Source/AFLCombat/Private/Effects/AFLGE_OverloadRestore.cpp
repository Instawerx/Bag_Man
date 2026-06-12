// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/AFLGE_OverloadRestore.h"

#include "Attributes/AFLAttributeSet_Combat.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGE_OverloadRestore)

// Native tag -- CDO-ctor-safe (module init precedes CDO construction; the Pulse.cpp rationale).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Data_Health_Restore_GE, "Data.Health.Restore");


UAFLGE_OverloadRestore::UAFLGE_OverloadRestore()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Additive Health, magnitude supplied per application via SetByCaller(Data.Health.Restore) --
	// positive restore-to-floor (the intercept computes floor - current at apply). Same shape as the
	// energy gain GE; clamping is on the combat set.
	FGameplayModifierInfo Mod;
	Mod.Attribute = UAFLAttributeSet_Combat::GetHealthAttribute();
	Mod.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = TAG_Data_Health_Restore_GE;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(Mod);
}
