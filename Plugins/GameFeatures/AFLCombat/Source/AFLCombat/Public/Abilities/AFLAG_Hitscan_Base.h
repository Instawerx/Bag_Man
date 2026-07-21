// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Abilities/AFLAG_Laser_Base.h"
#include "Engine/EngineTypes.h"

#include "AFLAG_Hitscan_Base.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UAbilityTask_WaitInputRelease;
struct FGameplayAbilityTargetDataHandle;
struct FHitResult;

/**
 * UAFLAG_Hitscan_Base
 *
 * The shared HITSCAN foundation for the AFL hitscan arsenal (Railgun/Flak/Overcharge SMG). It LIFTS
 * the proven Pulse pattern -- client traces from the camera, packs the hits into
 * FAFLAbilityTargetData_Hitscan(s) inside ONE FGameplayAbilityTargetDataHandle, ships them via
 * ServerSetReplicatedTargetData; the server validates (schema + lag-comp confirm) and applies the
 * damage GE -- but made REUSABLE by two hooks:
 *
 *   - PerformTrace() : the trace SHAPE. Base does a single line trace; bMultiHitPierce swaps in a
 *     multi-hit trace (Railgun's pierce). Flak will override for a spread cone; a subclass can
 *     override for anything exotic.
 *   - bChargeToFire  : the FIRE MODE. false = fire on activation (Pulse). true = hold-to-charge, fire
 *     on release (harvested from UAFLAG_Laser_Charge's WaitInputRelease gate; the Railgun).
 *
 * MULTI-HIT is nearly free: Pulse's ServerApplyTargetData already LOOPS the handle (Data.Num()), so
 * pierce is just "pack N hits client-side" -- the existing apply-loop damages each. NO new net-struct
 * (reuses FAFLAbilityTargetData_Hitscan in AFLNetTypes, the always-loaded module -> dodges the
 * GameFeature-struct-registration RPC-mismatch trap).
 *
 * DIVERGENCE FROM PULSE (deliberate; the proven Pulse/Charge stay UNTOUCHED -- reparent-to-dedupe is a
 * later pass): this base DROPS Pulse's bloom/spread cone, procedural recoil, and the AFL-0213 angular-
 * velocity telemetry (a railgun is a precise, non-blooming weapon; Flak's spread is a PerformTrace
 * concern, not per-shot bloom). It KEEPS the exact predict-send window shape, the schema check, and the
 * lag-comp rewind+confirm per hit. Concrete (not abstract): granted via BP children that just set the
 * flags + the damage GE + montage/cooldown.
 */
UCLASS()
class AFLCOMBAT_API UAFLAG_Hitscan_Base : public UAFLAG_Laser_Base
{
	GENERATED_BODY()

public:

	UAFLAG_Hitscan_Base();

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
	 * The trace SHAPE hook. Default: a single line trace (LineTraceSingleByChannel) -> 0/1 hit; if
	 * bMultiHitPierce, a multi-hit trace (LineTraceMultiByChannel) -> every pawn along the ray. Fills
	 * OutHits with the ordered hits (near->far). Override for exotic shapes (Flak spread cone).
	 */
	virtual void PerformTrace(UWorld* World, const FVector& ViewOrigin, const FVector& AimDir,
		AActor* IgnoreActor, TArray<FHitResult>& OutHits) const;

