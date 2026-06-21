// Copyright C12 AI Gaming. All Rights Reserved.

#include "GameModes/AFLGameMode.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Round/AFLRoundManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameMode)

bool AAFLGameMode::ControllerCanRestart(AController* Controller)
{
	// Round-based gate: deny mid-round respawn while a round is active. No-op (falls through to Super)
	// when no round manager is present, so this is safe to set as a global default game mode.
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GS = World->GetGameState<AGameStateBase>())
		{
			if (const UAFLRoundManagerComponent* RoundManager = GS->FindComponentByClass<UAFLRoundManagerComponent>())
			{
				if (RoundManager->ShouldBlockRestart())
				{
					return false;
				}
			}
		}
	}
	return Super::ControllerCanRestart(Controller);
}
