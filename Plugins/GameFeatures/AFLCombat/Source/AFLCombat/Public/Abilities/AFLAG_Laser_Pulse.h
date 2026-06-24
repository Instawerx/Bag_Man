// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Abilities/AFLAG_Laser_Base.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/EngineTypes.h"

#include "AFLAG_Laser_Pulse.generated.h"

class UAFLPulseTuningData;
class UAnimMontage;
class UGameplayEffect;
struct FGameplayAbilityTargetDataHandle;


/**
 * UAFLAG_Laser_Pulse
 *
 * Foundation for the Pulse weapon (AFL-0104). Locally-predicted, single-shot
 * laser ability bound to InputTag.Weapon.Fire (binding lands in AFL-0107).
 *
 * AFL-0106 wired the client-builds / server-validates / server-applies path:
 * the firing client traces locally from the camera, packs the FHitResult and
 * claimed view origin into FAFLAbilityTargetData_Hitscan, ships it via
 * UAbilitySystemBlueprintLibrary::ServerSetReplicatedTargetData, and the
 * server applies DamageEffectClass on the target when the data arrives. The
 * server NEVER traces and never reads the player's viewpoint directly
 * (master doc §7, AFL-0215 lint).
 *
 * Server-side validation (schema, geometry, lag-comp re-trace) lands in
 * AFL-0211 / AFL-0213. The angular-velocity reject is stubbed today as a log
 * line only — see ServerApplyTargetData.
 */
UCLASS()
class AFLCOMBAT_API UAFLAG_Laser_Pulse : public UAFLAG_Laser_Base
{
	GENERATED_BODY()

public:

	UAFLAG_Laser_Pulse();

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
	 * GameplayEffect that applies Pulse damage on hit. Default-wired by AFL-0105
	 * to GE_AFL_Damage_Pulse; BP children of this ability can override the CDO
	 * to swap in designer-tuned variants in AFL-0214.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Pulse")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/**
	 * Per-shot damage seed written to Source.Damage before MakeOutgoingSpec.
	 * UAFLDamageExecCalc snapshots Source.Damage at spec creation, so this is
	 * the value the ExecCalc multiplies by Headshot/Weakpoint/Distance and
	 * routes through armor mitigation to Shield/Health output modifiers.
	 * AFL-0209/0213/0214 designer tuning hooks (DA_AFLPulseTuning) plug in here.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Pulse", meta=(ClampMin="0.0"))
	float BaseDamage = 18.0f;

