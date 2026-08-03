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

	const float Threshold = FMath::Max(MinThresholdCm, Bot->GetMoveProfile().PreferredRangeCm * ThresholdScale);
	const float ToGoal = FVector::Dist2D(Pawn->GetActorLocation(), BB->GetValueAsVector(MoveGoalKeyName));
	if (ToGoal < Threshold)
	{
		return;   // close enough to be fighting rather than travelling -- walk
	}

	// Re-sent every tick on purpose. The first one activates the ability; the rest renew its lease. Silence is
	// the stop signal, so this is the only line that keeps a bot sprinting.
	FGameplayEventData Payload;
	Payload.Instigator = Pawn;
	Payload.EventMagnitude = ToGoal;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Pawn, TAG_Event_Movement_Sprint_Requested, Payload);
}

FString UAFLBTService_SprintDecision::GetStaticDescription() const
{
	return FString::Printf(TEXT("Sprint when %s is further than max(%.0fcm, PreferredRange x %.2f)"),
		*MoveGoalKeyName.ToString(), MinThresholdCm, ThresholdScale);
}
