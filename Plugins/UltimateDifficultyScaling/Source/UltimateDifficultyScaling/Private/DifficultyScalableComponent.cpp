// Copyright 2025, BlueprintsLab, All rights reserved

#include "DifficultyScalableComponent.h"
#include "DifficultyManagerSubsystem.h"
#include "DifficultyScalingConfig.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

// Sets default values for this component's properties
UDifficultyScalableComponent::UDifficultyScalableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UDifficultyScalableComponent::BeginPlay()
{
    Super::BeginPlay();

    ApplyCurrentDifficultySettings();

    if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this))
    {
        if (UDifficultyManagerSubsystem* DifficultySubsystem = GameInstance->GetSubsystem<UDifficultyManagerSubsystem>())
        {
            DifficultySubsystem->OnDifficultyChanged.AddDynamic(this, &UDifficultyScalableComponent::OnDifficultyChangedHandler);
        }
    }
}


void UDifficultyScalableComponent::ApplyCurrentDifficultySettings()
{
    AActor* OwnerActor = GetOwner();
    UDifficultyManagerSubsystem* DifficultySubsystem = UGameplayStatics::GetGameInstance(this)->GetSubsystem<UDifficultyManagerSubsystem>();
    if (!OwnerActor || !DifficultySubsystem) return;

    const UDifficultyScalingConfig* Config = DifficultySubsystem->GetDifficultyConfig();
    const int32 CurrentDifficultyIndex = DifficultySubsystem->GetCurrentDifficultyIndex();
    if (!Config) return;

    UClass* OwnerClass = OwnerActor->GetClass();

    // 1. Find the settings for this actor's specific Blueprint
    const FBlueprintDifficultySettings* BPSettings = Config->BlueprintSettings.FindByPredicate(
        [&](const FBlueprintDifficultySettings& Settings) {
            return Settings.TargetBlueprint.Get() && OwnerClass->IsChildOf(Settings.TargetBlueprint->GeneratedClass);
        });

    if (!BPSettings)
    {
        return; // No settings for this Blueprint class
    }

    // 2. Loop through the variables within that specific setting
    for (const FScaledVariableData& ScaledVariable : BPSettings->ScaledVariables)
    {
        if (!ScaledVariable.ScaledValuesAsString.IsValidIndex(CurrentDifficultyIndex)) continue;

        TArray<FString> PathParts;
        ScaledVariable.VariableName.ToString().ParseIntoArray(PathParts, TEXT("."));

        if (PathParts.IsEmpty()) continue;

        void* CurrentContainer = OwnerActor;
        UStruct* CurrentStructDefinition = OwnerClass;

        bool bPathIsValid = true;
        // 3. Traverse the path (e.g., "MyComponent", "MyStats") to find the direct container of the final variable
        for (int32 i = 0; i < PathParts.Num() - 1; ++i)
        {
            FProperty* PathProperty = CurrentStructDefinition->FindPropertyByName(FName(*PathParts[i]));
            if (!PathProperty)
            {
                bPathIsValid = false;
                break;
            }

            if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(PathProperty))
            {
                UObject* SubObject = ObjectProperty->GetObjectPropertyValue_InContainer(CurrentContainer);
                if (!SubObject) { bPathIsValid = false; break; }
                CurrentContainer = SubObject;
                CurrentStructDefinition = SubObject->GetClass();
            }
            else if (FStructProperty* StructProperty = CastField<FStructProperty>(PathProperty))
            {
                CurrentContainer = StructProperty->ContainerPtrToValuePtr<void>(CurrentContainer);
                CurrentStructDefinition = StructProperty->Struct;
            }
            else
            {
                // This path segment is not a container we can traverse into (e.g., it's a float)
                bPathIsValid = false;
                break;
            }
        }

        // 4. If the path was valid, find and set the final property
        if (bPathIsValid)
        {
            FName FinalPropertyName = FName(*PathParts.Last());
            FProperty* FinalProperty = CurrentStructDefinition->FindPropertyByName(FinalPropertyName);

            if (FinalProperty)
            {
                const FString& ValueString = ScaledVariable.ScaledValuesAsString[CurrentDifficultyIndex];
                FinalProperty->ImportText_Direct(*ValueString, FinalProperty->ContainerPtrToValuePtr<void>(CurrentContainer), OwnerActor, 0);

                UE_LOG(LogTemp, Log, TEXT("Applied difficulty setting: %s -> %s = %s"), *OwnerActor->GetName(), *ScaledVariable.VariableName.ToString(), *ValueString);
            }
        }
    }
}

void UDifficultyScalableComponent::OnDifficultyChangedHandler(int32 NewDifficultyIndex, const FString& NewDifficultyName)
{
    UE_LOG(LogTemp, Log, TEXT("Difficulty changed, reapplying settings for '%s'."), *GetOwner()->GetName());
    ApplyCurrentDifficultySettings();
}