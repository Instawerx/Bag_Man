// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/EngineTypes.h"

#include "AFLAG_Laser_Beam.generated.h"

class UGameplayEffect;
class UAFLBeamChannelComponent;
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

	/**
	 * Adds the per-instance vent gate on top of the stock checks: once this cannon has overheated it
	 * cannot re-channel until ITS OWN HeatNorm has decayed to HeatVentResumeNorm. Replaces the pawn-wide
	 * State.Overheated ActivationBlockedTags gate as the thing normal beam fire trips -- that tag stays
	 * in the container (so the ForceOverheat cheat still locks the beam out) but the beam no longer
	 * drives it, which is what decouples two arm-cannons from each other.
	 */
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

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
	 * Per-tick damage GE. Defaults to GE_AFL_Damage_BeamTick (1.2 dmg). BP
	 * children can swap in designer-tuned variants once AFL-0214 lands
	 * (parallel to Pulse).
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

	/**
	 * Per-tick heat GE (GE_AFL_Heat_BeamTick, Override HeatPerBeamTick +4 -> the AttributeSet folds it
	 * into the shared Heat pool and grants State.Overheated at the cap). AFL-0207.
	 *
	 * ⚠ BLOCK 19: THE BEAM NO LONGER APPLIES THIS. Heat is now per ability instance (HeatPerTick below),
	 * because the pool and its State.Overheated tag are per-PAWN and coupled the two arm-cannons. The
	 * property and its default are retained so the GE stays reachable and no content reference breaks,
	 * but nothing in this ability applies it. Do not "restore" the apply without re-reading that header
	 * block -- it re-introduces the dual-mount bug.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Heat")
	TSubclassOf<UGameplayEffect> HeatTickEffectClass;

	/**
	 * 0.5s carrier GE granting State.Combat.CoolingGate on the source; suppressed passive Heat decay
	 * while firing. AFL-0207.
	 *
	 * ⚠ BLOCK 19: THE BEAM NO LONGER APPLIES THIS EITHER -- HeatCoolingGraceSeconds below is its
	 * per-instance replacement. Retained for the same reason as HeatTickEffectClass.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Heat")
	TSubclassOf<UGameplayEffect> HeatCoolingGateEffectClass;

	/**
	 * Infinite Duration GE that periodically decays Heat by HeatDecayRate * 0.1.
	 * The beam ability ensures the GE is present on the source ASC on first
	 * activation; AFL-0214's AbilitySet grant will own this once it lands.
	 * AFL-0207.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Heat")
	TSubclassOf<UGameplayEffect> HeatDecayEffectClass;

	// ---------------------------------------------------------------------------------------------
	// PER-INSTANCE HEAT (Block 19). Replaces the shared-ASC Heat pool as the beam's overheat gate.
	//
	// WHY: Heat is ONE attribute per AbilitySystemComponent and State.Overheated is a pawn-wide loose
	// tag. Two arm-cannons therefore heated a SINGLE gauge at 2x rate and overheated together -- the
	// left's overheat force-ended the right's channel and blocked its re-entry. HeatNorm/bOverheated
	// below are ability-INSTANCE state (each equipment grant is its own FGameplayAbilitySpec, and
	// InstancedPerActor gives each spec its own instance), so each cannon heats and vents alone.
	//
	// The Heat / MaxHeat / HeatDecayRate / HeatPerBeamTick attributes, the four Heat GEs and the
	// afl.Combat.Heat / ForceOverheat / ResetHeat cheats are ALL untouched and still function -- the
	// beam simply no longer drives them. State.Overheated also stays in ActivationBlockedTags, so
	// ForceOverheat still locks the beam out as a diagnostic; nothing in normal beam fire sets it now.
	//
	// Defaults are a 1:1 normalisation of the shared-pool tuning they replace, so channel feel is
	// unchanged:  0.04 = the GE's +4 of MaxHeat 100  ->  25 ticks = 2.5s to overheat.
	//             0.2  = HeatDecayRate 20 of MaxHeat 100.
	//             0.5  = the GE_AFL_Heat_CoolingGate duration, now a grace window (see AdvanceHeat).
	//             0.3  = the AttributeSet's MaxHeat * 0.3 vent-resume threshold -> 3.5s to vent.
	// ---------------------------------------------------------------------------------------------

	/** Heat added per authoritative channel tick, normalised [0..1]. MUST exceed the per-tick decay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Heat", meta=(ClampMin="0.0", UIMin="0.0"))
	float HeatPerTick = 0.04f;

	/** Heat shed per second of IDLE time -- the gap since the last tick BEYOND HeatCoolingGraceSeconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Heat", meta=(ClampMin="0.0", UIMin="0.0"))
	float HeatDecayPerSec = 0.2f;

	/**
	 * Grace window before idle decay starts, seconds -- the per-instance port of GE_AFL_Heat_CoolingGate,
	 * which suppressed passive decay while firing. A gap shorter than this (i.e. a continuous channel at
	 * TickInterval) decays NOTHING, so a held beam heats at the full HeatPerTick exactly as it did.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Heat", meta=(ClampMin="0.0", UIMin="0.0"))
	float HeatCoolingGraceSeconds = 0.5f;

	/**
	 * Vent-resume threshold, normalised. Once overheated the beam cannot re-channel until HeatNorm has
	 * decayed to at or below this -- the port of the AttributeSet clearing State.Overheated below
	 * MaxHeat * 0.3. Hysteresis is the point: without it the beam would re-fire the instant it dipped
	 * off the cap.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Heat", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float HeatVentResumeNorm = 0.3f;

	/** Channel tick interval in seconds. 0.1s = 10 ticks/sec = 12 dps at 1.2 dmg/tick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam", meta=(ClampMin="0.01", UIMin="0.01"))
	float TickInterval = 0.1f;

	/** Maximum trace distance from the camera, centimetres. Matches Pulse default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Trace", meta=(ClampMin="100.0", UIMin="100.0"))
	float MaxRange = 8000.0f;

	/** Collision channel for the per-tick hitscan trace. Visibility matches Lyra's weapon channel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AFL|Beam|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// AFL-0208 (RP-2): the beam VFX is now a GameplayCue (GameplayCue.Weapon.Laser.Beam ->
	// AAFLCueNotify_LaserBeam in the AFLVFX plugin). The beam system, color, and aim-ray
	// origin distance live on the cue + the weapon's DA_AFL_LaserVisual (IAFLLaserVisualProvider),
	// NOT on this ability. The former BeamFXSystem / ImpactFXSystem / BeamVisualOriginDistance
	// UPROPERTYs were removed with the in-ability Niagara spawn.

private:

	/** Called by the FTimerManager on the locally-controlled client every TickInterval. */
	void TickChannel();

	/**
	 * Fold this tick's heat in: shed HeatDecayPerSec for every second of the gap since the last tick
	 * BEYOND HeatCoolingGraceSeconds, then add HeatPerTick, clamped [0..1]. Returns the new HeatNorm.
	 * A continuous channel ticks well inside the grace window, so it sheds nothing and heats at the
	 * full rate -- the per-instance equivalent of the CoolingGate GE suppressing decay while firing.
	 * Called from OnTargetDataReadyCallback on BOTH client and server so the two stay in step.
	 */
	float AdvanceHeat();

	/**
	 * HeatNorm as of NOW, with idle decay applied but NOT committed -- const, so CanActivateAbility can
	 * ask "am I vented yet?" without mutating. ActivateAbility commits the same value.
	 */
	float CurrentHeatNorm() const;

	// --- per-instance heat state. NOT attributes, NOT replicated (each side runs its own ramp off the
	// same per-tick target data, exactly like the hitscan auto-fire model). Both PERSIST across
	// channels: HeatNorm so venting is real time-based cooling rather than a free reset, bOverheated
	// so the lockout survives the force-end. ⚠ Unlike hitscan, EndAbility must NOT clear bOverheated --
	// this ability force-ends ITSELF on overheat, so clearing there would erase the lockout instantly.
	// It clears in ActivateAbility, which CanActivateAbility has already gated on being vented.
	float  HeatNorm            = 0.0f;
	double LastHeatTimeSeconds = 0.0;
	bool   bOverheated         = false;

	/**
	 * AFL-0208: resolve the weapon MUZZLE world location for the visible beam START. Copy of
	 * Pulse's proven UAFLAG_Laser_Pulse::ResolveMuzzleLocation: walk the pawn's attached
	 * actors + the character mesh's children for the SMC carrying a "Muzzle" socket; fall back
	 * to the weapon_r hand socket (NEVER origin) when no Muzzle socket resolves. Published into
	 * UAFLBeamChannelComponent each tick; the cue reads it so the beam emits from the barrel.
	 */
	FVector ResolveMuzzleLocation(class APawn* AvatarPawn) const;

	/**
	 * SIDE-SCOPED muzzle resolve -- the dual-mount (arm-cannon) correct overload. The pawn-scoped
	 * sibling above returns the FIRST "Muzzle" socket across EVERY attached mesh, which is right for a
	 * single held weapon and WRONG for two: both cannons resolved the same first match, so the left
	 * fired from the right's barrel. This confines the search to the actors SourceEquipment itself
	 * spawned, so each hand resolves its own muzzle.
	 *
	 * ⚠ UAFLAG_Laser_Beam is a SIBLING of UAFLAG_Laser_Base, not a child (it derives straight from
	 * ULyraGameplayAbility), so it cannot inherit the base's overload -- it carries its own, same
	 * contract, fed by its own GetAFLLaserWeaponInstance() rather than ResolveLaserVisualProvider().
	 *
	 * Pass GetAFLLaserWeaponInstance(). When it is null (activate-by-class harness, bot GameplayEvent
	 * fire, no current spec) this DELEGATES to the pawn-scoped resolver, so every pre-existing path
	 * keeps its proven behaviour byte-for-byte. Additive: the pawn-scoped signature is unchanged.
	 */
	FVector ResolveMuzzleLocation(UObject* SourceEquipment, class APawn* AvatarPawn) const;

	/**
	 * AFL-0208 (RP-2): the equipment/weapon instance that granted this ability and
	 * supplies the beam look (implements IAFLLaserVisualProvider). Used as the beam
	 * GameplayCue's SourceObject. Resolves from the current ability spec's SourceObject
	 * (the WID AbilitySet grant path sets it to the ULyraEquipmentInstance) -- mirrors
	 * ULyraGameplayAbility_FromEquipment::GetAssociatedEquipment without reparenting.
	 */
	UObject* GetAFLLaserWeaponInstance() const;

	/**
	 * AFL-0208: resolve (and lazily cache) the avatar's UAFLBeamChannelComponent --
	 * the published-value bridge the looping beam cue reads to drive User."Beam End".
	 * Idempotently adds the component to the avatar if absent (same ensure-on-activate
	 * shape as the Heat_Decay GE), so no content-side grant is required. Null only if
	 * there's no avatar actor yet.
	 */
	UAFLBeamChannelComponent* ResolveBeamChannel();

	/** Bound to UAbilityTargetDataSetDelegate. Fires on both client (immediate) and server (replicated). */
	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	/** Server-only per-tick GE apply. Source struct is FAFLAbilityTargetData_Hitscan, same as Pulse. */
	void ServerApplyTargetData(const FGameplayAbilityTargetDataHandle& Data);

	/** Source-side cooldown apply on release. No-op when ReleaseCooldownEffectClass is unset. */
	void ApplyReleaseCooldown();

	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;
	FTimerHandle TickTimerHandle;

	/** Lazily-resolved per-activation cache of the avatar's beam-channel bridge. Cleared in EndAbility. */
	TWeakObjectPtr<UAFLBeamChannelComponent> BeamChannel;
};
