// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularAIController.h"
#include "Teams/LyraTeamAgentInterface.h"

#include "LyraPlayerBotController.generated.h"

#define UE_API LYRAGAME_API

namespace ETeamAttitude { enum Type : int; }
struct FGenericTeamId;

class APlayerState;
class UAIPerceptionComponent;
class UObject;
struct FFrame;

/**
 * ALyraPlayerBotController
 *
 *	The controller class used by player bots in this project.
 */
UCLASS(MinimalAPI, Blueprintable)
class ALyraPlayerBotController : public AModularAIController, public ILyraTeamAgentInterface
{
	GENERATED_BODY()

public:
	UE_API ALyraPlayerBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ILyraTeamAgentInterface interface
	UE_API virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	UE_API virtual FGenericTeamId GetGenericTeamId() const override;
	UE_API virtual FOnLyraTeamIndexChangedDelegate* GetOnTeamIndexChangedDelegate() override;
	UE_API ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	//~End of ILyraTeamAgentInterface interface

	// Attempts to restart this controller (e.g., to respawn it)
	UE_API void ServerRestartController();

	//Update Team Attitude for the AI
	UFUNCTION(BlueprintCallable, Category = "Lyra AI Player Controller")
	UE_API void UpdateTeamAttitude(UAIPerceptionComponent* AIPerception);

	UE_API virtual void OnUnPossess() override;


private:
	UFUNCTION()
	UE_API void OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam);

protected:
	// Called when the player state is set or cleared
	UE_API virtual void OnPlayerStateChanged();

private:
	UE_API void BroadcastOnPlayerStateChanged();

protected:	
	//~AController interface
	UE_API virtual void InitPlayerState() override;
	UE_API virtual void CleanupPlayerState() override;
	UE_API virtual void OnRep_PlayerState() override;
	//~End of AController interface

private:
	UPROPERTY()
	FOnLyraTeamIndexChangedDelegate OnTeamChangedDelegate;

	UPROPERTY()
	TObjectPtr<APlayerState> LastSeenPlayerState;
};

#undef UE_API
