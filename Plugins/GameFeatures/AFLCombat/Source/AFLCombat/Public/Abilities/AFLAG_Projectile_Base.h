// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Abilities/AFLAG_Laser_Base.h"

#include "AFLAG_Projectile_Base.generated.h"

class UAnimMontage;

/**
 * UAFLAG_Projectile_Base
 *
 * Shared base for the AFL PROJECTILE weapons (Rocket first; Seeker reuses it with the
 * projectile's built-in ProjectileMovement homing). It inherits UAFLAG_Laser_Base's ONE muzzle
 * resolver and reuses the exact AFL fire-GA CONTRACT the proven Pulse ability established
 * (ReplicateNo / LocalPredicted / InstancedPerActor, the bot-fire GameplayEvent trigger, the
 * State.Firing owned tag, the Carrying/ThrowRecovery/Warmup/Ended blocked set, a per-weapon
 * CharacterFireMontage, cooldown on the CDO) -- but the fire ACTION is different: instead of a
 * hitscan trace + predict-and-send, it SERVER-AUTHORITATIVELY SPAWNS a replicated projectile
 * actor from the muzzle along the aim. The projectile (a B_Grenade-derived BP: ProjectileMovement
 * + Sphere-Overlap radial splash + explosion VFX) owns travel, impact, and damage -- so this
 * ability stays thin (no DamageEffectClass here; the projectile applies its own).
 *
 * Abstract: never granted directly. The granted ability is the BP child (GA_AFL_Rocket) that sets
 * ProjectileClass + the per-weapon montage/cooldown.
 *
 * Net model: LocalPredicted so the owner predicts the montage/cue for instant feel; the projectile
 * itself is spawned ONLY on authority (a replicated actor) so there is exactly one authoritative
 * projectile -- the server never reads the client viewpoint (it aims off the replicated control
 * rotation via GetBaseAimRotation), same doctrine as Pulse's server path.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLAG_Projectile_Base : public UAFLAG_Laser_Base
{
	GENERATED_BODY()

public:

	UAFLAG_Projectile_Base();

protected:

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** The replicated projectile actor to spawn (e.g. B_AFL_Rocket_Projectile). Set on the BP child CDO. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile")
	TSubclassOf<AActor> ProjectileClass;

	/**
	 * Launch speed along the aim, cm/s. If > 0, the spawned projectile's ProjectileMovementComponent
	 * velocity is overwritten to AimDir * this. If 0, the projectile's own InitialSpeed + the spawn
	 * rotation are used verbatim (the harvest-default path). BP child tunes per weapon.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile", meta=(ClampMin="0.0"))
	float LaunchSpeed = 0.0f;

	/**
	 * Third-person CHARACTER fire montage -- the trigger-pull + additive recoil kick, played
	 * fire-and-forget (ASC->PlayMontage, NOT AndWait -- the single-shot EndAbility would blend a kick
	 * out). Mirrors Pulse's CharacterFireMontage. Defaulted in the ctor to the stock rifle fire montage
	 * (AM_MM_Rifle_Fire on SK_Mannequin, additive); BP children may override per weapon.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile|FX")
	TObjectPtr<UAnimMontage> CharacterFireMontage;
};
