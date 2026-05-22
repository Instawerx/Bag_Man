// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/EngineTypes.h"

#include "AFLAG_Laser_Beam.generated.h"

class UGameplayEffect;
struct FGameplayAbilityTargetDataHandle;


/**
 * UAFLAG_Laser_Beam
 *
 * AFL-0206 channeling laser ability — the held-trigger sibling to the
 * single-shot Pulse (UAFLAG_Laser_Pulse, AFL-0104). While the trigger is
 * held the ability ticks every 100ms; each tick the firing client re-traces
 * from the camera, packs the hit into FAFLAbilityTargetData_Hitscan (reused
 * across Pulse + Beam — no per-weapon target-data forks), and ships it via
 * ServerSetReplicatedTargetData. The server applies GE_AFL_Damage_BeamTick
 * per arriving payload, which routes 1.2 damage through UAFLDamageExecCalc
 * for a baseline 12 dps.
 *
 * Channel termination: a UAbilityTask_WaitInputRelease listens for the
 * input release on both client and server (built-in GAS task — clients
 * replicate the release event up). On release we stop the tick timer,
 * apply GE_AFL_Cooldown_Beam (placeholder 3s; designer-tunes in S5), and
 * EndAbility on both sides.
 *
 * Out of scope (deferred to dependent tasks): heat consumption per tick
 * (AFL-0207), Niagara prism beam visual (AFL-0208), audio (AFL-0205),
 * InputTag.Weapon.SecondaryFire binding (AFL-0107 follow-up).
 *
 * Hard rails (per master doc §9.3 / AFL-0215 lint, mirrored from Pulse):
 *   - Extends ULyraGameplayAbility to keep the Lyra commit lifecycle.
 *   - Server never reads the player viewpoint directly. Trace is client-side
 *     only; payload ships via ServerSetReplicatedTargetData (master doc §7).
 *   - All native tags declared via UE_DEFINE_GAMEPLAY_TAG_STATIC at file
 *     scope in the .cpp; no RequestGameplayTag in the constructor body
 *     (post-2026-05-20 CDO crash pattern).
 */
UCLASS()
class AFLCOMBAT_API UAFLAG_Laser_Beam : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:

	UAFLAG_Laser_Beam();

protected:

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/**
	 * Per-tick damage GE. Defaults to GE_AFL_Damage_BeamTick (1.2 dmg, grants
	 * Event.Damage.BeamTick). BP children can swap in designer-tuned variants
	 * once AFL-0214 lands (parallel to Pulse).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * Cooldown GE applied to the source ASC on channel end. Defaults to
	 * GE_AFL_Cooldown_Beam (3s, grants Cooldown.Weapon.Beam). The cooldown
	 * is independent of the GAS CooldownGameplayEffectClass slot because
	 * the Lyra commit flow runs once at activation, not at release.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam")
	TSubclassOf<UGameplayEffect> ReleaseCooldownEffectClass;

	/** Channel tick interval in seconds. 0.1s = 10 ticks/sec = 12 dps at 1.2 dmg/tick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam", meta=(ClampMin="0.01", UIMin="0.01"))
	float TickInterval = 0.1f;

	/** Maximum trace distance from the camera, centimetres. Matches Pulse default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Trace", meta=(ClampMin="100.0", UIMin="100.0"))
	float MaxRange = 8000.0f;

	/** Collision channel for the per-tick hitscan trace. Visibility matches Lyra's weapon channel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:

	/** Called by the FTimerManager on the locally-controlled client every TickInterval. */
	void TickChannel();

	/** Bound to UAbilityTask_WaitInputRelease's OnRelease delegate on both sides. */
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	/** Bound to UAbilityTargetDataSetDelegate. Fires on both client (immediate) and server (replicated). */
	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	/** Server-only per-tick GE apply. Source struct is FAFLAbilityTargetData_Hitscan, same as Pulse. */
	void ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data);

	/** Source-side cooldown apply on release. No-op when ReleaseCooldownEffectClass is unset. */
	void ApplyReleaseCooldown();

	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;
	FTimerHandle TickTimerHandle;
};
