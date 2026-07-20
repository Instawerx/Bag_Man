// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"

#include "AFLDeployableBarrier.generated.h"

class UStaticMeshComponent;
class ULyraAbilitySystemComponent;
class ULyraHealthSet;
class UAFLAttributeSet_Combat;
struct FOnAttributeChangeData;

/**
 * AAFLDeployableBarrier
 *
 * The deployed cover wall spawned by the Shield puck on landing (Shield pilot). A LEAN replicated
 * AActor -- deliberately NOT a pawn: a pawn would be counted by the round/team/alive-player logic
 * (spawn selector, EQS, scoring) and corrupt the match. It carries only what it needs to be a
 * destructible, damageable obstacle.
 *
 * DAMAGE MODEL (harvest, not parallel-build): AFL weapons apply GAS damage GEs through
 * UAFLDamageExecCalc, which outputs to ULyraHealthSet.Damage and captures UAFLAttributeSet_Combat's
 * Armor/Shield/zone attributes. So the barrier self-grants BOTH sets on a self-owned
 * ULyraAbilitySystemComponent -- the exact stack B_AFL_TargetDummy proved takes AFL weapon damage.
 * Armor/Shield/zones default to 0 (their ctors) so every hit lands straight on Health; being a
 * STATIC mesh (no skeleton -> no bone -> no zone route) keeps the ExecCalc on its no-bone->Health
 * fallback, so a hit anywhere on the wall drains HP. At Health<=0 it breaks (OnOutOfHealth-equivalent
 * via the ASC Health-change delegate). Independently a lifetime timer despawns it after Duration.
 *
 * FULL BLOCK: the mesh blocks bodies + projectiles + hitscan/beams (BlockProfileName, default
 * BlockAll) -- "passes nothing." The shot that STOPS at the wall is the same shot that DAMAGES it
 * (hitscan hits the wall's collision = the GE target; the rocket detonates against it).
 *
 * Team/friendly-fire: PILOT is damageable-by-anyone (no team interface) so the deployer can prove
 * "shoot it down." Team-scoped friendly-fire is a later knob.
 */
UCLASS()
class AFLCOMBAT_API AAFLDeployableBarrier : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AAFLDeployableBarrier();

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End

protected:

	virtual void BeginPlay() override;

	/** Root: the wall mesh (placeholder cube for the pilot -> conformed Tripo barrier at MEET). Blocks all. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AFL|Barrier")
	TObjectPtr<UStaticMeshComponent> BarrierMesh;

	/** Self-owned ASC so AFL weapon damage GEs have a target. Minimal replication (no player). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AFL|Barrier")
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;

	/** The Lyra health the ExecCalc outputs damage to (Health/MaxHealth/Damage meta). */
	UPROPERTY()
	TObjectPtr<ULyraHealthSet> HealthSet;

	/** The AFL combat set the ExecCalc captures (Armor/Shield/zones -> all 0 -> damage passes to Health). */
	UPROPERTY()
	TObjectPtr<UAFLAttributeSet_Combat> CombatSet;

	// ---- DESIGN KNOBS (operator-tuned) ----

	/** Seconds before the barrier auto-despawns. 0 = never (destructible-only). Pilot ~15-20s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Barrier|Tuning", meta=(ClampMin="0.0"))
	float Duration = 17.0f;

	/** Hit points. Health + MaxHealth are seeded to this on BeginPlay; at <=0 the barrier breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Barrier|Tuning", meta=(ClampMin="1.0"))
	float MaxHP = 300.0f;

	/** Collision profile the mesh uses. Default BlockAll = "passes nothing" (bodies + shots + beams). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|Barrier|Tuning")
	FName BlockProfileName = FName(TEXT("BlockAll"));

	/**
	 * Break FX hook -- authored in the BP child (a GameplayCue/Niagara + audio), so the look is
	 * content while this C++ stays structural. bDestroyed = shot down (true) vs timed out (false).
	 * The actor Destroy()s right after this fires (a short lifespan lets the FX spawn).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="AFL|Barrier")
	void OnBarrierBroken(bool bDestroyed);

private:

	/** Bound to the ASC's Health attribute-change delegate; breaks the barrier when Health crosses <=0. */
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	/** Duration timer callback -> break (timed out). */
	void HandleLifetimeExpired();

	/** One-shot break: fire OnBarrierBroken(bDestroyed) then despawn. Guarded so it runs exactly once. */
	void BreakBarrier(bool bDestroyed);

	bool bBroken = false;
	FTimerHandle LifetimeTimerHandle;
};
