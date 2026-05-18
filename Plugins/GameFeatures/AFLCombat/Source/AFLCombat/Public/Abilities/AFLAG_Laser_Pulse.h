// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/EngineTypes.h"

#include "AFLAG_Laser_Pulse.generated.h"

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
class AFLCOMBAT_API UAFLAG_Laser_Pulse : public ULyraGameplayAbility
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

private:

	/** Local trace + TargetData pack + ship to server. Local-predicting clients only. */
	void ClientPredictAndSend();

	/** Bound to UAbilitySystemComponent::AbilityTargetDataSetDelegate. Fires on both client (immediate) and server (replicated). */
	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	/** Server-only: validate (stub) + ApplyGameplayEffectSpecToTarget. */
	void ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data);

	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;
};
