// Copyright C12 AI Gaming. All Rights Reserved.

#include "Abilities/AFLAG_Deployable_Base.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAG_Deployable_Base)

UAFLAG_Deployable_Base::UAFLAG_Deployable_Base()
{
	// Inherits the entire proven throw contract from UAFLAG_Projectile_Base's ctor (net policy, bot-fire
	// GameplayEvent trigger, State.Firing owned tag, the blocked set, the default fire montage). A deployable
	// throws exactly like the Rocket -- the delivery is identical; only the spawned puck's on-land behaviour
	// differs (spawn a deployed actor, not radial damage). So there is nothing to add here for the pilot: the
	// puck (ProjectileClass) is a lobbing projectile whose BP spawns the barrier on rest. DeployableClass is
	// the reserved data-drive knob (see header). BP child GA_AFL_Shield sets ProjectileClass + cooldown/montage.
}
