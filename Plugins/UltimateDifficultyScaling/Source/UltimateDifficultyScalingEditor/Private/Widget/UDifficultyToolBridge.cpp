// Copyright 2025, BlueprintsLab, All rights reserved

#include "Widget/UDifficultyToolBridge.h"
#include "SUltimateDifficultyScalingWindow.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"

void UDifficultyToolBridge::RequestInitialState()
{
	if (auto W = OwnerWidget.Pin()) W->RequestInitialStateInternal();
}

void UDifficultyToolBridge::SelectConfig(const FString& ConfigPath)
{
	if (auto W = OwnerWidget.Pin()) W->SelectConfigInternal(ConfigPath);
}

void UDifficultyToolBridge::CreateConfig(const FString& Name)
{
	if (auto W = OwnerWidget.Pin()) W->CreateConfigInternal(Name);
}

void UDifficultyToolBridge::AddDifficultyName(const FString& Name)
{
	if (auto W = OwnerWidget.Pin()) W->AddDifficultyNameInternal(Name);
}

void UDifficultyToolBridge::RenameDifficulty(int32 Index, const FString& Name)
{
	if (auto W = OwnerWidget.Pin()) W->RenameDifficultyInternal(Index, Name);
}

void UDifficultyToolBridge::RemoveDifficultyName(int32 Index)
{
	if (auto W = OwnerWidget.Pin()) W->RemoveDifficultyNameInternal(Index);
}

void UDifficultyToolBridge::RefreshList()
{
	if (auto W = OwnerWidget.Pin()) W->RefreshListInternal();
}

void UDifficultyToolBridge::AddBlueprints()
{
	if (auto W = OwnerWidget.Pin()) W->AddBlueprintsInternal();
}

void UDifficultyToolBridge::RemoveBlueprint(const FString& BlueprintPath)
{
	if (auto W = OwnerWidget.Pin()) W->RemoveBlueprintInternal(BlueprintPath);
}

void UDifficultyToolBridge::ClearAll()
{
	if (auto W = OwnerWidget.Pin()) W->ClearAllInternal();
}

void UDifficultyToolBridge::AddComponent(const FString& BlueprintPath)
{
	if (auto W = OwnerWidget.Pin()) W->AddComponentInternal(BlueprintPath);
}

void UDifficultyToolBridge::SelectBlueprint(const FString& BlueprintPath)
{
	if (auto W = OwnerWidget.Pin()) W->SelectBlueprintInternal(BlueprintPath);
}

void UDifficultyToolBridge::RequestTreeChildren(const FString& NodePath, int32 RequestId)
{
	if (auto W = OwnerWidget.Pin()) W->RequestTreeChildrenInternal(NodePath, RequestId);
}

void UDifficultyToolBridge::AddVariable(const FString& FullPath)
{
	if (auto W = OwnerWidget.Pin()) W->AddVariableInternal(FullPath);
}

void UDifficultyToolBridge::RemoveVariable(const FString& VariableName)
{
	if (auto W = OwnerWidget.Pin()) W->RemoveVariableInternal(VariableName);
}

void UDifficultyToolBridge::SetValue(const FString& VariableName, int32 DifficultyIndex, const FString& Value)
{
	if (auto W = OwnerWidget.Pin()) W->SetValueInternal(VariableName, DifficultyIndex, Value);
}

void UDifficultyToolBridge::CleanOrphans()
{
	if (auto W = OwnerWidget.Pin()) W->CleanOrphansInternal();
}

void UDifficultyToolBridge::SetShowInherited(bool bShow)
{
	if (auto W = OwnerWidget.Pin()) W->SetShowInheritedInternal(bShow);
}

void UDifficultyToolBridge::AiSuggest(const FString& Intent, bool bAllowAdd)
{
	if (auto W = OwnerWidget.Pin()) W->AiSuggestInternal(Intent, bAllowAdd);
}

void UDifficultyToolBridge::ApplyAiSuggestion(const FString& SuggestionJson)
{
	if (auto W = OwnerWidget.Pin()) W->ApplyAiSuggestionInternal(SuggestionJson);
}

void UDifficultyToolBridge::OpenAiSettings()
{
	if (auto W = OwnerWidget.Pin()) W->OpenSettingsViewToTab(TEXT("ai"));
}

void UDifficultyToolBridge::OpenSettings()
{
	if (auto W = OwnerWidget.Pin()) W->OpenSettingsView();
}

void UDifficultyToolBridge::OpenUpsell()
{
	if (auto W = OwnerWidget.Pin()) W->OpenSettingsViewToTab(TEXT("upsell"));
}

void UDifficultyToolBridge::OpenExternalUrl(const FString& Url)
{
	if (!Url.IsEmpty()) FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// HTML loader — read tool.html from Resources/UI, write a live copy under Saved/,
// and return a cache-busted file:// URL. Mirrors UBAAppBridge::BuildAppHtmlDataUri.
// ─────────────────────────────────────────────────────────────────────────────

FString UDifficultyToolBridge::BuildToolHtmlDataUri()
{
	FString Html;
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UltimateDifficultyScaling"));
	if (Plugin.IsValid())
	{
		const FString BasePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("UI"));
		const FString HtmlPath = FPaths::Combine(BasePath, TEXT("tool.html"));
		if (!FFileHelper::LoadFileToString(Html, *HtmlPath))
		{
			Html = TEXT("<html><body style='background:#161616;color:#e06060;font-family:sans-serif;padding:40px'><h2>tool.html not found</h2><p>Expected: ") + HtmlPath + TEXT("</p></body></html>");
		}
	}
	else
	{
		Html = TEXT("<html><body style='background:#161616;color:#e06060;padding:40px'>Plugin not found</body></html>");
	}

	const FString TempDir = FPaths::ProjectSavedDir() / TEXT("UltimateDifficultyScaling");
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*TempDir);
	const FString TempPath = TempDir / TEXT("tool_live.html");
	FFileHelper::SaveStringToFile(Html, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FString AbsPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*TempPath);
	AbsPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	return FString::Printf(TEXT("file:///%s?t=%lld"), *AbsPath, FDateTime::UtcNow().ToUnixTimestamp());
}
