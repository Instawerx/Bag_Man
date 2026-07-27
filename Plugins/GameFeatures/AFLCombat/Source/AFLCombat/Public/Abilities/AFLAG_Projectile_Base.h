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

	/** SEEKER: on fire the server SOFT-LOCKS the enemy under the reticle and hands the spawned projectile's
	 * ProjectileMovement a HomingTargetComponent. false = straight projectile (Rocket path, untouched). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile|Homing")
	bool bHoming = false;

	/** Homing curve strength (cm/s^2) written to the projectile's PMC on lock. Tuned to curve but stay DODGEABLE
	 * (juke / break LoS shakes it). ~8000+ is un-dodgeable = un-fun. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile|Homing", meta=(EditCondition="bHoming", ClampMin="0.0"))
	float HomingAccelerationMagnitude = 4000.0f;

	/** Soft-lock sweep range from the reticle, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile|Homing", meta=(EditCondition="bHoming", ClampMin="100.0"))
	float HomingLockRange = 12000.0f;

	/** Soft-lock sweep radius, cm -- the reticle tolerance so you needn't pixel-aim at the target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile|Homing", meta=(EditCondition="bHoming", ClampMin="0.0"))
	float HomingLockRadius = 60.0f;

	/**
	 * ARC-LOB (Volt Coilbreaker / neutral Lobber). false = straight/homing path UNTOUCHED. When true the launch
	 * is a FIXED-ARC ballistic lob: the spawn aim is pitched UP by ArcLaunchPitchDegrees and the projectile's PMC
	 * gravity is turned on (ArcGravityScale) so it travels a parabola -- aim higher to throw further (grenade/mortar
	 * feel), NOT an aim-assisted solve-to-crosshair. Parallel flag to bHoming; a weapon sets ONE (a homing lob is
	 * nonsensical -- if both are set, arc wins and homing is skipped). The projectile's own impact/overlap radial
	 * splash is trajectory-independent, so the arc reuses the BigSixx/Draco splash path with zero extra wiring.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile|Arc")
	bool bArcLob = false;

	/** Degrees to pitch the launch UP from the player's aim (fixed-arc). ~30 = a lobbed grenade-launcher throw;
	 * higher = a steeper mortar. The player still aims higher/lower to tune range on top of this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile|Arc", meta=(EditCondition="bArcLob", ClampMin="0.0", ClampMax="80.0"))
	float ArcLaunchPitchDegrees = 30.0f;

	/** PMC gravity scale applied to the arc projectile (> 0 -> it falls into the lob). 1.0 = world gravity, grenade-y;
	 * raise for a snappier/shorter arc, lower for a floatier/longer one. Only applied when bArcLob.
	 *
	 * ⚠⚠ ASSEMBLY REQUIREMENT -- THIS VALUE MUST BE MIRRORED ON THE PROJECTILE BP CDO.
	 * The projectile spawns on AUTHORITY ONLY and reaches clients by replication, so this write lands on the
	 * server's PMC and NOT on any client's. A client whose PMC still carries the flat-rocket default (0.0)
	 * extrapolates a STRAIGHT line between replication updates while the server flies a parabola -- the lob
	 * visibly stutters and snaps. Set ProjectileGravityScale on the arc projectile BP to the SAME number.
	 * (The straight/homing weapons are unaffected: both sides sit at 0.0 and already agree.) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Projectile|Arc", meta=(EditCondition="bArcLob", ClampMin="0.0"))
	float ArcGravityScale = 1.0f;
};
