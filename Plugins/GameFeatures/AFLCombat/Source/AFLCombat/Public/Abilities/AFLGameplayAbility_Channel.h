// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffectTypes.h"

#include "AFLGameplayAbility_Channel.generated.h"

class UGameplayEffect;
struct FAFLHitConfirmMessage;

/**
 * UAFLGameplayAbility_Channel  (Loot-Carry Phase B -- the reusable timed-channel BASE ability)
 *
 * Generalizes UAFLAG_Extract's PROVEN channel mechanism (it is NOT a component -- per
 * unreal-engine-expert: GAS owns activatable timed actions; never re-roll a timer/lock/cancel outside
 * GAS). The CARRY collect-channel subclasses this now; HARVEST (Phase 4) subclasses it too. Extract
 * itself is UNTOUCHED -- this base MIRRORS its shape, it does not refactor it.
 *
 * The mechanism (verbatim from extract, minus the extraction-specific energy/zone bits):
 *  - duration via UAbilityTask_WaitDelay (a timed commitment).
 *  - ChannelEffectClass (UAFLGE_Channel) by handle -> State.Channeling ONLY. NO movement lock -- the
 *    REQUIRED divergence from extract: Decision 3 is move-cancel ("moving away cancels it"), not frozen.
 *    Applied in ActivateAbility, removed in EndAbility on EVERY exit path.
 *  - INTERRUPTS, every non-complete end broadcasts Event.Channel.Interrupted from the EndAbility funnel:
 *      * Damage:   Event.Damage.Confirmed listener, filtered Target==Avatar -> cancel.
 *      * Movement: a NEW start-location position-delta poll (MaxMoveRadius). There is NO extract
 *                  precedent -- extract used a zone-exit TAG; a collect/harvest channel has no zone, so
 *                  this is a manual radius check on a timer (NOT Tick, per the skill).
 *      * External: death / teardown lands in the same EndAbility cleanup.
 *
 * TARGET (Decision D -- the divergence from extract): extract has no target (it channels in a zone and
 * pays from the pawn's own energy). A collect/harvest channel channels AGAINST a specific object and
 * pays FROM it. So this base is GAMEPLAY-EVENT-TRIGGERED: the trigger (the grab ability) sends a
 * GameplayEvent carrying Target = the channeled object; the base captures TriggerEventData->Target and
 * hands it to the subclass's OnChannelComplete. Subclasses set AbilityTriggers (their event tag).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLGameplayAbility_Channel : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_Channel();

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

	/** Subclass payoff hook -- the channel completed successfully (authority only). TargetActor = the
	 *  channeled object from the trigger event (null for a self-channel). The collect-channel grants from
	 *  it; HARVEST yields from it. Base does NOT call this on a client (Watts/pool replicate down). */
	virtual void OnChannelComplete(const AActor* TargetActor) {}

	/** Channel length (seconds). The CARRY collect-channel is short (~1.5s); HARVEST sets its own. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Channel", meta = (ClampMin = "0.1"))
	float ChannelDuration = 1.5f;

	/** The channel-window GE (State.Channeling; NO movement lock -- move-cancel per Decision 3). Defaults to UAFLGE_Channel. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Channel")
	TSubclassOf<UGameplayEffect> ChannelEffectClass;

	/** Moving farther than this (cm) from the start point cancels the channel -- the NEW position-delta
	 *  interrupt (no extract precedent). 0 = no movement cancel. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Channel", meta = (ClampMin = "0.0"))
	float MaxMoveRadius = 120.0f;

	/** Poll interval (s) for the move-radius check (a timer, not Tick). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Channel", meta = (ClampMin = "0.05"))
	float MoveCheckInterval = 0.15f;

private:
	UFUNCTION()
	void HandleChannelComplete();

	void HandleDamageConfirmed(FGameplayTag Channel, const FAFLHitConfirmMessage& Msg);
	void CheckMoveRadius();
	void BroadcastChannelEvent(const FGameplayTag& EventTag, float Magnitude) const;

	FActiveGameplayEffectHandle ChannelEffectHandle;
	FGameplayMessageListenerHandle DamageListener;
	FTimerHandle MoveCheckTimer;
	FVector ChannelStartLocation = FVector::ZeroVector;
	TWeakObjectPtr<const AActor> ChannelTarget;
	bool bChannelCompleted = false;
};
