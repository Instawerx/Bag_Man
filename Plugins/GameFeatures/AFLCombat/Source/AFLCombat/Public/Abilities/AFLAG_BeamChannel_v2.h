// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Abilities/AFLAG_Laser_Base.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/EngineTypes.h"

#include "AFLAG_BeamChannel_v2.generated.h"

class UAnimMontage;
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
class AFLCOMBAT_API UAFLAG_BeamChannel_v2 : public UAFLAG_Laser_Base
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

	/**
	 * GATED HEAT (Beam Cutter, weapon #8) -- ALL THREE knobs default null, so the live ShotgunBeam
	 * (which leaves them unset) stays byte-identical (the bHoming/bSpreadCone gated-addition
	 * pattern). A BP child sets all three together to enable the limiter: the retired beam's
	 * PROVEN triad (AFLAG_Laser_Beam / AFL-0207) -- build + gate + drain -- transplanted intact.
	 *
	 * Per-tick heat GE, applied source-to-self on the AUTHORITY each channel tick regardless of
	 * hit (heat is a hold-time budget, not an on-hit tax). The AttributeSet folds the delta into
	 * Heat and grants replicated State.Overheated at MaxHeat; the mid-channel check in
	 * ServerApplyTargetData then force-ends the channel. Cutter uses GE_AFL_Heat_CutterTick
	 * (2.5/tick vs MaxHeat 100 = ~4s continuous cut). Null = no heat (ShotgunBeam).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2|Heat")
	TSubclassOf<UGameplayEffect> HeatPerTickEffectClass;

	/**
	 * 0.5s carrier GE granting State.Combat.CoolingGate (GE_AFL_Heat_CoolingGate), refreshed every
	 * authoritative tick so the passive drain is fully suppressed WHILE firing -- without it,
	 * Heat_Decay fights the build mid-channel and the window math goes mushy. Null = off.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2|Heat")
	TSubclassOf<UGameplayEffect> HeatCoolingGateEffectClass;

	/**
	 * Infinite periodic drain GE (GE_AFL_Heat_Decay: Heat -= HeatDecayRate*0.1 @ 10 Hz, paused by
	 * the CoolingGate tag; the AttributeSet clears State.Overheated below MaxHeat*0.3). Ensured
	 * idempotently on the source ASC at activation (authority) -- the retired beam's ensure shape.
	 * WITHOUT this ensure heat would build once and never drain (permanent overheat). Null = off.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2|Heat")
	TSubclassOf<UGameplayEffect> HeatDecayEffectClass;

	/** Channel tick interval (s). 0.1 = 10 ticks/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float TickInterval = 0.1f;

	/** Max camera trace distance (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2|Trace", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float MaxRange = 8000.0f;

	/** Trace channel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/**
	 * Character fire montage -- the 2H brace / trigger-pull, played ONCE at beam-start via the
	 * GAS-replicated PlayMontage path (ASC->PlayMontage in ActivateAbility, which runs on the
	 * autonomous proxy + authority for this LocalPredicted ability), so the authority's
	 * RepAnimMontageInfo replicates the brace down to REMOTE clients (sim proxies) -- riding the SAME
	 * already-replicated delivery the beam visual uses (replicated UAFLBeamVisualComponent), no new
	 * plumbing. Null by default (the live Beam_v2 is unaffected); the Shotgun-Beam BP child sets
	 * AM_MM_Shotgun_Fire (2H additive).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BeamV2|FX")
	TObjectPtr<UAnimMontage> CharacterFireMontage;

private:
	/** Locally-controlled per-tick trace + publish (timer-driven). */
	void TickChannel();

	// ResolveMuzzleLocation now lives on UAFLAG_Laser_Base (shared with Pulse -- one resolver, no twin).

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
