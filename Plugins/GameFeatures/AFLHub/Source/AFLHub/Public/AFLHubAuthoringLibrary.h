// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "AFLHubAuthoringLibrary.generated.h"

class AWorldSettings;

/**
 * Editor-only authoring escape hatches for the hub build scripts.
 *
 * ALyraWorldSettings.DefaultGameplayExperience is protected + EditDefaultsOnly, so BOTH python
 * set_editor_property and the bridge ImportText setter refuse to write it on a placed level
 * instance (CPF_DisableEditOnInstance; the known Lyra WorldSettings gate). Lyra's own details
 * panel bypasses the gate with a custom view; this library is the scripted equivalent -- raw
 * FProperty ImportText below the edit gate. WITH_EDITOR only; a no-op that returns false in
 * any cooked build.
 */
UCLASS()
class AFLHUB_API UAFLHubAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Write WorldSettings->DefaultGameplayExperience from a soft-class path string
	 *  (e.g. "/AFLHub/Experiences/B_AFL_Experience_Hub.B_AFL_Experience_Hub_C"). */
	UFUNCTION(BlueprintCallable, Category = "AFL|Hub|Authoring", meta = (DevelopmentOnly))
	static bool SetWorldSettingsDefaultExperience(AWorldSettings* InWorldSettings, const FString& ExperienceClassPath);

	/** Append one FGameFeatureComponentEntry to a GameFeatureAction_AddComponents. Exists because
	 *  the action's ComponentList is unreachable from python (no reflection surface exposed) and
	 *  the bridge setter cannot hop object pointers -- the same class of gate as WorldSettings
	 *  above. Returns the NEW list count, or -1 on failure (honest readback in one call). */
	UFUNCTION(BlueprintCallable, Category = "AFL|Hub|Authoring", meta = (DevelopmentOnly))
	static int32 AppendAddComponentsEntry(UObject* AddComponentsAction, const FString& ActorClassPath,
		const FString& ComponentClassPath, bool bClient, bool bServer);

	/** Dump every entry of a GameFeatureAction_AddComponents as "Actor -> Component (C/S)" strings.
	 *  The read half of the append hatch -- diffing experiences beats probing one guess at a time. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Hub|Authoring", meta = (DevelopmentOnly))
	static TArray<FString> DumpAddComponentsEntries(UObject* AddComponentsAction);

	/** Remove every entry whose ComponentClass path CONTAINS the given substring. Returns removed
	 *  count. The cleanup half -- normalizing a hand-grown list to the canonical stack. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Hub|Authoring", meta = (DevelopmentOnly))
	static int32 RemoveAddComponentsEntriesMatching(UObject* AddComponentsAction, const FString& ComponentPathContains);
};
