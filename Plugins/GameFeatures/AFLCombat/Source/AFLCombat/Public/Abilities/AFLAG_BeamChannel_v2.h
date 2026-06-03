// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/EngineTypes.h"

#include "AFLAG_BeamChannel_v2.generated.h"

class UGameplayEffect;
class UAFLBeamChannelComponent;
class UAFLBeamVisualComponent;
struct FGameplayAbilityTargetDataHandle;


/**
 * UAFLAG_BeamChannel_v2  (CANARY — the rebuilt firing system, clean-room, throwaway-shaped)
 *
 * Proves the rebuilt beam architecture END-TO-END alongside the 3bf573b3 control (which stays
 * untouched). This is NOT a patch of UAFLAG_Laser_Beam -- it is the NEW path, built to ship.
 *
 * The rebuilt path (vs the retired cue-spawn model):
 *   - Q1=(b) DOCTRINE TRACE: WhileInputActive channel; each tick the locally-controlled client
 *     re-traces from the camera, publishes the world-space impact + muzzle into the pawn's
 *     UAFLBeamChannelComponent (the replicated published-value bridge -- KEPT, it's correct), and
 *     ships the payload to the server for damage. Server never reads the viewpoint.
 *   - Q2=(a) DELIVERY ON THE REPLICATED WEAPON ACTOR: the visual is a persistent toggled
 *     UAFLBeamVisualComponent on the weapon DISPLAY actor (which replicates -- proven 1c3cb0f9).
 *     This ability resolves that component (cross-actor: pawn-ability -> weapon actor -> component,
 *     exactly the bridge the real system needs) and calls SetBeamActive(true) on channel start /
 *     (false) on end. Authority-gated -> the replicated bBeamActive + OnRep drive the toggle on
 *     every machine via a shared function (Edge 1). NO GameplayCue spawn here.
 *   - WhileInputActive ALONE owns the held lifecycle -- NO explicit UAbilityTask_WaitInputRelease
 *     (the thrash lesson: WhileInputActive + WaitInputRelease re-cycle ~6x/sec). Lyra's
 *     CancelInputActivatedAbilities ends it on release; release-cooldown applies in EndAbility.
 *
 * KEEPS the proven foundation: UAFLDamageExecCalc + GE damage, UAFLAttributeSet_Combat heat,
 * lag-comp, AFLNetTypes hitscan payload, the AFLBeamChannelComponent bridge. Only the
 * weapon/cue/visual DELIVERY is new.
 *
 * ACCEPTANCE GATE: 2-client listen-server proxy watch -- host fires, the CLIENT sees the host's
 * proxy show mesh + beam Niagara tracking muzzle->target + toggle OFF on release. Pass -> the
 * architecture ships; stamp variants. Fail -> caught in this one canary, control intact.
 */
UCLASS()
class AFLCOMBAT_API UAFLAG_BeamChannel_v2 : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLAG_BeamChannel_v2();

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

	/** Per-tick damage GE (defaults to GE_AFL_Damage_BeamTick — same proven ExecCalc pipeline). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Release-cooldown GE (defaults to GE_AFL_Cooldown_Beam). Applied in EndAbility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2")
	TSubclassOf<UGameplayEffect> ReleaseCooldownEffectClass;

	/** Channel tick interval (s). 0.1 = 10 ticks/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float TickInterval = 0.1f;

	/** Max camera trace distance (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2|Trace", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float MaxRange = 8000.0f;

	/** Trace channel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
	/** Locally-controlled per-tick trace + publish (timer-driven). */
	void TickChannel();

	/** Pulse's proven muzzle resolve (attached SMC "Muzzle" socket; weapon_r fallback; never origin). */
	FVector ResolveMuzzleLocation(class APawn* AvatarPawn) const;

	/** The pawn's UAFLBeamChannelComponent (published-endpoint bridge, Q1=b). Authority creates if absent. */
	UAFLBeamChannelComponent* ResolveBeamChannel();

	/**
	 * Q2=(a) cross-actor reach: the UAFLBeamVisualComponent on the EQUIPPED WEAPON ACTOR (which
	 * replicates). Walk the avatar pawn's attached actors for the one carrying the visual component.
	 * This is the pawn-ability -> weapon-actor bridge the shipping system needs; proving it works
	 * here is the point. Authority calls SetBeamActive on it; the replicated bBeamActive + OnRep
	 * toggle the persistent NS on every machine.
	 */
	UAFLBeamVisualComponent* ResolveWeaponVisual() const;

	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);
	void ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data);
	void ApplyReleaseCooldown();

	/** Bound to UAbilityTask_WaitInputRelease's OnRelease delegate (both sides) -> ends the channel. */
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;
	FTimerHandle TickTimerHandle;
	TWeakObjectPtr<UAFLBeamChannelComponent> BeamChannel;
	TWeakObjectPtr<UAFLBeamVisualComponent> WeaponVisual;
};
