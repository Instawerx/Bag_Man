// Copyright 2025, BlueprintsLab, All rights reserved

#include "Widget/UDifficultySettingsBridge.h"
#include "SUltimateDifficultyScalingWindow.h"

#include "AI/UDS_ApiKeyManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"

void UDifficultySettingsBridge::CloseSettings()
{
	if (auto W = OwnerWidget.Pin()) W->CloseSettingsView();
}

void UDifficultySettingsBridge::OpenExternalUrl(const FString& Url)
{
	if (!Url.IsEmpty()) FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

FString UDifficultySettingsBridge::SettingsToJson()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	TArray<TSharedPtr<FJsonValue>> SlotsArr;
	for (const FUDS_ApiKeySlot& Slot : FUDS_ApiKeyManager::Get().GetAllSlots())
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("slotIndex"),       Slot.SlotIndex);
		O->SetStringField(TEXT("name"),            Slot.Name);
		O->SetStringField(TEXT("provider"),        Slot.Provider);
		O->SetStringField(TEXT("apiKey"),          Slot.ApiKey);
		O->SetStringField(TEXT("customBaseURL"),   Slot.CustomBaseURL);
		O->SetStringField(TEXT("customModelName"), Slot.CustomModelName);
		O->SetStringField(TEXT("geminiModel"),     Slot.GeminiModel);
		O->SetStringField(TEXT("openaiModel"),     Slot.OpenAIModel);
		O->SetStringField(TEXT("claudeModel"),     Slot.ClaudeModel);
		O->SetStringField(TEXT("deepseekModel"),   Slot.DeepSeekModel);
		SlotsArr.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("slots"), SlotsArr);
	Root->SetNumberField(TEXT("activeSlot"), FUDS_ApiKeyManager::Get().GetActiveSlotIndex());

	TArray<TSharedPtr<FJsonValue>> Providers;
	for (const TCHAR* P : { TEXT("OpenAI"), TEXT("Anthropic"), TEXT("Gemini"), TEXT("DeepSeek"), TEXT("Custom") })
	{
		Providers.Add(MakeShared<FJsonValueString>(FString(P)));
	}
	Root->SetArrayField(TEXT("providers"), Providers);

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

FString UDifficultySettingsBridge::LoadAllSettings()
{
	return SettingsToJson();
}

FString UDifficultySettingsBridge::RequestPluginInfo()
{
	TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UltimateDifficultyScaling")))
	{
		const FPluginDescriptor& D = Plugin->GetDescriptor();
		Info->SetStringField(TEXT("name"),       D.FriendlyName);
		Info->SetStringField(TEXT("version"),    D.VersionName);
		Info->SetStringField(TEXT("ueVersion"),  D.EngineVersion);
		Info->SetStringField(TEXT("docsUrl"),    D.DocsURL);
		Info->SetStringField(TEXT("supportUrl"), D.SupportURL);
	}

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Info.ToSharedRef(), Writer);
	return Out;
}

// ─────────────────────────────────────────────────────────────────────────────
// HTML loader — read settings.html, inline the upsell hero image as base64,
// write a live copy under Saved/, return a cache-busted file:// URL.
// Mirrors UBASettingsBridge::BuildSettingsHtmlDataUri.
// ─────────────────────────────────────────────────────────────────────────────

FString UDifficultySettingsBridge::BuildSettingsHtmlDataUri()
{
	FString Html;
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UltimateDifficultyScaling"));
	if (Plugin.IsValid())
	{
		const FString BasePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("UI"));
		const FString HtmlPath = FPaths::Combine(BasePath, TEXT("settings.html"));
		if (!FFileHelper::LoadFileToString(Html, *HtmlPath))
		{
			Html = TEXT("<html><body style='background:#161616;color:#e06060;font-family:sans-serif;padding:40px'><h2>settings.html not found</h2><p>Expected: ") + HtmlPath + TEXT("</p></body></html>");
		}

		// Inline the hero image so the file:// HTML needs no sibling resource resolution.
		const FString HeroPath = FPaths::Combine(BasePath, TEXT("ultimate_copilot_hero.jpg"));
		TArray<uint8> HeroBytes;
		if (FFileHelper::LoadFileToArray(HeroBytes, *HeroPath) && HeroBytes.Num() > 0)
		{
			Html.ReplaceInline(TEXT("%%HERO_IMG_BASE64%%"), *FBase64::Encode(HeroBytes));
		}
		else
		{
			Html.ReplaceInline(TEXT("%%HERO_IMG_BASE64%%"), TEXT(""));
		}
	}
	else
	{
		Html = TEXT("<html><body>Plugin not found</body></html>");
	}

	const FString TempDir = FPaths::ProjectSavedDir() / TEXT("UltimateDifficultyScaling");
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*TempDir);
	const FString TempPath = TempDir / TEXT("settings_live.html");
	FFileHelper::SaveStringToFile(Html, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	FString AbsPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*TempPath);
	AbsPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	return FString::Printf(TEXT("file:///%s?t=%lld"), *AbsPath, FDateTime::UtcNow().ToUnixTimestamp());
}
