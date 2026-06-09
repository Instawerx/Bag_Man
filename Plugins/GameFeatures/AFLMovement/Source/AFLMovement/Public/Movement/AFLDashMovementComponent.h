// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "AFLDashMovementComponent.generated.h"

class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * UAFLDashMovementComponent
 *
 * Sprint 3 Dash Movement Contract (AFL-0304-B) — the friction/air-control feel-swap,
 * delivered the PILLAR-2-CORRECT way: a standalone UActorComponent added to the pawn
 * via GameFeatureAction_AddComponents (the proven AFL attachment path — same as
 * UAFLDeathComponent / UAFLSkinColorComponent / UAFLPawnHitboxHistoryComponent), NOT a
 * UCharacterMovementComponent SUBCLASS.
 *
 * WHY NOT a CMC subclass: a CMC subclass can only be installed via a C++ ctor
 * SetDefaultSubobjectClass, which forces reparenting the hero BP onto a custom
 * ACharacter. B_Hero_BagMan MUST stay a child of B_Hero_ShooterMannequin (the proven
 * walker) — its ULyraHeroComponent + populated DefaultInputMappings + spawn/movement
 * init are load-bearing and inherited; reparenting off it silences input + breaks spawn
 * (the original project failure, and verified 2026-06-08). So the feel-swap is hosted in
 * a component that READS the pawn's EXISTING LyraCharacterMovementComponent via
 * GetCharacterMovement() and sets the public GroundFriction/AirControl floats directly.
 * No reparent, foundation intact.
 *
 * MECHANISM (lifted verbatim from the former UAFLCharacterMovementComponent tag-listener):
 * listens to State.Movement.Dashing on the owner's ASC (granted for 0.12s by
 * GE_AFL_Dash_Active, applied by GA_AFL_Dash). On tag-add: cache the CMC's CURRENT
 * GroundFriction/AirControl (so restore returns to the real pre-dash state, including
 * future buffs) and apply the dash tuning. On tag-remove: restore the cached values.
 *
 * ASC bind: direct GetAbilitySystemComponentFromActor first (covers self-ASC'd pawns),
 * ULyraPawnExtensionComponent::OnAbilitySystemInitialized_RegisterAndCall fallback for
 * the possessed PLAYER whose PlayerState ASC lands after pawn BeginPlay — the exact
 * pattern proven on this hero by UAFLDeathComponent.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLDashMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLDashMovementComponent();

	/** Sprint 3 tuning targets (§9.6). Editable on the component default for designer tuning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Dash")
	float DashGroundFriction = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Dash")
	float DashAirControl = 0.6f;

	UFUNCTION(BlueprintPure, Category = "AFL|Movement|Dash")
	bool IsDashTuningActive() const { return bDashTuningActive; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Deferred-player ASC-ready callback (fires via the PawnExtension hook). */
	void OnAbilitySystemReady();

	/** Bind/unbind the State.Movement.Dashing tag event on the owner's ASC. */
	void BindToAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindFromAbilitySystem();

	/** Tag-change callback: tag-add -> ApplyDashTuning, tag-remove -> RestoreDashTuning. */
	void HandleDashTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Resolve the owner's CharacterMovementComponent (the pawn's EXISTING stock CMC). */
	UCharacterMovementComponent* GetOwnerCMC() const;

	void ApplyDashTuning();
	void RestoreDashTuning();

	/** Cached at dash entry, restored at dash exit. -1.0 = uninitialized. */
	float CachedGroundFriction = -1.0f;
	float CachedAirControl = -1.0f;
	bool bDashTuningActive = false;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle DashTagChangedHandle;
};
