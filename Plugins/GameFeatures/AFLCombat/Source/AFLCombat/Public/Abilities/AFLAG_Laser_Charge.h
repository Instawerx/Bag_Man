// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Abilities/AFLAG_Laser_Base.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/EngineTypes.h"

#include "AFLAG_Laser_Charge.generated.h"

class UAnimMontage;
class UCurveFloat;
class UGameplayEffect;
struct FGameplayAbilityTargetDataHandle;


/**
 * UAFLAG_Laser_Charge  -- the CHARGE fire mode (power-weapon pilot: Tempest).
 *
 * The first mechanically-DISTINCT AFL fire ability: hold to charge, release to fire a single
 * charge-scaled hitscan shot. Distinct from the auto Pulse / held Beam that share the roster --
 * this is the "cross the map for it" power weapon the world spawners grant.
 *
 * SHAPE = BeamChannel_v2's hold lifecycle + Pulse's single-shot fire, per the conform-to spec
 * (Docs/IRONICS_WEAPON_AUTHORING_SPEC.md) and the feature map (Tools/full-weapon-feature-map.md §Charge):
 *   - HOLD (from BeamChannel_v2): ActivationPolicy=WhileInputActive + one UAbilityTask_WaitInputRelease
 *     (the PROVEN live pattern -- BeamChannel_v2.cpp:53,135-140; the old "thrash" was a pressed-trigger
 *     instant-loop, NOT this task). On activate: record the charge-start time + add the looping charge
 *     cue. NO CommitAbility on press (charge is free; the cooldown commits on FIRE only).
 *   - FIRE (from Pulse, deferred to release): OnChargeInputReleased computes Norm =
 *     clamp(held/MaxChargeTime,0,1); if Norm>=MinChargeToFire -> CommitAbility (cooldown now) +
 *     the locally-controlled client runs ClientPredictAndSend (camera trace, pack the SAME
 *     FAFLAbilityTargetData_Hitscan, ship to server); OnTargetDataReadyCallback plays the montage +
 *     Fire/Tracer cues + server-send (Pulse's proven shared-prediction-window shape); ServerApplyTargetData
 *     lag-comp-confirms + applies DamageEffectClass with Source.Damage = BaseDamage * ChargeCurve.Eval(Norm)
 *     (the ONLY divergence from Pulse's flat seed). Below MinChargeToFire -> cancel (no shot, no cooldown).
 *   - CLEANUP: EndAbility (end + cancel) unbinds the target-data delegate and RemoveGameplayCue(Charge) --
 *     the looping charge cue's OnRemove is the one place its VFX/audio/shake stop.
 *
 * Extends UAFLAG_Laser_Base (the ONE shared muzzle + tint resolver) -- NEVER the retired
 * AFLAG_Laser_Beam. LocalPredicted + InstancedPerActor + the bot-fire GameplayEvent trigger +
 * the blocked-tag set, exactly as Pulse/Beam (mandatory conform items).
 */
UCLASS()
class AFLCOMBAT_API UAFLAG_Laser_Charge : public UAFLAG_Laser_Base
{
	GENERATED_BODY()

public:

	UAFLAG_Laser_Charge();

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
	 * The "timeline": maps normalized charge 0..1 -> damage MULTIPLIER. Null-safe: a null curve falls
	 * back to a linear 0..1 -> 0.5..1.0 ramp (min charge = half damage, full = full), so the ability
	 * ships without a curve asset and the BP child plugs in a designer curve. Evaluated at Norm on the
	 * server (authoritative) for the damage seed + on the client (cosmetic) for the cue intensity.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Charge")
	TObjectPtr<UCurveFloat> ChargeCurve;

	/** Seconds to reach full charge (Norm=1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Charge", meta=(ClampMin="0.05", UIMin="0.05"))
	float MaxChargeTime = 1.5f;

	/** Minimum Norm to fire on release; below this the release CANCELS (no shot, no cooldown). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Charge", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinChargeToFire = 0.2f;

	/**
	 * Per-shot damage seed at FULL charge (Norm=1). Written to Source.Damage * ChargeCurve.Eval(Norm)
	 * before MakeOutgoingSpec; UAFLDamageExecCalc snapshots it. Power-weapon baseline (heavier than
	 * Pulse's 18) -- designer-tuned on the BP child.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Charge", meta=(ClampMin="0.0"))
	float BaseDamage = 60.0f;

	/** Damage GE (defaults to GE_AFL_Damage_Charge). Routes through the proven UAFLDamageExecCalc via Source.Damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Charge")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Max camera-trace distance (cm). Longer than Pulse -- a charge shot reaches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Charge|Trace", meta=(ClampMin="100.0", UIMin="100.0"))
	float MaxRange = 12000.0f;

	/** Hitscan trace channel (Visibility, matching Pulse/Beam). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Charge|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/**
	 * Third-person CHARACTER fire montage -- the trigger-pull / heavy kick, played ONCE at fire (release)
	 * via the GAS-replicated PlayMontage path (Pulse/Beam shape). Defaulted to AM_MM_Rifle_Fire (additive
	 * AAT_ROTATION_OFFSET_MESH_SPACE on SK_Mannequin); the Tempest BP child may override. MANDATORY conform
	 * item -- without it the gun fires but the hero never pulls the trigger.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Charge|FX")
	TObjectPtr<UAnimMontage> CharacterFireMontage;

private:

	/** Local trace + TargetData pack + ship (locally-controlled only). Charge intensity is recomputed in the
	 *  callback via ComputeChargeNorm() so it is valid on the server path too. */
	void ClientPredictAndSend();

	/** Bound to AbilityTargetDataSetDelegate -- fires on client (immediate) and server (replicated). */
	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	/** Server-only: lag-comp confirm + apply DamageEffectClass with charge-scaled Source.Damage. */
	void ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data);

	/** WaitInputRelease OnRelease sink (both sides). Fires the charged shot or cancels. */
	UFUNCTION()
	void OnChargeInputReleased(float TimeHeld);

	/** Norm = clamp((Now - ChargeStartTimeSeconds)/MaxChargeTime, 0, 1). Own timing on each side (server-authoritative for damage). */
	float ComputeChargeNorm() const;

	/** ChargeCurve->Eval(Norm) with the null-safe 0.5..1.0 linear fallback. */
	float EvalChargeMultiplier(float Norm) const;

	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;

	/** World seconds at ActivateAbility (charge start). InstancedPerActor -> per-ASC; -1 = not charging. */
	double ChargeStartTimeSeconds = -1.0;

	/** Whether the looping charge cue was added (so EndAbility removes exactly once). */
	bool bChargeCueAdded = false;
};
