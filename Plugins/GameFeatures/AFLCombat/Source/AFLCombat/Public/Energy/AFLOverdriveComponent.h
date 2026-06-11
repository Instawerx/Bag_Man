// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffectTypes.h"

#include "AFLOverdriveComponent.generated.h"

class UAbilitySystemComponent;
class UCharacterMovementComponent;
struct FGameplayTag;
struct FLyraVerbMessage;
struct FOnAttributeChangeData;

/**
 * UAFLOverdriveComponent  (energy cycle 2 -- the threshold consumer; NO GameplayAbility by design)
 *
 * SERVER half: listens Event.Energy.ThresholdReached (server-world-only broadcast, by design) ->
 * applies UAFLGE_OverdriveBuff BY HANDLE (guarded: never while State.Energy.Overdrive already
 * holds) with the periodic drain magnitude set from afl.Energy.DrainPerSecond; removes the handle
 * when CarriedEnergy reaches 0 (attribute-change delegate -- full consumption is the exit; the
 * set's upward-crossing-only broadcast gives re-trigger hysteresis free) and on death
 * (ULyraHealthComponent::OnDeathStarted -- Lyra does NOT clear Infinite GEs on the PlayerState
 * ASC at death/respawn, so an explicit remove ships; the drop-component bind precedent).
 *
 * BOTH-SIDES half (owning client + server -- CMC prediction must agree or the client
 * rubber-bands): RegisterGameplayTagEvent(State.Energy.Overdrive) -> on rise, cache + swap CMC
 * MaxWalkSpeed x1.15; on fall, restore the cache. The dash component's CMC-property-swap
 * precedent verbatim, including its re-entrancy guard: NEVER cache an already-swapped value
 * (bSpeedSwapped gates the cache write, so a duplicate rise event cannot bake the boosted speed
 * in as the restore target).
 *
 * Granted to the hero via the experience AddComponents row, client=true server=true (unlike the
 * server-only drop/hitbox rows -- the speed swap is the client half).
 */
UCLASS(ClassGroup = (AFL), Blueprintable, meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLOverdriveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLOverdriveComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Speed multiplier while overdriven (S7: +15%). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Energy", meta = (ClampMin = "1.0"))
	float SpeedMultiplier = 1.15f;

private:
	/** Deferred arm: the PlayerState ASC may not be resolvable at pawn BeginPlay; poll briefly. */
	void TryArm();

	void HandleThresholdReached(FGameplayTag Channel, const FLyraVerbMessage& Msg);
	void HandleEnergyChanged(const FOnAttributeChangeData& Data);
	void HandleOverdriveTagChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	void RemoveBuff(const TCHAR* Reason);

	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<UCharacterMovementComponent> CMC;

	FGameplayMessageListenerHandle ThresholdListener;
	FDelegateHandle EnergyChangedHandle;
	FDelegateHandle OverdriveTagHandle;
	FActiveGameplayEffectHandle BuffHandle;
	FTimerHandle ArmRetryTimer;

	/** Dash-precedent re-entrancy guard: the cached restore value is written ONCE per swap. */
	bool bSpeedSwapped = false;
	float CachedMaxWalkSpeed = 0.0f;
};
