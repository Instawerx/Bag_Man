// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "AFLSlideMovementComponent.generated.h"

class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * UAFLSlideMovementComponent  (Movement Overhaul — Phase 1: slide feel-swap + capsule duck)
 *
 * The slide CMC-state half: while State.Movement.Sliding is held (granted by GE_AFL_Slide_Active, applied by
 * GA_AFL_Slide), make the ground slippery and duck the capsule so the character slides low. The MOTION
 * (root-motion slide montage + Motion Warping to a variable stop point) is owned by GA_AFL_Slide — this
 * component only mutates the stock CMC feel, the PILLAR-2-CORRECT way (standalone UActorComponent added via
 * GameFeatureAction_AddComponents, NOT a CMC subclass; see UAFLDashMovementComponent's header for why).
 *
 * On tag-add: cache the CMC's CURRENT GroundFriction / BrakingDecelerationWalking, set the low slide values,
 * and Crouch() the character (CMC lowers the capsule + offsets the mesh, replicated). On tag-remove: restore
 * the cache and UnCrouch(). Re-entrancy guard (bSlideActive) per the Overdrive/Dash precedent. Tick-free.
 *
 * Slide-and-shoot note: this component does NOT touch firing/aiming — the upper body keeps aiming because the
 * slide MONTAGE plays on a lower-body slot (authored content) and GA_AFL_Slide does not block the fire ability.
 *
 * ASC bind mirrors UAFLDashMovementComponent (direct first, PawnExtension fallback for the possessed player).
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLSlideMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLSlideMovementComponent();

	/** GroundFriction while sliding (low = slippery so the slide carries). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Slide", meta = (ClampMin = "0.0"))
	float SlideGroundFriction = 0.4f;

	/** BrakingDecelerationWalking while sliding (low = the slide doesn't scrub speed instantly). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Slide", meta = (ClampMin = "0.0"))
	float SlideBrakingDeceleration = 512.0f;

	/** Duck the capsule (via CMC Crouch) during the slide so the character passes under low geometry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Slide")
	bool bDuckCapsuleDuringSlide = true;

	UFUNCTION(BlueprintPure, Category = "AFL|Movement|Slide")
	bool IsSliding() const { return bSlideActive; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnAbilitySystemReady();
	void BindToAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindFromAbilitySystem();
	void HandleSlideTagChanged(const FGameplayTag Tag, int32 NewCount);
	UCharacterMovementComponent* GetOwnerCMC() const;
	void ApplySlideTuning();
	void RestoreSlideTuning();

	/** Cached at slide entry, restored at slide exit. -1.0 = uninitialized. */
	float CachedGroundFriction = -1.0f;
	float CachedBrakingDeceleration = -1.0f;
	bool bSlideActive = false;
	bool bDidCrouch = false;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle SlideTagChangedHandle;
};
