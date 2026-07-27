// Copyright 2025, BlueprintsLab, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DifficultyScalableComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ULTIMATEDIFFICULTYSCALING_API UDifficultyScalableComponent : public UActorComponent
{
    GENERATED_BODY()

public:	
    // Sets default values for this component's properties
    UDifficultyScalableComponent();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

public:	
    /**
     * This function reads the central difficulty config and applies the correct variable values to this component's owner.
     * It is called automatically on BeginPlay and whenever the difficulty changes.
     */
    UFUNCTION(BlueprintCallable, Category = "Difficulty Scaling")
    void ApplyCurrentDifficultySettings();

private:
    /** Handles the logic when the global difficulty setting is changed. */
    UFUNCTION()
    void OnDifficultyChangedHandler(int32 NewDifficultyIndex, const FString& NewDifficultyName);
};