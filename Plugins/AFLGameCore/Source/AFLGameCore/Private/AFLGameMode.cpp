// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLGameMode.h"

#include "AFLGameCore.h"                      // LogAFLGameCore
#include "AFLRoundRestartPolicy.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"           // ParseOption -- the ?PlayFabId= connect-option read
#include "Teams/AFLReconcileIdComponent.h"    // the T2 identity-join stash

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

FString AAFLGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	// Stock init first (player name, spectator flag, etc.).
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	// T2 identity-join: stash the ?PlayFabId= reconcile key onto the PlayerState so UAFLMatchmakerDataProvider can
	// match the matchmaker roster (member.id) to this controller. A pure NO-OP for LocalFill / PIE joins (no
	// ?PlayFabId= present) -> safe on every live join. Not read until the matchmaker provider is active (S12).
	const FString PlayFabId = UGameplayStatics::ParseOption(Options, TEXT("PlayFabId"));
	if (!PlayFabId.IsEmpty() && NewPlayerController)
	{
		if (APlayerState* PS = NewPlayerController->PlayerState)
		{
			UAFLReconcileIdComponent* IdComp = PS->FindComponentByClass<UAFLReconcileIdComponent>();
			if (!IdComp)
			{
				IdComp = NewObject<UAFLReconcileIdComponent>(PS);
				IdComp->RegisterComponent();
			}
			IdComp->SetReconcileId(PlayFabId);
			UE_LOG(LogAFLGameCore, Log, TEXT("AFLTeams: identity-join -- stashed reconcile id '%s' on %s."),
				*PlayFabId, *GetNameSafe(PS));
		}
	}

	return Result;
}
