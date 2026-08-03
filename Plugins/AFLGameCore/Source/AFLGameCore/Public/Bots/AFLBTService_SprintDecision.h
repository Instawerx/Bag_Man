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

	// ================= AI-3b: the rest of the kit =================
	//
	// ONE LADDER, keyed on distance-to-goal expressed in units of the bot's OWN preferred range. Sprint is
	// continuous (a lease, renewed every tick it applies); dash/slide/roll are one-shots gated by a per-bot
	// cadence so a 0.5s service does not spam them.
	//
	//   d > Pref * ThresholdScale            SPRINT   crossing the arena
	//   sprinting and d < SlideArrivalCm     SLIDE    arriving out of a sprint, the way a human uses it
	//   DashBand * Pref < d <= sprint edge   DASH     mid-range burst; its own cooldown GE also limits it
	//   d <= RollBand * Pref                 ROLL     in the fight, a short dodge
	//
	// PER-BOT VARIANCE COMES FROM TWO AXES ALREADY ROLLED, no new randomness: every threshold scales off
	// PreferredRangeCm, and the discretionary cadence is RepositionIntervalSec -- the axis that has been
	// dead since AI-2 because UBTService::Interval is a plain float and could not be data-bound. Used here
	// it finally does something, and two bots stop dashing on the same beat.

	/** Slide when already sprinting and this close to the goal. */
	UPROPERTY(EditAnywhere, Category = "AFL|Kit", meta = (ClampMin = "0.0"))
	float SlideArrivalCm = 400.0f;

	/** Dash when distance-to-goal exceeds this multiple of preferred range (and is under the sprint edge). */
	UPROPERTY(EditAnywhere, Category = "AFL|Kit", meta = (ClampMin = "0.0"))
	float DashBand = 0.60f;

	/** Roll when distance-to-goal is under this multiple of preferred range -- in the fight, not travelling. */
	UPROPERTY(EditAnywhere, Category = "AFL|Kit", meta = (ClampMin = "0.0"))
	float RollBand = 0.35f;

	/** Master switch for the one-shots. Sprint is unaffected -- it shipped and is proven. */
	UPROPERTY(EditAnywhere, Category = "AFL|Kit")
	bool bEnableOneShots = true;

private:
	/** Next time (world seconds) this bot may fire a discretionary one-shot. Spacing is the bot's own
	 *  RepositionIntervalSec, so cadence varies per bot without a new roll. */
	double NextOneShotTime = 0.0;
};
