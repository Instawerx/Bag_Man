// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"          // EMovementMode
#include "GameplayTagContainer.h"

#include "AFLClimbMovementComponent.generated.h"

class UAbilitySystemComponent;
class UCharacterMovementComponent;

/** Broadcast when the forward surface-trace fails while climbing -> the climb ability cancels. */
DECLARE_MULTICAST_DELEGATE(FAFLOnClimbSurfaceLost);

/**
 * UAFLClimbMovementComponent  (P-CONTROLS climb, Option-γ two-layer split)
 *
 * The CMC-STATE half of climb. The ABILITY (UAFLGameplayAbility_Climb) owns the MOTION
 * (root-motion montage via AbilityTask_PlayMontageAndWait); THIS component owns the
 * MOVEMENT-COMPONENT STATE, exactly mirroring UAFLDashMovementComponent: a standalone
 * UActorComponent added to B_Hero_BagMan via the experience's GameFeatureAction_AddComponents
 * (NOT a CMC subclass, NOT a reparent -- the Pillar-2 doctrine proven by AFL-0304-B / 20021663;
 * the hero stays a child of the proven-walker B_Hero_ShooterMannequin).
 *
 * On State.Movement.Climbing (granted by GE_AFL_Climb_Active): cache the stock CMC's
 * GravityScale + MovementMode, then set GravityScale=0 and MovementMode=MOVE_Flying so the
 * pawn holds on the wall plane while the ability's root motion translates it up. On tag-remove:
 * restore the cached values (the dash restore-symmetry). While the tag is active the component
 * TICKS and forward-traces (origin + forward*TraceDistance) for surface-loss; on a miss it
 * broadcasts OnClimbSurfaceLost so the ability force-exits.
 *
 * Reads the pawn's EXISTING stock UCharacterMovementComponent via GetCharacterMovement() --
 * GravityScale/MovementMode are public; no PhysCustom, no MOVE_Custom (those would need a CMC
 * subclass). ASC bind mirrors UAFLDashMovementComponent / UAFLDeathComponent (direct resolve
 * first, ULyraPawnExtensionComponent::OnAbilitySystemInitialized_RegisterAndCall fallback).
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLClimbMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLClimbMovementComponent();

	/** Forward trace length (cm) for the climbable-surface / surface-loss check. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Climb")
	float SurfaceTraceDistance = 80.0f;

	/** True while climb state is applied (gravity off / flying). */
	UFUNCTION(BlueprintPure, Category = "AFL|Movement|Climb")
	bool IsClimbStateActive() const { return bClimbStateActive; }

	/** The ability binds this to cancel itself when the wall is lost mid-climb. */
	FAFLOnClimbSurfaceLost OnClimbSurfaceLost;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void OnAbilitySystemReady();
	void BindToAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindFromAbilitySystem();
	void HandleClimbTagChanged(const FGameplayTag Tag, int32 NewCount);

	UCharacterMovementComponent* GetOwnerCMC() const;

	void ApplyClimbState();
	void RestoreClimbState();

	/** Cached at climb entry, restored at climb exit. */
	float CachedGravityScale = 1.0f;
	TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_Walking;
	bool bClimbStateActive = false;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	FDelegateHandle ClimbTagChangedHandle;
};
