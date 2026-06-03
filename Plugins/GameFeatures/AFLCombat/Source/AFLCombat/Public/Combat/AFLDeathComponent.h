// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"   // FGameplayEffectSpec, FActiveGameplayEffectHandle (used by-value in the GE-applied callback)

#include "AFLDeathComponent.generated.h"

class UAbilitySystemComponent;
class UAFLAttributeSet_Combat;
class ULyraHealthComponent;
struct FGameplayEffectSpec;
struct FActiveGameplayEffectHandle;


/**
 * UAFLDeathComponent
 *
 * The reusable, universal death driver for every AFL combatant (the player, the target
 * dummy, and every future enemy). It closes the core combat gap that nothing in BAG MAN
 * could die: AFL runs its own combat-health economy (UAFLAttributeSet_Combat +
 * UAFLDamageExecCalc), parallel to Lyra's ULyraHealthSet, so Lyra's HealthSet-driven death
 * never fired for AFL damage.
 *
 * Doctrine (AFL owns the trigger; Lyra's shipped machinery does the death):
 *   - TRIGGER (AFL-native): binds UAFLAttributeSet_Combat::OnOutOfHealth (Health crossed to
 *     <=0). NOT ULyraHealthSet -- the player drains AFL Health, so death must fire off the AFL
 *     set to be correct for all combatants.
 *   - SEQUENCE (reuse Lyra, networked): on the AFL signal, drive the owner's existing
 *     ULyraHealthComponent::StartDeath() -> (delay) -> FinishDeath(). Those are public,
 *     HealthSet-INDEPENDENT (they only drive the REPLICATED ELyraDeathState + the
 *     Status.Death.Dying/Dead tags + OnDeathStarted/Finished, then ForceNetUpdate) -- so the
 *     full Lyra death runs networked: DeathState replicates, OnRep_DeathState ragdolls/cleans
 *     up on every client. We replace the trigger source, NOT the death implementation -- no
 *     hand-rolled, host-only SetSimulatePhysics.
 *
 * Overkill is ORTHOGONAL and untouched here: Event.Damage.Overkill is a per-hit gib qualifier
 * (fires on a big hit without killing; a small finishing shot kills without it). It stays the
 * dismemberment channel (UAFLDismemberComponent / the AFLOverkillListener test sink consume the
 * fan-out bus); this component does NOT subscribe to it. Death = Health<=0; gib = Overkill.
 *
 * Authority-only: StartDeath/FinishDeath run on the server and replicate down. The component
 * is added uniformly to combatants; the death drive is server-gated.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLDeathComponent();

	/** Find the UAFLDeathComponent on an actor (mirrors ULyraHealthComponent::FindHealthComponent). */
	UFUNCTION(BlueprintPure, Category = "AFL|Death")
	static UAFLDeathComponent* FindDeathComponent(const AActor* Actor);

	/**
	 * Wire to the owner's ASC: locate UAFLAttributeSet_Combat and bind OnOutOfHealth. Called
	 * from BeginPlay; combatants whose ASC initializes later (player via PlayerState) can call
	 * this explicitly once their ASC is ready. Idempotent.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Death")
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	/** Unbind (mirrors ULyraHealthComponent::UninitializeFromAbilitySystem). */
	UFUNCTION(BlueprintCallable, Category = "AFL|Death")
	void UninitializeFromAbilitySystem();

	/** Seconds between StartDeath and FinishDeath/cleanup. Consumers tune it (dummy holds longer for a visible ragdoll; the player sets bDestroyOnDeathFinish=false). */
	void SetDeathFinishDelay(float InSeconds) { DeathFinishDelay = InSeconds; }
	void SetDestroyOnDeathFinish(bool bInDestroy) { bDestroyOnDeathFinish = bInDestroy; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** PawnExtension OnAbilitySystemInitialized callback -- resolve the ASC + bind (Lyra pattern). */
	void OnAbilitySystemReady();

	/**
	 * Retry hook for the DEFERRED AFL combat set. The set is granted via the experience's
	 * AddAbilities action, which can land AFTER ASC init -- so if the set wasn't present at
	 * InitializeWithAbilitySystem, we listen for GE-applied (GE_AFL_Combat_InitData seeds the
	 * set) and bind OnOutOfHealth once it appears. Fixes the "ASC has no UAFLAttributeSet_Combat"
	 * race that left death unarmed.
	 */
	void OnGameplayEffectAddedToSelf(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);

	/** Bound to UAFLAttributeSet_Combat::OnOutOfHealth. Authority drives Lyra's death sequence. */
	void HandleAFLOutOfHealth(AActor* Instigator, AActor* Causer, float Magnitude);

	/** FinishDeath after the dying->dead delay (ragdoll settle), so cleanup ordering matches Lyra. */
	void HandleFinishDeath();

	/** Seconds between StartDeath (ragdoll begins) and FinishDeath (cleanup). Matches Lyra's feel. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Death", meta = (ClampMin = "0.0"))
	float DeathFinishDelay = 2.0f;

	/** Whether to auto-destroy the owning actor on FinishDeath. Default FALSE = player-safe: the
	 *  player yields pawn teardown to Lyra's death->respawn flow (GA_AutoRespawn + ALyraCharacter::
	 *  UninitAndDestroy), so the controller-side inventory/quickbar tear down coherently and the
	 *  respawned pawn can re-equip. Transient combatants with no respawn flow (the target dummy)
	 *  opt back into TRUE in their ctor so their proven self-destruct death is unchanged. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Death")
	bool bDestroyOnDeathFinish = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	/** The AFL combat set we listen to (resolved from the ASC). */
	const UAFLAttributeSet_Combat* CombatSet = nullptr;

	/** The owner's Lyra health component, if any -- we drive its replicated death sequence. */
	UPROPERTY(Transient)
	TObjectPtr<ULyraHealthComponent> LyraHealthComponent = nullptr;

	FDelegateHandle OutOfHealthHandle;
	FDelegateHandle GESpawnedHandle;   // listens for the deferred AFL-set grant (GE-applied)
	FTimerHandle FinishDeathTimer;
	bool bDeathStarted = false;
};
