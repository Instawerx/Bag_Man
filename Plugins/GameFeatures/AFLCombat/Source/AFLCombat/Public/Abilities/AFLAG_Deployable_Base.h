// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Abilities/AFLAG_Projectile_Base.h"

#include "AFLAG_Deployable_Base.generated.h"

/**
 * UAFLAG_Deployable_Base
 *
 * Shared base for AFL DEPLOYABLE weapons -- weapons that THROW a puck projectile which, on
 * landing, spawns a persistent replicated DEPLOYED actor (Shield = a barrier wall; the future
 * EMP-disable = a field). Inherits the entire proven throw machinery from UAFLAG_Projectile_Base
 * (server-authoritative SpawnActor from the muzzle along the aim, the Pulse fire contract:
 * ReplicateNo / LocalPredicted / InstancedPerActor, the bot-fire GameplayEvent trigger, the
 * State.Firing owned tag + Carrying/ThrowRecovery/Warmup/Ended blocked set, CharacterFireMontage,
 * cooldown on the CDO). The DELIVERY is identical to a rocket; the difference is entirely in the
 * spawned projectile -- a "puck" whose on-land behaviour spawns DeployableClass instead of dealing
 * radial damage.
 *
 * HARVEST NOTE: this adds NO new delivery code over the Rocket. It exists as the named foundation
 * the EMP-disable (and any future placeable) inherits, and as the home for DeployableClass -- the
 * data-driven "what to deploy" knob. For the Shield PILOT the puck BP references its barrier
 * directly (mirrors how the Rocket projectile self-contains its explosion); the interface-driven
 * stamp (ability -> puck.DeployableClass) is the increment-2 generalisation that lets EMP reuse
 * ONE puck with a different DeployableClass. Documented, not built, to keep the pilot thin.
 *
 * Abstract: never granted directly. The granted ability is the BP child (GA_AFL_Shield) that sets
 * ProjectileClass (the puck) + DeployableClass (the barrier) + the per-weapon montage/cooldown.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLAG_Deployable_Base : public UAFLAG_Projectile_Base
{
	GENERATED_BODY()

public:

	UAFLAG_Deployable_Base();

protected:

	/**
	 * The persistent deployed actor the thrown puck spawns on landing (e.g. B_AFL_ShieldBarrier).
	 * Reserved for the data-driven puck (increment 2, shared by EMP); for the Shield pilot the puck
	 * BP references its barrier directly, so this documents the contract without wiring it yet.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Deployable")
	TSubclassOf<AActor> DeployableClass;
};
