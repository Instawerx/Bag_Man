// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Character/LyraCharacterMovementComponent.h"
#include "GameplayTagContainer.h"

#include "AFLCharacterMovementComponent.generated.h"

class UAbilitySystemComponent;
struct FGameplayTag;

/**
 * UAFLCharacterMovementComponent
 *
 * Sprint 3 Dash Movement Contract — see §9.6 of Docs/BAG_MAN_MASTER_BUILD_v2.0.md.
 *
 * Listens to State.Movement.Dashing on the owning pawn's ASC. On tag-add,
 * caches current GroundFriction + AirControl values and applies the dash
 * tuning (low friction, raised air-control). On tag-remove, restores the
 * cached values.
 *
 * Critical rule: values are cached at dash ENTRY, not at component
 * construction. This guarantees restore returns to the real pre-dash
 * state, including future systems that may have modified friction or
 * air-control (leg-loss, temporary buffs) at the moment dash begins.
 */
UCLASS(Config = Game)
class AFLMOVEMENT_API UAFLCharacterMovementComponent : public ULyraCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UAFLCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/** Sprint 3 tuning targets (§9.6). Editable in the CDO for designer tuning. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Movement|Dash")
	float DashGroundFriction = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Movement|Dash")
	float DashAirControl = 0.6f;

	/** True while State.Movement.Dashing has caused us to swap friction/air-control. */
	UFUNCTION(BlueprintPure, Category = "AFL|Movement|Dash")
	bool IsDashTuningActive() const { return bDashTuningActive; }

protected:
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void OnUnregister() override;

private:
	/** Bind/unbind the tag-change delegate on the owning pawn's ASC. */
	void TryBindToAbilitySystem();
	void UnbindFromAbilitySystem();

	/** Tag-change callback wired via UAbilitySystemComponent::RegisterGameplayTagEvent. */
	void HandleDashTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Apply the dash friction/air-control. Idempotent — guarded by bDashTuningActive. */
	void ApplyDashTuning();

	/** Restore cached friction/air-control. Idempotent. */
	void RestoreDashTuning();

	/** Cached at dash entry, restored at dash exit. -1.0f = uninitialized. */
	float CachedGroundFriction = -1.0f;
	float CachedAirControl = -1.0f;

	/** True while dash tuning is applied. Prevents double-cache/double-restore. */
	bool bDashTuningActive = false;

	/** Cached weak ref to the owning pawn's ASC. Re-resolved on null. */
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	/** Delegate handle for the tag-change registration. */
	FDelegateHandle DashTagChangedHandle;
};
