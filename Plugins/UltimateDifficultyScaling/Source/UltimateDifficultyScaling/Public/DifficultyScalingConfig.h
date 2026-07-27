// Copyright 2025, BlueprintsLab, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Blueprint.h"
#include "DifficultyScalingConfig.generated.h"

/**
 * NEW: A struct to hold the data for a single variable, WITHOUT the Blueprint info.
 * 
 * 
 */
USTRUCT(BlueprintType)
struct FScaledVariableData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Variable Info")
    FName VariableName;

    UPROPERTY(VisibleAnywhere, Category = "Variable Info", meta = (ReadOnly))
    FName PropertyType;

    UPROPERTY(EditAnywhere, Category = "Scaled Values")
    TArray<FString> ScaledValuesAsString;
};

/**
 * NEW: A struct to hold all the difficulty settings for a SINGLE Blueprint.
 */
USTRUCT(BlueprintType)
struct FBlueprintDifficultySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Blueprint")
    TSoftObjectPtr<UBlueprint> TargetBlueprint;

    UPROPERTY(EditAnywhere, Category = "Blueprint", meta = (TitleProperty = "VariableName"))
    TArray<FScaledVariableData> ScaledVariables;
};


/**
 * The central Data Asset, now using our new hierarchical structure.
 */
UCLASS(BlueprintType)
class ULTIMATEDIFFICULTYSCALING_API UDifficultyScalingConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Difficulty Settings")
    TArray<FString> DifficultyNames;

    // --- UPDATED: We now use an array of our new parent struct ---
    UPROPERTY(EditAnywhere, Category = "Scaled Blueprints", meta = (TitleProperty = "TargetBlueprint"))
    TArray<FBlueprintDifficultySettings> BlueprintSettings;
};