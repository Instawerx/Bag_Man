// Copyright C12 AI Gaming. All Rights Reserved.

#include "Bots/AFLBTService_SprintDecision.h"

#include "AFLGameCore.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Bots/AFLBotController.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBTService_SprintDecision)

// Must match AFLGameplayAbility_Sprint.cpp. Declared natively on both sides rather than shared through a
// header so AFLGameCore keeps zero dependency on the AFLMovement GameFeature -- the same rule that keeps
// IAFLMatchTierSource an interface. A tag string is the whole contract.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Movement_Sprint_Requested, "Event.Movement.Sprint.Requested");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Movement_Dash_Requested,   "Event.Movement.Dash.Requested");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Movement_Slide_Requested,  "Event.Movement.Slide.Requested");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Movement_Roll_Requested,   "Event.Movement.Roll.Requested");

UAFLBTService_SprintDecision::UAFLBTService_SprintDecision(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("AFL Sprint Decision");

	// 0.5s, no deviation. The ability's lease is 1.25s, so this tolerates two consecutive misses before sprint
	// drops. Randomising the interval here would randomise the lease margin too -- not worth it for a service
	// whose only job is to keep saying "still want it".
	Interval = 0.5f;
	RandomDeviation = 0.0f;

	bNotifyTick = true;
	bTickIntervals = true;
}

void UAFLBTService_SprintDecision::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	const AAFLBotController* Bot = Cast<AAFLBotController>(OwnerComp.GetAIOwner());
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!Bot || !Pawn || !BB)
	{
		return;
	}

	// No goal means nothing to travel toward. Staying silent here is what makes a starved query stop the
	// sprint rather than leave it running into nothing.
	if (!BB->IsVectorValueSet(MoveGoalKeyName))
	{
		return;
	}

	const FAFLBotMoveProfile& M = Bot->GetMoveProfile();
	const float Threshold = FMath::Max(MinThresholdCm, M.PreferredRangeCm * ThresholdScale);
	const float ToGoal = FVector::Dist2D(Pawn->GetActorLocation(), BB->GetValueAsVector(MoveGoalKeyName));

	auto Send = [Pawn](const FGameplayTag& Tag, float Magnitude)
	{
		FGameplayEventData Payload;
		Payload.Instigator = Pawn;
		Payload.EventMagnitude = Magnitude;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Pawn, Tag, Payload);
	};

	// SPRINT -- continuous. Re-sent every tick on purpose: the first activates, the rest renew the lease, and
	// silence is the stop signal. This is the only line that keeps a bot sprinting.
	const bool bWantSprint = (ToGoal >= Threshold);
	if (bWantSprint)
	{
		Send(TAG_Event_Movement_Sprint_Requested, ToGoal);
	}

	if (!bEnableOneShots)
	{
		return;
	}

	// ONE-SHOTS -- rate limited by the bot's OWN RepositionIntervalSec. Without this a 0.5s service would ask
	// for a dash twice a second and the roster would look twitchy and identical; with it, cadence varies per
	// bot off an axis that was already rolled and has been inert since AI-2.
	const UWorld* World = Pawn->GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now < NextOneShotTime)
	{
		return;
	}

	// Ladder, most-committed first. Only ONE fires per window -- stacking a dash into a roll would hand the
	// body to two abilities at once, which is the conflict this whole pattern exists to avoid.
	// By value, not by pointer: UE_DEFINE_GAMEPLAY_TAG_STATIC declares an FNativeGameplayTag, which converts
	// to FGameplayTag but is not one -- &Tag is an FNativeGameplayTag*, unrelated to FGameplayTag*.
	FGameplayTag Chosen;
	if (bWantSprint && ToGoal < SlideArrivalCm)
	{
		Chosen = TAG_Event_Movement_Slide_Requested;   // arriving out of a sprint
	}
	else if (ToGoal > M.PreferredRangeCm * DashBand && ToGoal < Threshold)
	{
		Chosen = TAG_Event_Movement_Dash_Requested;    // mid-range burst; its cooldown GE also gates it
	}
	else if (ToGoal <= M.PreferredRangeCm * RollBand)
	{
		Chosen = TAG_Event_Movement_Roll_Requested;    // in the fight, a short dodge
	}

	if (Chosen.IsValid())
	{
		Send(Chosen, ToGoal);
		NextOneShotTime = Now + FMath::Max(0.5f, M.RepositionIntervalSec);
		UE_LOG(LogAFLGameCore, Log, TEXT("AFL_BOTKIT: %s -> %s at %.0fcm (next in %.2fs)"),
			*GetNameSafe(Pawn), *Chosen.ToString(), ToGoal, M.RepositionIntervalSec);
	}
}

FString UAFLBTService_SprintDecision::GetStaticDescription() const
{
	return FString::Printf(TEXT("Sprint when %s is further than max(%.0fcm, PreferredRange x %.2f)"),
		*MoveGoalKeyName.ToString(), MinThresholdCm, ThresholdScale);
}
