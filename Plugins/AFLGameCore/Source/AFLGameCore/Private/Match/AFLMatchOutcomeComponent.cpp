// Copyright C12 AI Gaming. All Rights Reserved.

#include "Match/AFLMatchOutcomeComponent.h"

#include "AFLGameCore.h"                    // LogAFLGameCore
#include "Engine/Engine.h"                  // GEngine->GetWorldFromContextObject
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchOutcomeComponent)

UAFLMatchOutcomeComponent::UAFLMatchOutcomeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAFLMatchOutcomeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLMatchOutcomeComponent, Outcomes);
}

UAFLMatchOutcomeComponent* UAFLMatchOutcomeComponent::Find(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UAFLMatchOutcomeComponent>() : nullptr;
}

UAFLMatchOutcomeComponent* UAFLMatchOutcomeComponent::EnsureOn(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return nullptr;
	}

	// SERVER ONLY. A client that created its own copy would own an unreplicated component that shadows the
	// real one by class, and FindComponentByClass would then return whichever was registered first -- a
	// board silently reading local zeroes instead of the server's answer.
	if (!GameState->HasAuthority())
	{
		return GameState->FindComponentByClass<UAFLMatchOutcomeComponent>();
	}

	if (UAFLMatchOutcomeComponent* Existing = GameState->FindComponentByClass<UAFLMatchOutcomeComponent>())
	{
		return Existing;
	}

	UAFLMatchOutcomeComponent* Created = NewObject<UAFLMatchOutcomeComponent>(GameState);
	Created->RegisterComponent();
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_OUTCOME: created match-outcome component on %s."), *GetNameSafe(GameState));
	return Created;
}

FAFLPlayerOutcome& UAFLMatchOutcomeComponent::FindOrAddMutable(APlayerState* Player)
{
	for (FAFLPlayerOutcome& Entry : Outcomes)
	{
		if (Entry.Player == Player)
		{
			return Entry;
		}
	}
	FAFLPlayerOutcome& Added = Outcomes.AddDefaulted_GetRef();
	Added.Player = Player;
	return Added;
}

void UAFLMatchOutcomeComponent::ServerRecordStake(APlayerState* Player, int32 InStake)
{
	if (!Player || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	FindOrAddMutable(Player).Stake = InStake;
	OnOutcomesChanged.Broadcast();   // the server's own listen-host UI sees no OnRep -- fire locally too
}

void UAFLMatchOutcomeComponent::ServerRecordPayout(APlayerState* Player, int32 InPayout)
{
	if (!Player || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	FAFLPlayerOutcome& Entry = FindOrAddMutable(Player);
	Entry.Payout = InPayout;
	Entry.bHasSettle = true;
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_OUTCOME: %s paid %d."), *Player->GetPlayerName(), InPayout);
	OnOutcomesChanged.Broadcast();
}

void UAFLMatchOutcomeComponent::ServerRecordRatingDelta(APlayerState* Player, float InDelta)
{
	if (!Player || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	FAFLPlayerOutcome& Entry = FindOrAddMutable(Player);
	Entry.RatingDelta = InDelta;
	Entry.bHasRating = true;
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_OUTCOME: %s rating delta %+.2f."), *Player->GetPlayerName(), InDelta);
	OnOutcomesChanged.Broadcast();
}

const FAFLPlayerOutcome* UAFLMatchOutcomeComponent::FindOutcome(const APlayerState* Player) const
{
	return Outcomes.FindByPredicate([Player](const FAFLPlayerOutcome& E) { return E.Player == Player; });
}

void UAFLMatchOutcomeComponent::OnRep_Outcomes()
{
	OnOutcomesChanged.Broadcast();
}
