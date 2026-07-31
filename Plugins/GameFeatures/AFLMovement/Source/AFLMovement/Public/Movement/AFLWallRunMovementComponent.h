// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "AFLWallRunMovementComponent.generated.h"

class UAbilitySystemComponent;
class UCharacterMovementComponent;

/**
 * UAFLWallRunMovementComponent  (Movement Overhaul — Phase 2: wall-run detection + CMC state)
 *
 * Mirrors UAFLClimbMovementComponent's two-layer split, extended for the CONTEXTUAL (no-key) wall-run:
 *  - DETECTION (while airborne + not wall-running): throttled left/right SIDE-traces; on a wall within reach
 *    with enough forward speed, sends Event.Movement.WallRun.Detected to the owner ASC (which triggers
 *    UAFLGameplayAbility_WallRun). The ability, not a keypress, is the activation — no input binding.
 *  - STATE (while State.Movement.WallRunning holds): caches GravityScale, sets gravity 0 + MOVE_Flying, and
 *    each tick side-traces the wall, drives CMC velocity along the wall tangent (+ a stick force toward the
 *    wall), and broadcasts OnWallSurfaceLost when the wall is gone -> the ability exits. Exposes the current
 *    wall normal for the wall-jump ability.
 *
 * NOT a CMC subclass (the reparent trap; see UAFLDashMovementComponent). Added via GameFeatureAction_AddComponents.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLWallRunMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLWallRunMovementComponent();

	// --- Detection ---
	/** Side reach (cm) for the wall-detect trace while airborne. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun") float DetectionSideDistance = 60.0f;
	/** Minimum horizontal speed (cm/s) required to start a wall-run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun") float MinForwardSpeed = 300.0f;
	/** Detection trace cadence (s) while airborne (throttle; mobile can widen it). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun") float DetectionInterval = 0.06f;
	/** After a wall-run ends, suppress re-detection for this long (s) so a wall-jump doesn't re-stick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun") float ReattachCooldown = 0.4f;

	// --- Wall-run physics ---
	/** Speed (cm/s) along the wall tangent while wall-running. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun") float WallRunSpeed = 750.0f;
	/** Velocity toward the wall (cm/s) to keep the pawn attached. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun") float WallStickForce = 200.0f;
	/** Downward drift (cm/s) applied while wall-running (0 = pure horizontal; small = a gravity-slide feel). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun") float WallRunDownDrift = 0.0f;
	/** Side reach (cm) for the wall-loss trace while wall-running (a bit longer than detection, tolerates gaps). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallRun") float WallLossTraceDistance = 90.0f;

	/** Broadcast when the wall is lost mid-run -> the wall-run ability cancels. */
	DECLARE_MULTICAST_DELEGATE(FOnWallSurfaceLost);
	FOnWallSurfaceLost OnWallSurfaceLost;

	/** Current wall normal (world), valid while wall-running (used by the wall-jump ability). */
	UFUNCTION(BlueprintPure, Category = "AFL|WallRun") FVector GetCurrentWallNormal() const { return CurrentWallNormal; }
	UFUNCTION(BlueprintPure, Category = "AFL|WallRun") bool IsWallRunning() const { return bWallRunActive; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void OnAbilitySystemReady();
	void BindToAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindFromAbilitySystem();
	void HandleWallRunTagChanged(const FGameplayTag Tag, int32 NewCount);

	UCharacterMovementComponent* GetOwnerCMC() const;
	void EnterWallRunState();
	void ExitWallRunState();
	void TickDetection(float DeltaTime);
	void TickWallRunPhysics(float DeltaTime);
	/** Side-trace from the capsule along Dir (unit) up to Distance. */
	bool SideTrace(const FVector& Dir, float Distance, FHitResult& OutHit) const;

	UPROPERTY() TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	FDelegateHandle WallRunTagHandle;

	bool bWallRunActive = false;
	float CachedGravityScale = -1.0f;
	FVector CurrentWallNormal = FVector::ZeroVector;
	float DetectAccum = 0.0f;
	float ReattachTimer = 0.0f;
};
