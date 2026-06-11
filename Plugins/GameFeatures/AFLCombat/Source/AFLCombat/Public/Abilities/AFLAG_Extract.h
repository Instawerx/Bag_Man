// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffectTypes.h"

#include "AFLAG_Extract.generated.h"

class UGameplayEffect;
struct FAFLHitConfirmMessage;

/**
 * UAFLAG_Extract  (extraction cycle 1 -- S8 AFL-0803: the P2 cash-out channel)
 *
 * 6s timed channel (UAbilityTask_WaitDelay -- NOT the beam's WhileInputActive shape: this is a
 * timed commitment, not a held input). Gating is pure tags:
 *  - ActivationRequiredTags = State.InExtractionZone (dispensed by AAFLExtractionZone's GE).
 *  - ActivationOwnedTags    = State.Extracting (instant local), mirrored by the channel GE
 *    (replicated truth) -- UAFLGE_ExtractChannel also grants Gameplay.MovementStopped (O1
 *    hard-lock: Lyra CMC zeroes speed natively).
 *
 * COMMIT (server, OnFinish): CarriedEnergy x WattsPerEnergy -> UAFLWalletComponent::
 * EarnWattsAuthority (the CommitMutation funnel; AFL-0805 off-GAS pick) -> CarriedEnergy zeroed
 * through the SAME negative-SetByCaller GE rail as the death burst (never a direct write) ->
 * Event.Extraction.Complete.
 *
 * INTERRUPTS -- every non-complete end broadcasts Event.Extraction.Failed from the EndAbility
 * funnel (single home, no double-fire):
 *  - Damage:    Event.Damage.Confirmed listener (the drop-on-damage consumer pattern), filtered
 *               Target==Avatar -> AFL-0808: UAFLEnergyDropComponent::BurstNow(100%) -> cancel.
 *  - Zone exit: RegisterGameplayTagEvent(State.InExtractionZone) count-0 -> cancel (energy
 *               retained -- leaving is not getting shot).
 *  - External:  death/teardown cancels land in the same EndAbility cleanup.
 *
 * EXIT-PATH AUDIT (all funnel through EndAbility -- the grab-funnel discipline):
 *   (1) success commit, (2) damage interrupt, (3) zone-exit tag-drop, (4) external cancel
 *   (death CancelAbilities / spec removal), (5) avatar teardown. EndAbility removes the channel
 *   GE by handle, unregisters both listeners, and emits Failed when the commit never happened.
 */
UCLASS()
class AFLCOMBAT_API UAFLAG_Extract : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLAG_Extract();

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

	/** Channel length (seconds). S8 AFL-0803 spec value. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction", meta = (ClampMin = "0.1"))
	float ChannelDuration = 6.0f;

	/** Cash-out rate (locked pick: 1 energy -> 10 Watts; ~4k W/match economy budget). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction", meta = (ClampMin = "0.0"))
	float WattsPerEnergy = 10.0f;

	/** The channel-window GE (State.Extracting + Gameplay.MovementStopped). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	TSubclassOf<UGameplayEffect> ChannelEffectClass;

private:
	UFUNCTION()
	void HandleChannelComplete();

	void HandleDamageConfirmed(FGameplayTag Channel, const FAFLHitConfirmMessage& Msg);
	void HandleZoneTagChanged(const FGameplayTag Tag, int32 NewCount);
	void BroadcastExtractionEvent(const FGameplayTag& EventTag, float Magnitude) const;

	FActiveGameplayEffectHandle ChannelEffectHandle;
	FGameplayMessageListenerHandle DamageListener;
	FDelegateHandle ZoneTagHandle;
	bool bChannelCompleted = false;
};
