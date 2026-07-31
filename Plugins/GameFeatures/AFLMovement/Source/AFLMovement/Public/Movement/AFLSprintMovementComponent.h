// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "AFLSprintMovementComponent.generated.h"

class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * UAFLSprintMovementComponent  (Movement Overhaul — Phase 1: sprint / faster-run feel-swap)
 *
 * The sprint MaxWalkSpeed / MaxAcceleration feel-swap, delivered the PILLAR-2-CORRECT way: a standalone
 * UActorComponent added to the pawn via GameFeatureAction_AddComponents (the proven AFL attachment path),
 * NOT a UCharacterMovementComponent SUBCLASS.
 *
 * WHY NOT a CMC subclass: a CMC subclass can only be installed via a C++ ctor SetDefaultSubobjectClass,
 * which forces reparenting the hero BP off B_Hero_ShooterMannequin — that silences input + breaks spawn
 * (the verified project failure; see UAFLDashMovementComponent's header). So the feel-swap is hosted in a
 * component that READS the pawn's EXISTING LyraCharacterMovementComponent via GetCharacterMovement() and
 * sets the public MaxWalkSpeed / MaxAcceleration floats directly. No reparent, foundation intact.
 *
 * MECHANISM (Dash-component tag-listener + Overdrive speed-swap, fused):
 * listens to State.Movement.Sprinting on the owner ASC (granted while the sprint input is held by
 * GE_AFL_Sprint_Active, applied by GA_AFL_Sprint). On tag-add: cache the CMC's CURRENT MaxWalkSpeed /
 * MaxAcceleration (so restore returns to the real pre-sprint state, e.g. including a concurrent Overdrive
 * buff) and multiply them. On tag-remove: restore the cache. Re-entrancy guard (bSprintSwapped) so a
 * duplicate rise event can never bake the boosted value in as the restore target (the Overdrive precedent).
 * Runs BOTH client + server (the swap keys off the replicated tag) so CMC prediction agrees — no rubber-band.
 *
 * ASC bind mirrors UAFLDashMovementComponent: direct GetAbilitySystemComponentFromActor first (self-ASC'd
 * pawns), ULyraPawnExtensionComponent::OnAbilitySystemInitialized_RegisterAndCall fallback for the possessed
 * PLAYER whose PlayerState ASC lands after pawn BeginPlay.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLSprintMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLSprintMovementComponent();

	/** MaxWalkSpeed multiplier while sprinting, applied over the LIVE base (e.g. the Pro pawn's tuned speed). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Sprint", meta = (ClampMin = "1.0"))
	float SprintSpeedMultiplier = 1.4f;

	/** MaxAcceleration multiplier while sprinting (snappier ramp). 1.0 leaves acceleration untouched. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Sprint", meta = (ClampMin = "1.0"))
	float SprintAccelMultiplier = 1.15f;

	UFUNCTION(BlueprintPure, Category = "AFL|Movement|Sprint")
	bool IsSprinting() const { return bSprintSwapped; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Deferred-player ASC-ready callback (fires via the PawnExtension hook). */
	void OnAbilitySystemReady();

	/** Bind/unbind the State.Movement.Sprinting tag event on the owner's ASC. */
	void BindToAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindFromAbilitySystem();

	/** Tag-change callback: tag-add -> ApplySprintTuning, tag-remove -> RestoreSprintTuning. */
	void HandleSprintTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Resolve the owner's CharacterMovementComponent (the pawn's EXISTING stock CMC). */
	UCharacterMovementComponent* GetOwnerCMC() const;

	void ApplySprintTuning();
	void RestoreSprintTuning();

	/** Cached at sprint entry, restored at sprint exit. -1.0 = uninitialized. */
	float CachedMaxWalkSpeed = -1.0f;
	float CachedMaxAcceleration = -1.0f;
	bool bSprintSwapped = false;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle SprintTagChangedHandle;
};
