// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AFLGameplayAbility_WallJump.generated.h"

/**
 * UAFLGameplayAbility_WallJump  (Movement Overhaul — Phase 2: wall-jump off a wall-run)
 *
 * A thin input ability that only activates DURING a wall-run (ActivationRequiredTags =
 * State.Movement.WallRunning). Bound to the JUMP input alongside the normal jump ability: while wall-running
 * the pawn is in MOVE_Flying (stock jump no-ops), so pressing jump activates THIS instead — it reads the
 * current wall normal from UAFLWallRunMovementComponent and LaunchCharacter's the pawn off the wall (away +
 * up). The launch pulls the pawn off the surface, so the wall-run's per-tick side-trace loses the wall next
 * frame and that ability exits on its own. When not wall-running the required tag is absent, so this never
 * fires and normal jump is unaffected. Abstract; GA_AFL_WallJump BP child (usually just tunes the forces).
 */
UCLASS(Abstract)
class AFLMOVEMENT_API UAFLGameplayAbility_WallJump : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UAFLGameplayAbility_WallJump(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Launch speed (cm/s) away from the wall (along its normal). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallJump")
	float AwayForce = 550.0f;

	/** Launch speed (cm/s) upward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|WallJump")
	float UpForce = 550.0f;
};
