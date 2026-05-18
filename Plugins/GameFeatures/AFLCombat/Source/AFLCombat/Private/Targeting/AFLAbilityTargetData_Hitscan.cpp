// Copyright C12 AI Gaming. All Rights Reserved.

#include "Targeting/AFLAbilityTargetData_Hitscan.h"

#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAbilityTargetData_Hitscan)


void FAFLAbilityTargetData_Hitscan::AddTargetDataToContext(FGameplayEffectContextHandle& Context, bool bIncludeActorArray) const
{
	// Parent populates HitResult + (optionally) the actor array on the context.
	FGameplayAbilityTargetData_SingleTargetHit::AddTargetDataToContext(Context, bIncludeActorArray);

	// Stamp the claimed origin onto the context. The engine's effect context
	// owns Origin already (FGameplayEffectContext::Origin), so we reuse it
	// rather than introducing a parallel field on a subclassed context. Server
	// validators downstream (AFL-0211 LOS re-trace) can read this back via
	// Context.GetOrigin() without knowing about FAFLAbilityTargetData_Hitscan.
	Context.AddOrigin(ClaimedViewOrigin);
}

bool FAFLAbilityTargetData_Hitscan::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayAbilityTargetData_SingleTargetHit::NetSerialize(Ar, Map, bOutSuccess);

	ClaimedViewOrigin.NetSerialize(Ar, Map, bOutSuccess);
	ClaimedAimDirection.NetSerialize(Ar, Map, bOutSuccess);
	Ar << AimAngularVelocityDegPerSec;

	return true;
}
