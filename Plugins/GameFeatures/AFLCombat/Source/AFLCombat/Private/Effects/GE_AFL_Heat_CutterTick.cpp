// Copyright C12 AI Gaming. All Rights Reserved.

#include "Effects/GE_AFL_Heat_CutterTick.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GE_AFL_Heat_CutterTick)


UGE_AFL_Heat_CutterTick::UGE_AFL_Heat_CutterTick()
{
	// Parent ctor added the single HeatPerBeamTick Override modifier at 4.0 (the
	// retired laser's ~2.5s window against MaxHeat 100). Retune that SAME modifier
	// to 2.5: 40 ticks @ 100ms = the ruled ~4s continuous-cut window. Index-guarded
	// so a future parent reshape can't silently strand the tune on a wrong slot.
	if (Modifiers.Num() > 0)
	{
		Modifiers[0].ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.5f));
	}
}
