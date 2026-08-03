// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLMovementPathScope.h"

#include "AFLMovement.h"
#include "AIController.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"

namespace
{
	/** Resolved through AAIController on purpose -- a player-controlled pawn has no path following component,
	 *  so this returns null for humans and both entry points become no-ops without needing a bot check. */
	UPathFollowingComponent* ResolvePFC(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
		AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
		return AI ? AI->GetPathFollowingComponent() : nullptr;
	}
}

void AFLMovementPath::Suspend(const FGameplayAbilityActorInfo* ActorInfo)
{
	UPathFollowingComponent* PFC = ResolvePFC(ActorInfo);
	if (!PFC || PFC->GetStatus() != EPathFollowingStatus::Moving)
	{
		return;   // human, or not currently following anything
	}

	// Keep, not Reset. Reset calls StopMovementKeepPathing, which would zero the velocity a dash just applied
	// -- in the same frame, before it moved anyone.
	PFC->PauseMove(PFC->GetCurrentRequestId(), EPathFollowingVelocityMode::Keep);
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_PATHSCOPE: suspended path following for %s."),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));
}

void AFLMovementPath::Resume(const FGameplayAbilityActorInfo* ActorInfo)
{
	UPathFollowingComponent* PFC = ResolvePFC(ActorInfo);
	if (!PFC || PFC->GetStatus() != EPathFollowingStatus::Paused)
	{
		return;   // never suspended, already resumed, or a new request already cleared it
	}

	PFC->ResumeMove(PFC->GetCurrentRequestId());
	UE_LOG(LogAFLMovement, Log, TEXT("AFL_PATHSCOPE: resumed path following for %s."),
		*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));
}
