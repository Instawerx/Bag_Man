// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTService.h"

#include "AFLBTService_SprintDecision.generated.h"

/**
 * UAFLBTService_SprintDecision  (AI-3 -- the decision half of bot sprint)
 *
 * THE SEAM. Sits on the Shoot And Move sequence beside Shoot, SetFocus and Find Best Position, so it inherits
 * their lifetime exactly: it only runs while the bot has ammo, has an enemy, and is allowed to fire. Leaving
 * that branch stops the ticking, which is also how the sprint ends -- see below.
 *
 * WHY SPRINT AND NOT THE REST OF THE KIT. Sprint is the only movement ability that composes with the MoveTo
 * now driving bots: it multiplies CharacterMovement MaxWalkSpeed, a value path following already reads, so the
 * bot takes the same path faster and nothing notices. Dash overrides XY velocity outright (LaunchCharacter),
 * and Slide/Roll/Vault are root-motion montages -- all four take the body away from MoveTo and need a
 * path-conflict design before they are safe.
 *
 * THE RULE. Sprint when the bot is further from its MoveGoal than its own preferred engagement range; walk
 * when closer. The reasoning is that PreferredRangeCm already means "the distance at which this bot fights",
 * so anything beyond it is travel rather than fighting -- and because that value is already rolled per bot
 * (~426-916cm observed across tiers), two bots differ without inventing a new axis to roll.
 *
 * IT SENDS, IT DOES NOT COMMAND. This service never touches movement. It broadcasts
 * Event.Movement.Sprint.Requested every tick it wants sprint; the ability holds a lease that expires if the
 * events stop. That is deliberate: a stop-event design latches sprint on forever the moment the bot leaves
 * this branch and nobody is left to send the stop.
 */
UCLASS(meta = (DisplayName = "AFL Sprint Decision"))
class AFLGAMECORE_API UAFLBTService_SprintDecision : public UBTService
{
	GENERATED_BODY()

public:
	UAFLBTService_SprintDecision(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** Blackboard key holding the reposition goal (written by the RunEQS service). */
	UPROPERTY(EditAnywhere, Category = "AFL|Sprint")
	FName MoveGoalKeyName = TEXT("MoveGoal");

	/** Multiplier on the bot's own PreferredRangeCm to get its sprint threshold. 1.0 = sprint whenever the
	 *  goal is further away than this bot likes to fight from. */
	UPROPERTY(EditAnywhere, Category = "AFL|Sprint", meta = (ClampMin = "0.1"))
	float ThresholdScale = 1.0f;

	/** Floor on the resolved threshold, cm. Stops a low roll turning into "always sprinting", which reads as
	 *  one gear and is a probe failure in its own right. */
	UPROPERTY(EditAnywhere, Category = "AFL|Sprint", meta = (ClampMin = "0.0"))
	float MinThresholdCm = 300.0f;
};
