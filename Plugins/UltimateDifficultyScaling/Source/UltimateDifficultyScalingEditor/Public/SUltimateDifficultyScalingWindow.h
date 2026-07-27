// Copyright 2025, BlueprintsLab, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// Forward declarations
class UDifficultyScalingConfig;
class UBlueprint;
class UStruct;
class FProperty;
class SWebBrowser;
class SWidgetSwitcher;
class UDifficultyToolBridge;
class UDifficultySettingsBridge;
struct FAssetData;
class FJsonValue;

/**
 * Host widget for the difficulty-scaling editor. Replaces the old Slate UI with two
 * SWebBrowsers (tool + settings/info) inside an SWidgetSwitcher, and owns the C++↔JS
 * bridge UObjects. All editor operations the JS calls land in the *Internal() methods
 * below, which carry over the reflection/asset logic that used to drive the Slate UI.
 *
 * Architecture mirrors CodeExplainer's SBlueprintAnalystShellWidget.
 */
class SUltimateDifficultyScalingWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUltimateDifficultyScalingWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SUltimateDifficultyScalingWindow();

	// ── Bridge → widget API (public so the bridge UObjects can call directly) ──

	// Config asset
	void RequestInitialStateInternal();
	void SelectConfigInternal(const FString& ConfigPath);
	void CreateConfigInternal(const FString& Name);

	// Difficulty names
	void AddDifficultyNameInternal(const FString& Name);
	void RenameDifficultyInternal(int32 Index, const FString& Name);
	void RemoveDifficultyNameInternal(int32 Index);

	// Blueprints
	void RefreshListInternal();
	void AddBlueprintsInternal();
	void RemoveBlueprintInternal(const FString& BlueprintPath);
	void ClearAllInternal();
	void AddComponentInternal(const FString& BlueprintPath);

	// Variable editor
	void SelectBlueprintInternal(const FString& BlueprintPath);
	void RequestTreeChildrenInternal(const FString& NodePath, int32 RequestId);
	void AddVariableInternal(const FString& FullPath);
	void RemoveVariableInternal(const FString& VariableName);
	void SetValueInternal(const FString& VariableName, int32 DifficultyIndex, const FString& Value);
	void CleanOrphansInternal();
	void SetShowInheritedInternal(bool bShow);

	// AI
	void AiSuggestInternal(const FString& Intent, bool bAllowAdd);
	void ApplyAiSuggestionInternal(const FString& SuggestionJson);

	// Settings/info view switching
	void OpenSettingsView();
	void OpenSettingsViewToTab(const FString& TabName);
	void CloseSettingsView();

protected:
	// Best-effort: dragging Blueprint assets from the Content Browser onto the panel.
	// (The native "Add Blueprint" picker is the reliable path; CEF may swallow drops.)
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	// Browser load callbacks
	void OnToolBrowserLoaded();
	void OnSettingsBrowserLoaded();
	void OnSettingsBrowserConsoleMessage(const FString& Message, const FString& Source, int32 Line, enum class EWebBrowserConsoleLogSeverity Severity);
	void OnSettingsBrowserTitleChanged(const FText& Title);
	void DispatchSettingsRpc(const FString& Payload);
	void PushPluginInfoToJs();
	void PushSettingsToJs();

	// AI helpers
	/** Build the per-variable spec list (control/options/integer/current) the AI prompt + validator use. */
	void BuildAiVariableSpecs(bool bAllowAdd, TArray<TSharedPtr<class FJsonObject>>& OutSpecs) const;

	// JS push helpers (C++ → JS)
	void PushInitialState();
	void PushBlueprintList();
	void PushVariableData();
	void PushToast(const FString& Message, const FString& Kind);
	void ExecOnTool(const FString& Js);

	// Config / asset helpers
	UDifficultyScalingConfig* LoadActiveConfig();
	void GatherConfigAssets(TArray<FAssetData>& Out) const;
	void SyncActiveConfigToProjectSettings();
	bool AddBlueprintToConfig(UBlueprint* Blueprint);   // returns true if newly added

	// Reflection helpers (carried over from the old Slate window)
	FProperty* ResolvePropertyByPath(const FString& DottedPath) const;
	bool IsVariablePathValid(const FString& VariablePath) const;
	FString GetSmartDefaultForProperty(FProperty* Property) const;
	/** Build the per-variable widget descriptor (control kind, enum options, integer flag). */
	TSharedPtr<class FJsonObject> MakeControlDescriptor(const FString& VariablePath, FName StoredPropertyType) const;
	/** Emit the direct children of a struct/class node as JSON tree nodes. */
	void BuildChildNodes(UStruct* TargetStruct, const FString& CurrentPath, UStruct* StopAtStruct, TArray<TSharedPtr<FJsonValue>>& Out) const;
	/** Resolve the UStruct a given tree node path expands into (Self/component/struct). Returns null for leaves. */
	UStruct* ResolveContainerStructForPath(const FString& NodePath, UStruct*& OutStopAtStruct) const;

	// State
	TSharedPtr<SWidgetSwitcher> MainSwitcher;
	TSharedPtr<SWebBrowser>     ToolBrowser;
	TSharedPtr<SWebBrowser>     SettingsBrowser;

	TObjectPtr<UDifficultyToolBridge>     ToolBridge;
	TObjectPtr<UDifficultySettingsBridge> SettingsBridge;

	TWeakObjectPtr<UDifficultyScalingConfig> ConfigAsset;
	TWeakObjectPtr<UBlueprint>               SelectedBlueprint;
	bool bShowInheritedVariables = false;

	bool    bSettingsBrowserLoadKicked = false;
	int32   LastSettingsRpcCounter = -1;
	FString PendingSettingsTab;   // tab to focus once the (lazily loaded) settings page is ready
};
