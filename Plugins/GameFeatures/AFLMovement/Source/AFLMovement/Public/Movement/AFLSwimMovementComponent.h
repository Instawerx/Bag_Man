// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"

#include "AFLSwimMovementComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;

/**
 * UAFLSwimMovementComponent  (water phase 2 -- swim tuning + the movement-mode instrument)
 *
 * Applies the swim feel values to the pawn's EXISTING stock CharacterMovementComponent, and logs every
 * movement-mode transition. Delivered as a GameFeature-attached UActorComponent, conforming to
 * UAFLSprintMovementComponent -- the proven P-CONTROLS pattern.
 *
 * ⚠ WHY THIS EXISTS AT ALL -- THE FAILURE IT REPLACES.
 * The same values were previously authored in UAFLCharacterMovementComponent's constructor. The code was
 * correct and byte-verified, and COMPLETELY INERT: that subclass is installed only by AAFLCharacter's
 * SetDefaultSubobjectClass, and NO PAWN DERIVES FROM AAFLCharacter. B_Hero_BagMan_Pro descends from
 * B_Hero_ShooterMannequin -> B_Hero_Default -> ALyraCharacter and runs the STOCK CMC, so the constructor
 * never ran on anything that plays.
 *
 * The pre-check at the time confirmed no BLUEPRINT overrode the property. Nobody checked whether any pawn
 * used the CLASS. That is the same shape as the tag-contract and warmup episodes: THE VALUE WAS VERIFIED,
 * THE CONSUMER WAS NOT. The lesson worth carrying: for a default to matter, something must construct the
 * type that carries it -- "the property is set correctly" and "the property is reachable" are different
 * claims and only the second one moves the game.
 *
 * ⚠ WHY NOT JUST INSTALL THE SUBCLASS. A CMC subclass installs ONLY via a C++ ctor SetDefaultSubobjectClass,
 * which forces reparenting the hero BP off B_Hero_ShooterMannequin -- and that silences input and breaks
 * spawn. That is banked in UAFLSprintMovementComponent.h:17-24 as a verified project failure, and it is
 * P-CONTROLS doctrine. So this component READS the stock CMC via GetCharacterMovement() and writes its
 * public floats, exactly as sprint does. No reparent, foundation intact.
 *
 * DIVERGENCE FROM SPRINT, and it makes this component simpler:
 *   Sprint is TAG-DRIVEN -- it must reach the owner's ASC, so it carries the deferred PawnExtension bind
 *   for the possessed player whose PlayerState ASC lands after pawn BeginPlay.
 *   Swim tuning is STATIC. It needs no ASC, no tag, and no cache/restore pair, because nothing toggles it:
 *   the values are applied once and left. So the ASC machinery is deliberately absent rather than copied.
 *
 * THE INSTRUMENT rides here because it has to. The OnMovementModeChanged override added to the CMC subclass
 * is inert for the same reason the constructor was -- it lives on a class nothing constructs. This component
 * binds ACharacter::MovementModeChangedDelegate instead, which is a real hook on the real pawn.
 */
UCLASS(ClassGroup = (AFL), Blueprintable, meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLSwimMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UAFLSwimMovementComponent();

	/**
	 * Max swim speed, cm/s. 50% of the Pro pawn's 700 land reference -- unmistakably slower than running,
	 * while still crossing the ~370 m swimmable band in reasonable time, so water reads as traversable
	 * rather than as a soft wall (R32).
	 *
	 * The engine default is 300, which at 43% is coincidentally close. THAT IS EXACTLY WHY IT NEEDS
	 * AUTHORING: right-ish by accident is not a decision, and nothing downstream can distinguish a value
	 * someone chose from one nobody touched.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Swim", meta = (ClampMin = "0.0"))
	float MaxSwimSpeed = 350.0f;

	/**
	 * Buoyancy. 1.0 is neutral -- the robot floats at the surface and stays visible, which is what R32's
	 * traversable-water ruling needs. Below 1.0 it sinks and water becomes a trap, the behaviour R32 retired.
	 *
	 * A RECORDED DECISION even though it equals the engine default: the next reader should find the value
	 * chosen and reasoned, not absent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Movement|Swim", meta = (ClampMin = "0.0"))
	float Buoyancy = 1.0f;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Resolve the owner's EXISTING stock CharacterMovementComponent. Never creates or replaces one. */
	UCharacterMovementComponent* GetOwnerCMC() const;

	/** Write MaxSwimSpeed / Buoyancy onto that CMC. Idempotent. */
	void ApplySwimTuning();

	/**
	 * Bound to ACharacter::MovementModeChangedDelegate -- a real multicast hook on the real pawn, so no
	 * tick and no polling. UFUNCTION is required: the delegate is DYNAMIC.
	 */
	UFUNCTION()
	void HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	/** True once the tuning has been written, so a re-entry cannot double-apply or log a false transition. */
	bool bTuningApplied = false;
};
