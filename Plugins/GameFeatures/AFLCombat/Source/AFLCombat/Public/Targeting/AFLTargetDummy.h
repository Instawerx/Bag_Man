// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacterWithAbilities.h"

#include "AFLTargetDummy.generated.h"

class UAFLDeathComponent;
class UAFLAttributeSet_Combat;
class UAFLPawnHitboxHistoryComponent;
class ULyraHealthSet;                 // CONVERGENCE: the react now binds THIS set's OnHealthChanged
struct FGameplayEffectSpec;           // FLyraAttributeEvent payload (react handler param)


/**
 * AAFLTargetDummy
 *
 * The first CONSUMER of the AFL death system (UAFLDeathComponent), and the standing combat
 * target every future test runs against. It is NOT where death logic lives -- it just grants
 * the reusable death component and reacts to damage. The player and every enemy reuse the same
 * UAFLDeathComponent; this dummy proves it works end-to-end before the player inherits it.
 *
 * Base = ALyraCharacterWithAbilities: a real Lyra pawn that owns its OWN typed
 * ULyraAbilitySystemComponent in its ctor, so AFLCombat's GameFeatureAction_AddAbilities
 * CastChecked<ULyraAbilitySystemComponent> succeeds (BM-DEBT-004-safe -- the unpossessed-
 * ALyraCharacter crash does not fire). It also inherits ALyraCharacter's ULyraHealthComponent,
 * whose replicated StartDeath/FinishDeath the UAFLDeathComponent drives from the AFL signal.
 *
 * The AFL combat attribute set (UAFLAttributeSet_Combat, Health=100 + GE_AFL_Combat_InitData)
 * is granted by the experience's GameFeatureAction_AddAbilities entry keyed on this class
 * (the proven BM-0102 1a path) -- the same DA_AFL_Combat_AbilitySet the player gets. Mesh,
 * placement, and the hit-react VISUALS are content/BP on top of this C++ base.
 */
UCLASS()
class AFLCOMBAT_API AAFLTargetDummy : public ALyraCharacterWithAbilities
{
	GENERATED_BODY()

public:
	AAFLTargetDummy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * TEST-RIG (BM-0105 lag-comp watch, NOT gameplay): server-side sinusoidal LATERAL (Y) sweep
	 * around the placed origin. Lag-comp only DEMONSTRATES against a MOVING target -- a stationary
	 * dummy's hits land under latency whether or not lag-comp exists (no rewind needed if nothing
	 * moved). The lateral displacement over the rewind window is what makes a confirmed hit under
	 * latency PROVE the rewind compensated. Authority-driven + replicated so the server position
	 * (what ConfirmHit rewinds) diverges from the client's latency-delayed view -- the whole point.
	 * Mirrors AAFLLagTestDummy's proven sweep.
	 */
	UPROPERTY(EditAnywhere, Category = "AFL|Target|TestRig")
	float SweepAmplitude = 200.0f;   // cm to each side

	UPROPERTY(EditAnywhere, Category = "AFL|Target|TestRig")
	float SweepFrequency = 1.5f;     // rad/s

	/** When false, the dummy stays put (for the no-movement baseline / non-lag-comp tests). */
	UPROPERTY(EditAnywhere, Category = "AFL|Target|TestRig")
	bool bEnableLateralSweep = true;

	/**
	 * Bound to ULyraHealthSet::OnHealthChanged for the per-hit react (CONVERGENCE: Health lives on the Lyra set
	 * now). The visible react itself (flash MID / hit montage) is a BlueprintImplementableEvent so the look is
	 * authored in the BP child (operator-tunable), keeping this C++ base structural-only.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Target")
	void OnDamageReact(float Magnitude);

	/** The reusable AFL death driver (Health<=0 -> Lyra's replicated death sequence). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Target")
	TObjectPtr<UAFLDeathComponent> DeathComponent;

	/**
	 * TEST-RIG: per-pawn 60Hz hitbox-history publisher the lag-comp rewind reads. ConfirmHit
	 * rewinds via UAFLLagCompensationWorldSubsystem, which only knows targets that registered a
	 * UAFLPawnHitboxHistoryComponent -- without this, the dummy is invisible to the rewind and
	 * BM-0105 can't run against it. Self-registers (server-only) in its own BeginPlay; no grant.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Target|TestRig")
	TObjectPtr<UAFLPawnHitboxHistoryComponent> HitboxHistory;

private:
	/** CONVERGENCE: ULyraHealthSet::OnHealthChanged is FLyraAttributeEvent (6-param). The signed Health delta is
	 *  NewValue-OldValue (NOT EffectMagnitude, which for the Damage meta is the positive damage, not the delta). */
	void HandleHealthChanged(AActor* EffectInstigator, AActor* EffectCauser, const FGameplayEffectSpec* EffectSpec, float EffectMagnitude, float OldValue, float NewValue);

	/**
	 * TEST-RIG visible-death (NOT gameplay): ragdoll the mannequin mesh on death so the kill is
	 * unmistakable on screen (Lyra's OnDeathStarted only DisableMovementAndCollision()s then
	 * destroys -- the body just freezes+vanishes, which read as "did it die?"). Bound additively
	 * to this dummy's ULyraHealthComponent::OnDeathStarted (driven by UAFLDeathComponent ->
	 * StartDeath). Host-visible only; MP-correct death visuals belong in a replicated death
	 * GameplayCue later (same architecture as the beam cue) -- flagged, not built here.
	 */
	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	const ULyraHealthSet* ReactHealthSet = nullptr;   // CONVERGENCE: the Lyra set we bind OnHealthChanged on
	FDelegateHandle HealthChangedHandle;

	/** Sweep center, captured at BeginPlay = the placed location (test-rig lateral patrol). */
	FVector SpawnOrigin = FVector::ZeroVector;
};
