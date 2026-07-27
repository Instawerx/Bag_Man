// Copyright 2025, BlueprintsLab, All rights reserved

#include "DifficultyManagerSubsystem.h"
#include "DifficultyScalingConfig.h" 
#include "Engine/AssetManager.h"  
#include "DifficultyScalingSettings.h"
#include "Engine/StreamableManager.h"   
#include "Kismet/GameplayStatics.h" 
#include "DifficultySaveGame.h" 

void UDifficultyManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    CurrentDifficultyIndex = 0; // Default to the first difficulty
    
    const UDifficultyScalingSettings* Settings = GetDefault<UDifficultyScalingSettings>();
    if (Settings && !Settings->DifficultyConfigAsset.IsNull())
    {
        FStreamableManager& StreamableManager = UAssetManager::Get().GetStreamableManager();
        LoadedConfig = StreamableManager.LoadSynchronous<UDifficultyScalingConfig>(Settings->DifficultyConfigAsset);

        if(LoadedConfig)
        {
             UE_LOG(LogTemp, Log, TEXT("DifficultyManagerSubsystem: Successfully loaded DifficultyScalingConfig asset: %s"), *LoadedConfig->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("DifficultyManagerSubsystem: Failed to load DifficultyScalingConfig asset from path: %s"), *Settings->DifficultyConfigAsset.ToString());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DifficultyManagerSubsystem: No DifficultyScalingConfig asset specified in Project Settings."));
    }
    LoadDifficulty();
}

void UDifficultyManagerSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

bool UDifficultyManagerSubsystem::SetCurrentDifficultyByName(const FString& DifficultyName)
{
    if (!LoadedConfig)
    {
        UE_LOG(LogTemp, Error, TEXT("SetCurrentDifficultyByName failed: DifficultyScalingConfig is not loaded."));
        return false;
    }

    const int32 Index = LoadedConfig->DifficultyNames.Find(DifficultyName);
    if (Index != INDEX_NONE)
    {
        return SetCurrentDifficultyByIndex(Index);
    }

    UE_LOG(LogTemp, Warning, TEXT("SetCurrentDifficultyByName: Could not find difficulty named '%s'."), *DifficultyName);
    return false;
}

bool UDifficultyManagerSubsystem::SetCurrentDifficultyByIndex(int32 Index)
{
    if (LoadedConfig && LoadedConfig->DifficultyNames.IsValidIndex(Index))
    {
        if (CurrentDifficultyIndex != Index)
        {
            CurrentDifficultyIndex = Index;
            const FString& NewDifficultyName = LoadedConfig->DifficultyNames[CurrentDifficultyIndex];

            UE_LOG(LogTemp, Log, TEXT("Difficulty changed to: %s (Index: %d)"), *NewDifficultyName, CurrentDifficultyIndex);

            OnDifficultyChanged.Broadcast(CurrentDifficultyIndex, NewDifficultyName);

            SaveDifficulty();
        }
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("SetCurrentDifficultyByIndex: Index %d is not a valid difficulty index."), Index);
    return false;
}

FString UDifficultyManagerSubsystem::GetCurrentDifficultyName() const
{
    if (LoadedConfig && LoadedConfig->DifficultyNames.IsValidIndex(CurrentDifficultyIndex))
    {
        return LoadedConfig->DifficultyNames[CurrentDifficultyIndex];
    }
    return FString(TEXT("Invalid"));
}

void UDifficultyManagerSubsystem::SaveDifficulty()
{
    // Create a new save game object or load an existing one.
    UDifficultySaveGame* SaveGameObject = nullptr;
    if (UGameplayStatics::DoesSaveGameExist(UDifficultySaveGame::SaveSlotName, 0))
    {
        SaveGameObject = Cast<UDifficultySaveGame>(UGameplayStatics::LoadGameFromSlot(UDifficultySaveGame::SaveSlotName, 0));
    }
    else
    {
        SaveGameObject = Cast<UDifficultySaveGame>(UGameplayStatics::CreateSaveGameObject(UDifficultySaveGame::StaticClass()));
    }

    if (SaveGameObject)
    {
        // Set the data to save and write it to the file.
        SaveGameObject->SavedDifficultyIndex = CurrentDifficultyIndex;
        UGameplayStatics::SaveGameToSlot(SaveGameObject, UDifficultySaveGame::SaveSlotName, 0);
        UE_LOG(LogTemp, Log, TEXT("Difficulty saved. Index: %d"), CurrentDifficultyIndex);
    }
}

void UDifficultyManagerSubsystem::LoadDifficulty()
{
    if (UGameplayStatics::DoesSaveGameExist(UDifficultySaveGame::SaveSlotName, 0))
    {
        UDifficultySaveGame* SaveGameObject = Cast<UDifficultySaveGame>(UGameplayStatics::LoadGameFromSlot(UDifficultySaveGame::SaveSlotName, 0));
        if (SaveGameObject)
        {
            // Apply the loaded difficulty index. We call the internal function to avoid an infinite save/load loop.
            SetCurrentDifficultyByIndex(SaveGameObject->SavedDifficultyIndex);
            UE_LOG(LogTemp, Log, TEXT("Difficulty loaded. Index: %d"), SaveGameObject->SavedDifficultyIndex);
        }
    }
}