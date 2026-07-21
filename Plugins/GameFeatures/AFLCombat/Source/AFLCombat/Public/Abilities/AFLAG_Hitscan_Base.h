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

	/** FIRE MODE: true = hold-to-charge, fire on release (Railgun); false = fire on activation (Pulse-style). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Charge")
	bool bChargeToFire = false;

	/** Seconds to full charge (only when bChargeToFire). Below MinChargeToFire on release = no shot, no cost. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Charge", meta=(EditCondition="bChargeToFire", ClampMin="0.05"))
	float MaxChargeTime = 1.25f;

	/** Minimum charge fraction [0..1] required to fire (only when bChargeToFire). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Hitscan|Charge", meta=(EditCondition="bChargeToFire", ClampMin="0.0", ClampMax="1.0"))
	float MinChargeToFire = 0.25f;

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

	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> WaitReleaseTask;

	double ChargeStartTimeSeconds = 0.0;
	bool   bFired = false;
	bool   bChargeCueAdded = false;   // looping charge cue live -> RemoveGameplayCue in EndAbility
};