	/** Maximum trace distance from the camera, centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Pulse|Trace", meta=(ClampMin="100.0", UIMin="100.0"))
	float MaxRange = 8000.0f;

	/** Collision channel for the hitscan trace. Defaults to Visibility to match Lyra's weapon trace channel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Pulse|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/**
	 * AFL-0213 telemetry stub: angular velocity above this budget logs a
	 * `hitscan_reject reason=ang` but does NOT discard the shot. Real reject
	 * lands with UAFLLagCompensationWorldSubsystem (AFL-0211).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Pulse|Telemetry", meta=(ClampMin="0.0"))
	float MaxAimAngularVelocityDegPerSec = 720.0f;

	/**
	 * Q2 cosmetic: distance along the aim ray, measured from the view origin,
	 * where the visual tracer cue STARTS. The trace itself is unaffected --
	 * trace origin, AimDirection, Hit, damage, crosshair are untouched.
	 *
	 * BagMan is third-person (CM_ThirdPerson). The carbine mesh sits left+up
	 * of sight (intentional hero framing). A geometric-muzzle tracer origin
	 * would bend the tracer off the aim ray at its start. Riding the aim ray
	 * (Lyra ELyraAbilityTargetingSource::CameraTowardsFocus pattern) keeps the
	 * tracer on the shot line, matching the sight=shot contract.
	 *
	 * Clamped at the emit site: if the impact is nearer than this distance,
	 * the origin pulls back toward the camera so the tracer never reverses.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Tracer", meta=(ClampMin="0.0", UIMin="0.0"))
	float TracerVisualOriginDistance = 80.0f;

	/**
	 * AFL-0209: data-driven recoil + spread tuning. Set on the BP/CDO to the
	 * canonical DA_AFLPulseTuning asset. AFL.Combat.LoadTuning swaps it at
	 * runtime; the per-knob cheats (AFL.Combat.SetSpread / AFL.Combat.SetRecoil)
	 * operate on a transient duplicate installed via SetTransientTuningData so
	 * the source asset is never mutated. Null is tolerated — the ability falls
	 * back to a hardcoded default set matching DA_AFLPulseTuning's defaults.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AFL|Tuning")
	TObjectPtr<UAFLPulseTuningData> TuningData;

	/**
	 * Third-person CHARACTER fire montage -- the trigger-pull + recoil KICK, played per shot.
	 * Mirrors ULyraGameplayAbility_RangedWeapon::CharacterFireMontage (the property GA_Weapon_Fire
	 * plays via PlayMontageAndWait). Defaulted in the ctor to AM_MM_Rifle_Fire: an
	 * AAT_ROTATION_OFFSET_MESH_SPACE ADDITIVE on SK_Mannequin, so it LAYERS on the held aim pose
	 * instead of replacing it. This is the VISUAL half of recoil (mesh kick + trigger pull, seen by
	 * everyone); TuningData above is the SEPARATE procedural half (camera/aim punishment, owner-only)
	 * -- the two complement, they do not double a single kick. BP children may override per-weapon.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Pulse|FX")
	TObjectPtr<UAnimMontage> CharacterFireMontage;

public:

	/**
	 * Replace the live TuningData pointer (called by AFL.Combat.LoadTuning and
	 * AFL.Combat.SetSpread / AFL.Combat.SetRecoil). Callers that need to mutate
	 * the asset must pass a duplicate; this setter does not duplicate for them.
	 */
	void SetTransientTuningData(UAFLPulseTuningData* InTuning) { TuningData = InTuning; }

	/** Read-only accessor for the cheats. */
	UAFLPulseTuningData* GetTuningData() const { return TuningData; }

private:

	/** Local trace + TargetData pack + ship to server. Local-predicting clients only. */
	void ClientPredictAndSend();

	/** Bound to UAbilitySystemComponent::AbilityTargetDataSetDelegate. Fires on both client (immediate) and server (replicated). */
	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	/** Server-only: validate (stub) + ApplyGameplayEffectSpecToTarget. */
	void ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data);

	// ResolveMuzzleLocation now lives on UAFLAG_Laser_Base (shared with Beam -- one resolver, no twin).

	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;

	/**
	 * AFL-0209 bloom state. InstancedPerActor — one instance per ASC, reused
	 * across activations — so these survive between shots naturally.
	 *
	 * `bBloomInitialized` gates a one-time floor sync to the resolved
	 * TuningData->BaseSpreadDegrees on the first ClientPredictAndSend. The
	 * header literals here are placeholders only; if a designer sets a
	 * different BaseSpreadDegrees on the DA, the first shot will use the DA's
	 * value, not this literal. Without that gate, the first shot would bloom
	 * from the header's 0.5f floor regardless of the DA — silently desyncing
	 * a tuned base.
	 *
	 * No UPROPERTY: these are local trace state, never replicated, never
	 * exposed to BP. Direction perturbation runs only on the firing client; the
	 * server's lag-comp re-trace consumes the perturbed direction back through
	 * FAFLAbilityTargetData_Hitscan::ClaimedAimDirection.
	 */
	float CurrentSpreadDegrees = 0.0f;
	float LastFireTime         = 0.0f;
	bool  bBloomInitialized    = false;

	/** AFL-0213 (cycle 3): previous shot's PRE-spread aim + fire time, for the per-shot-pair angular
	 *  velocity shipped in FAFLAbilityTargetData_Hitscan (replaces the hardcoded 0.0f that made the
	 *  720 deg/s telemetry check untrippable). InstancedPerActor -- survives between shots. -1.0
	 *  sentinel = no prior shot (the pair's first shot ships 0 deg/s). */
	FVector LastShotAimDirection = FVector::ZeroVector;
	double  LastShotTimeSeconds  = -1.0;
};
