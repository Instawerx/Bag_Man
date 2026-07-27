// Copyright 2025, BlueprintsLab, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ScriptDelegates.h" 
#include "DifficultyManagerSubsystem.generated.h"

class UDifficultyScalingConfig;

// Delegate that will be broadcast whenever the difficulty changes.
// It sends the new difficulty index and name as parameters.
//
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDifficultyChangedSignature, int32, NewDifficultyIndex, const FString&, NewDifficultyName);

/**
 * Manages the game's current difficulty state.
 */
UCLASS()
class ULTIMATEDIFFICULTYSCALING_API UDifficultyManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Begin USubsystem override
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    // End USubsystem override

    /** Sets the current difficulty by its name as defined in the DifficultyScalingConfig. */
    UFUNCTION(BlueprintCallable, Category = "Difficulty Scaling")
    bool SetCurrentDifficultyByName(const FString& DifficultyName);

    /** Sets the current difficulty by its index in the DifficultyScalingConfig array. */
    UFUNCTION(BlueprintCallable, Category = "Difficulty Scaling")
    bool SetCurrentDifficultyByIndex(int32 Index);

    /** Returns the index of the currently active difficulty. */
    UFUNCTION(BlueprintPure, Category = "Difficulty Scaling")
    int32 GetCurrentDifficultyIndex() const { return CurrentDifficultyIndex; }
    
    /** Returns the name of the currently active difficulty. */
    UFUNCTION(BlueprintPure, Category = "Difficulty Scaling")
    FString GetCurrentDifficultyName() const;

    /** Returns a read-only pointer to the loaded Difficulty Scaling Config asset. */
    const UDifficultyScalingConfig* GetDifficultyConfig() const { return LoadedConfig; }

    /** Saves the current difficulty index to a file. This is called automatically when difficulty changes. */
    UFUNCTION(BlueprintCallable, Category = "Difficulty Scaling")
    void SaveDifficulty();

    /** Loads the difficulty index from a file. This is called automatically when the game starts. */
    UFUNCTION(BlueprintCallable, Category = "Difficulty Scaling")
    void LoadDifficulty();

public:
    /** This delegate is broadcast whenever the difficulty is successfully changed. */
    UPROPERTY(BlueprintAssignable, Category = "Difficulty Scaling")
    FOnDifficultyChangedSignature OnDifficultyChanged;

private:
    /** A reference to the Difficulty Scaling Config asset defined in the project settings. */
    UPROPERTY()
    TObjectPtr<UDifficultyScalingConfig> LoadedConfig;
    
    /** The index of the current difficulty level, corresponding to the DifficultyNames array in the LoadedConfig. */
    UPROPERTY()
    int32 CurrentDifficultyIndex;
};