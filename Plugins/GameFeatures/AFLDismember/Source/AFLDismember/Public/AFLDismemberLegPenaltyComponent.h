// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "AFLDismemberLegPenaltyComponent.generated.h"

class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * S4-INC1: the leg-consequence vehicle -- a CMC-tag-listener cloned VERBATIM in shape
 * from the PROVEN UAFLDashMovementComponent. It is the data-driven consequence of a
 * leg dismemberment: while State.Dismembered.LeftLeg OR State.Dismembered.RightLeg holds
 * on the owner's ASC (granted by the leg row's ConsequenceGE), it caches + scales the
 * pawn's MaxWalkSpeed; on the LAST leg tag clearing, it restores.
 *
 * Same Pillar-2-correct shape as the dash component: a standalone UActorComponent added
 * via GameFeatureAction_AddComponents (NOT a CMC subclass), reading the pawn's EXISTING
 * stock CMC via GetCharacterMovement() and setting the public MaxWalkSpeed float. No
 * reparent. ASC bind = direct GetAbilitySystemComponentFromActor first, ULyraPawnExtension
 * OnAbilitySystemInitialized_RegisterAndCall fallback for the possessed player. Tick-free
 * (tag-event-driven). Server-authoritative gameplay: the ConsequenceGE applies server-side.
 *
 * TWO tags (L + R leg) drive ONE penalty: a NewCount-aware refcount across both tags, so
 * losing both legs does not double-apply and restoring one leg (if that were possible) keeps
 * the penalty while the other holds. In the head-zone-first slice the leg GE is unwired
 * until PHASE B; this component is the vehicle the leg ConsequenceGE drives.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLDISMEMBER_API UAFLDismemberLegPenaltyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLDismemberLegPenaltyComponent();

	/** Fraction of MaxWalkSpeed kept while a leg is severed (0.5 = half speed). Designer-tunable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember|Leg")
	float SeveredLegSpeedScale = 0.5f;

	UFUNCTION(BlueprintPure, Category = "AFL|Dismember|Leg")
	bool IsLegPenaltyActive() const { return bLegPenaltyActive; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Deferred-player ASC-ready callback (fires via the PawnExtension hook). */
	void OnAbilitySystemReady();

	/** Bind/unbind the State.Dismembered.Left/RightLeg tag events on the owner's ASC. */
	void BindToAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindFromAbilitySystem();

	/** Tag-change callback for either leg tag: recomputes the severed-leg count -> apply/restore. */
	void HandleLegTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Resolve the owner's CharacterMovementComponent (the pawn's EXISTING stock CMC). */
	UCharacterMovementComponent* GetOwnerCMC() const;

	void ApplyLegPenalty();
	void RestoreLegPenalty();

	/** Cached at first-leg-severed, restored when the last leg tag clears. -1.0 = uninitialized. */
	float CachedMaxWalkSpeed = -1.0f;
	bool bLegPenaltyActive = false;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle LeftLegTagHandle;
	FDelegateHandle RightLegTagHandle;
};
