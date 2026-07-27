// Copyright 2025, BlueprintsLab, All rights reserved
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UDifficultyToolBridge.generated.h"

class SUltimateDifficultyScalingWindow;
class SWebBrowser;

/**
 * Bridge between tool.html JS and the host Slate widget.
 *   JS calls:   window.ue.app.<method>(args)   (UFUNCTION names arrive lowercased)
 *   C++ pushes: BrowserRef->ExecuteJavascript("on<Foo>(<json>)")
 *
 * Each method is a thin forwarder to a *Internal() on the owning widget, which holds
 * all the state and reflection logic. This mirrors UBAAppBridge from CodeExplainer.
 */
UCLASS()
class UDifficultyToolBridge : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SUltimateDifficultyScalingWindow> OwnerWidget;
	TWeakPtr<SWebBrowser> BrowserRef;

	// ── Lifecycle ──
	UFUNCTION() void RequestInitialState();

	// ── Config asset ──
	UFUNCTION() void SelectConfig(const FString& ConfigPath);
	UFUNCTION() void CreateConfig(const FString& Name);

	// ── Difficulty names ──
	UFUNCTION() void AddDifficultyName(const FString& Name);
	UFUNCTION() void RenameDifficulty(int32 Index, const FString& Name);
	UFUNCTION() void RemoveDifficultyName(int32 Index);

	// ── Blueprints ──
	UFUNCTION() void RefreshList();
	UFUNCTION() void AddBlueprints();                 // native multi-select asset picker
	UFUNCTION() void RemoveBlueprint(const FString& BlueprintPath);
	UFUNCTION() void ClearAll();
	UFUNCTION() void AddComponent(const FString& BlueprintPath);

	// ── Variable editor ──
	UFUNCTION() void SelectBlueprint(const FString& BlueprintPath);
	UFUNCTION() void RequestTreeChildren(const FString& NodePath, int32 RequestId);
	UFUNCTION() void AddVariable(const FString& FullPath);
	UFUNCTION() void RemoveVariable(const FString& VariableName);
	UFUNCTION() void SetValue(const FString& VariableName, int32 DifficultyIndex, const FString& Value);
	UFUNCTION() void CleanOrphans();
	UFUNCTION() void SetShowInherited(bool bShow);

	// ── AI ──
	UFUNCTION() void AiSuggest(const FString& Intent, bool bAllowAdd);
	UFUNCTION() void ApplyAiSuggestion(const FString& SuggestionJson);
	UFUNCTION() void OpenAiSettings();

	// ── Cross-shell / external ──
	UFUNCTION() void OpenSettings();
	UFUNCTION() void OpenUpsell();
	UFUNCTION() void OpenExternalUrl(const FString& Url);

	// ── HTML loader ──
	static FString BuildToolHtmlDataUri();
};
