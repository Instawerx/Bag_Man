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
/**
 * Identity Movement M1 — custom traversal sub-modes driven through the CMC's replicated saved-move
 * prediction path (never component-tick direct writes). Value = CustomMovementMode; PhysCustom() dispatches
 * on it. Physics are filled in M2; M1 lands the prediction plumbing only.
 */
UENUM(BlueprintType)
enum class EAFLCustomMoveMode : uint8
{
	None = 0   UMETA(DisplayName = "None"),
	WallRun    UMETA(DisplayName = "Wall Run"),
	Climb      UMETA(DisplayName = "Climb"),
	Slide      UMETA(DisplayName = "Slide"),
};

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

	// ---- M1 prediction spine (unwired until the hero uses this component; see project_movement_aaa_upgrade) ----

	/** Predicted movement intent. Set on the owning client from input/abilities; serialized into the saved move
	 *  (compressed flags) and replayed on the server, so both sides compute the same movement without corrections. */
	void SetWantsToSprint(bool bWants) { bWantsToSprint = bWants; }
	void SetWantsWallRun(bool bWants)  { bWantsWallRun  = bWants; }
	void SetWantsClimb(bool bWants)    { bWantsClimb    = bWants; }
	void SetWantsSlide(bool bWants)    { bWantsSlide    = bWants; }
	bool WantsToSprint() const { return bWantsToSprint; }

	/** Sprint speed returned by GetMaxSpeed while sprinting on the ground — predicted on BOTH sides, so it
	 *  replaces the tag-driven MaxWalkSpeed swap (which lives outside reconciliation) once this is wired. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Movement|Sprint")
	float SprintSpeed = 980.f;

	virtual float GetMaxSpeed() const override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;

	/**
	 * INSTRUMENTATION ONLY -- logs every movement-mode transition, then defers entirely to Super.
	 *
	 * WHY THIS EXISTS. UE logs NOTHING on a movement-mode change, and `showdebug` draws to the screen
	 * without writing to the log. Three PIE runs failed to settle whether swim engages -- not because the
	 * observation was missed, but because THERE WAS NO INSTRUMENT. Asking an operator to read an overlay
	 * again is not a measurement loop; this makes the transition a fact in the log file.
	 *
	 * One line answers four open questions at once: whether swim engaged, whether the water volume was
	 * entered, whether the authored MaxSwimSpeed reached runtime, and whether the exit is clean.
	 *
	 * Behaviourally inert -- Super runs first and nothing else is changed.
	 */
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	/** Read the predicted-intent bits the client packed into the compressed flags (M1 spine). */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	/** Dispatch the custom traversal sub-modes. Stub in M1; filled by M2 (wall-run / climb / slide). */
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	/** Bind/unbind the tag-change delegate on the owning pawn's ASC. */
	void TryBindToAbilitySystem();
	void UnbindFromAbilitySystem();

	/**
	 * Subscribes to ULyraPawnExtensionComponent::OnAbilitySystemInitialized so the
	 * CMC's tag listener wires up after Lyra binds the PlayerState ASC to the pawn.
	 * Lyra binds the ASC well after BeginPlay (via the PawnExtension init-state
	 * lifecycle, after Controller and PlayerState replication), so the legacy
	 * BeginPlay-only bind misses every real player pawn.
	 */
	void RegisterWithLyraPawnExtension();

	/** Callback fired from ULyraPawnExtensionComponent when the ASC becomes available. */
	void OnLyraAbilitySystemInitialized();

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

	// ---- M1 predicted intent (mirrored to/from FSavedMove_AFL compressed flags) ----
	bool bWantsToSprint = false;
	bool bWantsWallRun = false;
	bool bWantsClimb = false;
	bool bWantsSlide = false;

	friend class FSavedMove_AFL;
};
