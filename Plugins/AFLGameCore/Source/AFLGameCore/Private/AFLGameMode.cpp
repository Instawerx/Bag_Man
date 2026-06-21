// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLGameMode.h"

#include "AFLRoundRestartPolicy.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLGameMode)

bool AAFLGameMode::ControllerCanRestart(AController* Controller)
{
	// Round-based gate: deny mid-round respawn while a round is active. Decoupled from the GameFeature
	// round driver via the IAFLRoundRestartPolicy seam -- NO concrete GameFeature type is referenced, so
	// this always-loaded GameMode carries ZERO dependency into AFLCombat. No-op (falls through to Super)
	// when no policy provider is present, so it is safe to set as a global default game mode.
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GS = World->GetGameState<AGameStateBase>())
		{
			TArray<UActorComponent*> Comps;
			GS->GetComponents(Comps);
			for (const UActorComponent* Comp : Comps)
			{
				if (const IAFLRoundRestartPolicy* Policy = Cast<IAFLRoundRestartPolicy>(Comp))
				{
					if (Policy->ShouldBlockRestart())
					{
						return false;
					}
					break;   // the first policy provider decides
				}
			}
		}
	}
	return Super::ControllerCanRestart(Controller);
}
