// Copyright 2025, BlueprintsLab, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DifficultyScalingSettings.generated.h"

class UDifficultyScalingConfig;

/**
 * Defines the settings for the Ultimate Difficulty Scaling plugin, which can be configured in Project Settings.
 */
UCLASS(config = Game, defaultconfig)
class ULTIMATEDIFFICULTYSCALING_API UDifficultyScalingSettings : public UObject
{
    GENERATED_BODY()

public:
    UDifficultyScalingSettings();

    /** A soft pointer to the Difficulty Scaling Config Data Asset that should be used for this project. */
    UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "General", meta = (AllowedClasses = "/Script/UltimateDifficultyScaling.DifficultyScalingConfig"))
    FSoftObjectPath DifficultyConfigAsset;
};