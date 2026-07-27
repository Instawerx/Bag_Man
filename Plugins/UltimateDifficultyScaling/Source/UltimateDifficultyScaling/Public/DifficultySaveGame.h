// Copyright 2025, BlueprintsLab, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DifficultySaveGame.generated.h"

/**
 * A simple SaveGame object to store the currently selected difficulty index.
 */
UCLASS()
class ULTIMATEDIFFICULTYSCALING_API UDifficultySaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    // The index of the difficulty level that the player has saved.
    UPROPERTY()
    int32 SavedDifficultyIndex;

    // A unique identifier for our save game slot.
    inline static const FString SaveSlotName = TEXT("DifficultySettings");
};