	/** GE applied to each hit target's ASC (seeds Source.Damage = BaseDamage via SetByCaller). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Per-shot damage seed. Full-damage-through on pierce for the pilot (no per-body falloff yet). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan", meta=(ClampMin="0.0"))
	float BaseDamage = 40.0f;

	/** Max trace distance from the camera, cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Trace", meta=(ClampMin="100.0"))
	float MaxRange = 12000.0f;

	/** Trace channel (Visibility matches Lyra's weapon trace). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** PIERCE: multi-hit trace that penetrates every body along the ray (Railgun). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Trace")
	bool bMultiHitPierce = false;

	/** FLAK SPREAD: fire a cone of PelletCount single-hit pellets instead of one line. Overrides pierce -- spread
	 * is the identity, not spread+pierce. Each pellet packs as its own hit into the one handle, so the proven
	 * apply-loop damages each (BaseDamage per pellet -- keep it LOW so a full point-blank cone ~= one strong shot). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Trace")
	bool bSpreadCone = false;

	/** Pellets per shot when bSpreadCone; each is a single-hit trace (no pierce) applying the damage GE once. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Trace", meta=(EditCondition="bSpreadCone", ClampMin="1"))
	int32 PelletCount = 8;

	/** Cone half-angle (deg) when bSpreadCone. Wider = more natural range falloff (pellets miss at distance). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Trace", meta=(EditCondition="bSpreadCone", ClampMin="0.0"))
	float SpreadHalfAngleDeg = 5.0f;

	/** FIRE MODE: true = hold-to-charge, fire on release (Railgun); false = fire on activation (Pulse-style). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Charge")
	bool bChargeToFire = false;

	/** Seconds to full charge (only when bChargeToFire). Below MinChargeToFire on release = no shot, no cost. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Charge", meta=(EditCondition="bChargeToFire", ClampMin="0.05"))
	float MaxChargeTime = 1.25f;

	/** Minimum charge fraction [0..1] required to fire (only when bChargeToFire). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Charge", meta=(EditCondition="bChargeToFire", ClampMin="0.0", ClampMax="1.0"))
	float MinChargeToFire = 0.25f;

	/** AUTO-FIRE (Overcharge SMG): sustained fire while the input is held, RPM ramps with ability-local heat.
	 * Overrides the charge/instant fire modes. One activation = one held burst (the loop is inside). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Auto")
	bool bAutoFire = false;

	/** Fire interval at zero heat -- the COLD RPM (0.15s = 400 RPM). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Auto", meta=(EditCondition="bAutoFire", ClampMin="0.02"))
	float ColdFireInterval = 0.15f;

	/** Fire interval at max heat -- the HOT RPM (0.067s = ~900 RPM). Interval Lerps Cold->Hot as HeatNorm 0->1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Auto", meta=(EditCondition="bAutoFire", ClampMin="0.02"))
	float HotFireInterval = 0.067f;

	/** Heat added per shot [0..1]. MUST exceed HeatDecayPerSec*ColdFireInterval so a HELD burst heats even from cold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Auto", meta=(EditCondition="bAutoFire", ClampMin="0.0"))
	float HeatPerShot = 0.14f;

	/** Heat decayed per second of the GAP since the last shot -> tight-packed hold shots heat, spaced taps cool.
	 * At 0.7 a tap gap >~0.22s cools more than one shot adds (bursts stay cool; only a sustained hold overheats). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Auto", meta=(EditCondition="bAutoFire", ClampMin="0.0"))
	float HeatDecayPerSec = 0.7f;

	/** OVERHEAT LOCKOUT: server-applied cooldown GE when HeatNorm hits 1. Grant State.Weapon.Overheated (~1.5s);
	 * that tag is in ActivationBlockedTags, so the lockout is server-validated (a client can't ignore it). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Auto", meta=(EditCondition="bAutoFire"))
	TSubclassOf<UGameplayEffect> OverheatCooldownEffectClass;

	/** Third-person fire montage (trigger-pull + kick), fire-and-forget like Pulse. BP child may override. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|FX")
	TObjectPtr<UAnimMontage> CharacterFireMontage;

private:

	/** The shared fire path: trace -> pack handle -> ship -> (authority) apply. Called on activate (instant) or release (charge). */
	void Fire();

	/** Local trace + multi-hit TargetData pack + ship to server. Local-predicting clients only. */
	void ClientPredictAndSend();

	/** Bound to AbilityTargetDataSetDelegate; fires on client (immediate) and server (replicated). */
	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	/** Server-only: schema check + lag-comp confirm + ApplyGameplayEffectSpecToTarget, LOOPED over every hit. */
	void ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data);

	/** Charge fraction [0..1] from ChargeStartTimeSeconds. */
	float ComputeChargeNorm() const;

	/** WaitInputRelease callback (bChargeToFire path) -> Fire() if charged enough, else cancel free. */
	UFUNCTION()
	void OnChargeInputReleased(float TimeHeld);

	/** AUTO-FIRE loop (bAutoFire, local client only): start the repeating fire timer + the WaitInputRelease gate. */
	void StartAutoFire();
	/** One auto-shot -- bypasses Fire()'s bFired/CommitAbility gates -- then reschedules at CurrentFireInterval unless overheated. */
	void AutoFireTick();
	/** Clear the fire timer + EndAbility (called on input release or on overheat). */
	void StopAutoFire();
	/** WaitInputRelease callback (bAutoFire path) -> StopAutoFire. */
	UFUNCTION()
	void OnAutoFireInputReleased(float TimeHeld);
	/** Decay HeatNorm by the gap since the last shot, add HeatPerShot, clamp [0..1]; returns the new HeatNorm. */
	float AdvanceHeat();
	/** Current fire interval = Lerp(Cold, Hot) by HeatNorm -- RPM ramps with heat. */
	float CurrentFireInterval() const;

	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitReleaseTask;

	double ChargeStartTimeSeconds = 0.0;
	bool   bFired = false;
	bool   bChargeCueAdded = false;   // looping charge cue live -> RemoveGameplayCue in EndAbility

	// AUTO-FIRE (bAutoFire) ability-local state. HeatNorm + LastShotTimeSeconds PERSIST across bursts
	// (InstancedPerActor) so the gap-based decay cools between bursts. NOT an attribute, NOT replicated --
	// the fire cadence is a client-feel thing; the server independently ramps its own copy per received shot
	// for the authoritative overheat lockout.
	FTimerHandle AutoFireTimerHandle;
	float  HeatNorm = 0.0f;
	double LastShotTimeSeconds = 0.0;
	bool   bOverheated = false;
};
