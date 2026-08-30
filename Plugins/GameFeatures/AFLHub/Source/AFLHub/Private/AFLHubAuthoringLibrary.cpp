// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHubAuthoringLibrary.h"

#include "GameFeatureAction_AddComponents.h" // the append hatch below

#include "AFLHub.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLHubAuthoringLibrary)

bool UAFLHubAuthoringLibrary::SetWorldSettingsDefaultExperience(AWorldSettings* InWorldSettings, const FString& ExperienceClassPath)
{
#if WITH_EDITOR
	if (!InWorldSettings)
	{
		return false;
	}
	FProperty* Prop = InWorldSettings->GetClass()->FindPropertyByName(TEXT("DefaultGameplayExperience"));
	if (!Prop)
	{
		UE_LOG(LogAFLHub, Warning, TEXT("AFL_HUB_AUTH: no DefaultGameplayExperience property on %s."),
			*InWorldSettings->GetClass()->GetName());
		return false;
	}

	InWorldSettings->Modify();
	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(InWorldSettings);
	const TCHAR* Result = Prop->ImportText_Direct(*ExperienceClassPath, ValuePtr, InWorldSettings, PPF_None);
	InWorldSettings->MarkPackageDirty();

	UE_LOG(LogAFLHub, Log, TEXT("AFL_HUB_AUTH: DefaultGameplayExperience <- '%s' on %s: %s."),
		*ExperienceClassPath, *InWorldSettings->GetPathName(), Result ? TEXT("OK") : TEXT("IMPORT FAILED"));
	return Result != nullptr;
#else
	return false;
#endif
}

int32 UAFLHubAuthoringLibrary::AppendAddComponentsEntry(UObject* AddComponentsAction,
	const FString& ActorClassPath, const FString& ComponentClassPath, bool bClient, bool bServer)
{
#if WITH_EDITOR
	UGameFeatureAction_AddComponents* Action = Cast<UGameFeatureAction_AddComponents>(AddComponentsAction);
	if (!Action)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HubAuthoring] AppendAddComponentsEntry: not a GameFeatureAction_AddComponents (%s)."),
			AddComponentsAction ? *AddComponentsAction->GetClass()->GetName() : TEXT("null"));
		return -1;
	}
	FGameFeatureComponentEntry Entry;
	Entry.ActorClass = TSoftClassPtr<AActor>(FSoftClassPath(ActorClassPath));
	Entry.ComponentClass = TSoftClassPtr<UActorComponent>(FSoftClassPath(ComponentClassPath));
	Entry.bClientComponent = bClient ? 1 : 0;
	Entry.bServerComponent = bServer ? 1 : 0;
	Action->ComponentList.Add(Entry);
	Action->MarkPackageDirty();
	UE_LOG(LogTemp, Display, TEXT("[HubAuthoring] AddComponents += %s on %s (count=%d)."),
		*ComponentClassPath, *ActorClassPath, Action->ComponentList.Num());
	return Action->ComponentList.Num();
#else
	return -1;
#endif
}

TArray<FString> UAFLHubAuthoringLibrary::DumpAddComponentsEntries(UObject* AddComponentsAction)
{
	TArray<FString> Out;
#if WITH_EDITOR
	const UGameFeatureAction_AddComponents* Action = Cast<UGameFeatureAction_AddComponents>(AddComponentsAction);
	if (!Action) { Out.Add(TEXT("NOT an AddComponents action")); return Out; }
	for (const FGameFeatureComponentEntry& E : Action->ComponentList)
	{
		Out.Add(FString::Printf(TEXT("%s -> %s (%s%s)"),
			*E.ActorClass.ToString(), *E.ComponentClass.ToString(),
			E.bClientComponent ? TEXT("C") : TEXT(""), E.bServerComponent ? TEXT("S") : TEXT("")));
	}
#endif
	return Out;
}

int32 UAFLHubAuthoringLibrary::RemoveAddComponentsEntriesMatching(UObject* AddComponentsAction, const FString& ComponentPathContains)
{
#if WITH_EDITOR
	UGameFeatureAction_AddComponents* Action = Cast<UGameFeatureAction_AddComponents>(AddComponentsAction);
	if (!Action || ComponentPathContains.IsEmpty()) { return 0; }
	const int32 Before = Action->ComponentList.Num();
	Action->ComponentList.RemoveAll([&ComponentPathContains](const FGameFeatureComponentEntry& E)
	{
		return E.ComponentClass.ToString().Contains(ComponentPathContains);
	});
	const int32 Removed = Before - Action->ComponentList.Num();
	if (Removed > 0) { Action->MarkPackageDirty(); }
	UE_LOG(LogTemp, Display, TEXT("[HubAuthoring] AddComponents -= %d entr(ies) matching '%s' (count=%d)."),
		Removed, *ComponentPathContains, Action->ComponentList.Num());
	return Removed;
#else
	return 0;
#endif
}
