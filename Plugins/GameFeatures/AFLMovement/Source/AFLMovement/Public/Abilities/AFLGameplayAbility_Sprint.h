// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_Sprint.generated.h"

class UGameplayEffect;

/**
 * UAFLGameplayAbility_Sprint  (Movement Overhaul — Phase 1: hold-to-sprint)
 *
 * The sprint gameplay ability — the pure feel-STATE half; UAFLSprintMovementComponent owns the CMC swap.
 * Parented to ULyraGameplayAbility (same as Dash/Climb), Local-predicted, server-validated. Abstract —
 * the GA_AFL_Sprint BP child sets SprintActiveEffectClass.
 *
 * Held lifecycle (the Climb/Beam_v2 pattern): ActivationPolicy = WhileInputActive activates the ability
 * ONCE for the whole hold — Lyra's ProcessAbilityInput does NOT re-fire it every frame. A WaitInputRelease
 * task ends it on release. WITHOUT WhileInputActive the input re-triggers ~every frame and the ability
 * re-activates repeatedly.
 *
 * Lifecycle:
 *   ActivateAbility: CommitAbility; (optional) require grounded; apply SprintActiveEffectClass
 *     (grants State.Movement.Sprinting -> the component swaps MaxWalkSpeed/MaxAcceleration); bind
 *     WaitInputRelease -> EndAbility.
 *   EndAbility: remove the active GE (tag clears -> the component restores the CMC).
 *
 * No motion of its own — the locomotion blendspace covers the higher speed.
 *
 * Native defaults (locked): InstancingPolicy = InstancedPerActor, NetExecutionPolicy = LocalPredicted,
 * ActivationPolicy = WhileInputActive.
 * BP-configured (GA_AFL_Sprint child): SprintActiveEffectClass -> GE_AFL_Sprint_Active_C.
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_Sprint : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UGameplayAbility interface
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
	//~End of UGameplayAbility interface

	/** Duration GE applied on activation — grants State.Movement.Sprinting (mirror GE_AFL_Dash_Active). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Sprint")
	TSubclassOf<UGameplayEffect> SprintActiveEffectClass;

	/** Require the character be grounded (not falling) to begin sprinting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Sprint")
	bool bRequireGrounded = true;

	/** LEASE WINDOW for the event-driven (bot) path. A GameplayEvent has no release, so the bot path cannot
	 *  end the way the human path does -- it must expire. The requester re-sends the event on its own cadence;
	 *  each one re-arms this window, and the window firing IS the end of the sprint.
	 *
	 *  Must exceed the requester's tick interval with margin: the BT service ticks at 0.5s, so 1.25s tolerates
	 *  two consecutive missed renewals before sprint drops. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Sprint")
	float BotSprintLeaseSeconds = 1.25f;

private:
	/** Input-release callback (WaitInputRelease) -> end the sprint. HUMAN PATH ONLY. */
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	/** Renewal callback -- another Sprint.Requested arrived while already sprinting. BOT PATH ONLY. */
	UFUNCTION()
	void OnSprintRenewed(FGameplayEventData Payload);

	/** (Re)arm the lease. Called on activation and on every renewal. */
	void RenewBotLease();

	/** Lease ran out -- nobody asked for sprint recently enough. End it. */
	void OnBotLeaseExpired();

	/** First-exit-wins guard so a duplicate release / task-end can't double-EndAbility. */
	bool bEnding = false;

	/** One-shot, re-armed on every renewal. Firing means expiry. */
	FTimerHandle BotLeaseTimer;
};
