// Copyright C12 AI Gaming. All Rights Reserved.

#include "Bots/AFLBotFillComponent.h"

#include "AFLGameCore.h"                    // LogAFLGameCore
#include "AIController.h"                   // complete AAIController (BotControllerClass is TSubclassOf<AAIController>)
#include "Teams/LyraTeamSubsystem.h"        // live team count
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"         // GetIntOption (URL "NumBots" parity)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBotFillComponent)

void UAFLBotFillComponent::ServerCreateBots_Implementation()
{
#if WITH_SERVER_CODE
	if (BotControllerClass == nullptr)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFLBots: no BotControllerClass set; skipping bot fill."));
		return;
	}

	// Reset the name pool exactly as stock does before spawning.
	RemainingBotNames = RandomBotNames;

	const UWorld* World = GetWorld();
	AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();

	// NumTeams from the live registry (team creation ran HighPriority, before this LowPriority bot pass).
	int32 NumTeams = 0;
	if (const ULyraTeamSubsystem* Teams = World ? World->GetSubsystem<ULyraTeamSubsystem>() : nullptr)
	{
		NumTeams = Teams->GetTeamIDs().Num();
	}

	// Real humans present at fill time (bots + spectators excluded).
	int32 HumanCount = 0;
	for (const APlayerState* PS : GameState->PlayerArray)
	{
		if (PS && !PS->IsABot() && !PS->IsOnlyASpectator())
		{
			++HumanCount;
		}
	}

	const int32 Target = FMath::Max(0, TeamSize) * NumTeams;   // e.g. 3 * 2 = 6 for 3v3
	int32 EffectiveBotCount = FMath::Max(0, Target - HumanCount);

	// Keep parity with stock's URL override so QA can still force an exact count.
	if (AGameModeBase* GameMode = GetGameMode<AGameModeBase>())
	{
		EffectiveBotCount = UGameplayStatics::GetIntOption(GameMode->OptionsString, TEXT("NumBots"), EffectiveBotCount);
	}

	UE_LOG(LogAFLGameCore, Log,
		TEXT("AFLBots: human-aware fill -- TeamSize=%d NumTeams=%d Humans=%d -> %d bot(s) (target %d)"),
		TeamSize, NumTeams, HumanCount, EffectiveBotCount, Target);

	// Reuse the stock spawn/possess/team-routing path unchanged -- each bot still routes through
	// OnGameModePlayerInitialized -> ServerChooseTeamForPlayer -> the provider's balance.
	for (int32 Count = 0; Count < EffectiveBotCount; ++Count)
	{
		SpawnOneBot();
	}
#endif // WITH_SERVER_CODE
}
