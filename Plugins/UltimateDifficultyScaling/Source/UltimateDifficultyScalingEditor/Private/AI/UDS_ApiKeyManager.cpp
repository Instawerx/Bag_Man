// Copyright 2025, BlueprintsLab, All rights reserved

#include "AI/UDS_ApiKeyManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"

namespace
{
	static const TCHAR* DefaultGeminiModel   = TEXT("gemini-2.5-flash");
	static const TCHAR* DefaultOpenAIModel   = TEXT("gpt-4o-mini");
	static const TCHAR* DefaultClaudeModel   = TEXT("claude-sonnet-4-5");
	static const TCHAR* DefaultDeepSeekModel = TEXT("deepseek-chat");
}

FUDS_ApiKeyManager& FUDS_ApiKeyManager::Get()
{
	static FUDS_ApiKeyManager Instance;
	return Instance;
}

FUDS_ApiKeyManager::FUDS_ApiKeyManager()
{
	for (int32 i = 0; i < UDS_MAX_API_KEY_SLOTS; ++i) Slots[i].SlotIndex = i;
	LoadFromConfig();
}

FUDS_ApiKeyManager::~FUDS_ApiKeyManager()
{
	SaveToConfig();
}

FString FUDS_ApiKeyManager::GetConfigFilePath()
{
	return FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("UltimateDifficultyScaling"), TEXT("api_keys.json"));
}

void FUDS_ApiKeyManager::LoadFromConfig()
{
	const FString ConfigPath = GetConfigFilePath();
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigPath)) return;

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ConfigPath)) return;

	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid()) return;

	FScopeLock Lock(&SlotsLock);
	ActiveSlotIndex = RootObj->GetIntegerField(TEXT("ActiveSlot"));
	if (ActiveSlotIndex < 0 || ActiveSlotIndex >= UDS_MAX_API_KEY_SLOTS) ActiveSlotIndex = 0;

	const TArray<TSharedPtr<FJsonValue>>* SlotsArrayPtr = nullptr;
	if (!RootObj->TryGetArrayField(TEXT("Slots"), SlotsArrayPtr)) return;

	const int32 Count = FMath::Min(SlotsArrayPtr->Num(), (int32)UDS_MAX_API_KEY_SLOTS);
	for (int32 i = 0; i < Count; ++i)
	{
		const TSharedPtr<FJsonObject>& SlotObj = (*SlotsArrayPtr)[i]->AsObject();
		if (!SlotObj.IsValid()) continue;

		SlotObj->TryGetStringField(TEXT("Name"),            Slots[i].Name);
		SlotObj->TryGetStringField(TEXT("Provider"),        Slots[i].Provider);
		SlotObj->TryGetStringField(TEXT("ApiKey"),          Slots[i].ApiKey);
		SlotObj->TryGetStringField(TEXT("CustomBaseURL"),   Slots[i].CustomBaseURL);
		SlotObj->TryGetStringField(TEXT("CustomModelName"), Slots[i].CustomModelName);
		SlotObj->TryGetStringField(TEXT("GeminiModel"),     Slots[i].GeminiModel);
		SlotObj->TryGetStringField(TEXT("OpenAIModel"),     Slots[i].OpenAIModel);
		SlotObj->TryGetStringField(TEXT("ClaudeModel"),     Slots[i].ClaudeModel);
		SlotObj->TryGetStringField(TEXT("DeepSeekModel"),   Slots[i].DeepSeekModel);

		if (Slots[i].GeminiModel.IsEmpty())   Slots[i].GeminiModel   = DefaultGeminiModel;
		if (Slots[i].OpenAIModel.IsEmpty())   Slots[i].OpenAIModel   = DefaultOpenAIModel;
		if (Slots[i].ClaudeModel.IsEmpty())   Slots[i].ClaudeModel   = DefaultClaudeModel;
		if (Slots[i].DeepSeekModel.IsEmpty()) Slots[i].DeepSeekModel = DefaultDeepSeekModel;

		Slots[i].SlotIndex = i;
	}
}

void FUDS_ApiKeyManager::SaveToConfig()
{
	const FString ConfigPath = GetConfigFilePath();
	const FString Directory = FPaths::GetPath(ConfigPath);
	if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Directory))
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
	}

	TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> SlotsArray;
	{
		FScopeLock Lock(&SlotsLock);
		for (int32 i = 0; i < UDS_MAX_API_KEY_SLOTS; ++i)
		{
			TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
			SlotObj->SetStringField(TEXT("Name"),            Slots[i].Name);
			SlotObj->SetStringField(TEXT("Provider"),        Slots[i].Provider);
			SlotObj->SetStringField(TEXT("ApiKey"),          Slots[i].ApiKey);
			SlotObj->SetStringField(TEXT("CustomBaseURL"),   Slots[i].CustomBaseURL);
			SlotObj->SetStringField(TEXT("CustomModelName"), Slots[i].CustomModelName);
			SlotObj->SetStringField(TEXT("GeminiModel"),     Slots[i].GeminiModel);
			SlotObj->SetStringField(TEXT("OpenAIModel"),     Slots[i].OpenAIModel);
			SlotObj->SetStringField(TEXT("ClaudeModel"),     Slots[i].ClaudeModel);
			SlotObj->SetStringField(TEXT("DeepSeekModel"),   Slots[i].DeepSeekModel);
			SlotsArray.Add(MakeShared<FJsonValueObject>(SlotObj));
		}
		RootObj->SetArrayField(TEXT("Slots"), SlotsArray);
		RootObj->SetNumberField(TEXT("ActiveSlot"), ActiveSlotIndex);
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(JsonString, *ConfigPath);
}

void FUDS_ApiKeyManager::SetSlot(int32 SlotIndex, const FUDS_ApiKeySlot& Slot)
{
	if (SlotIndex < 0 || SlotIndex >= UDS_MAX_API_KEY_SLOTS) return;
	{
		FScopeLock Lock(&SlotsLock);
		Slots[SlotIndex] = Slot;
		Slots[SlotIndex].SlotIndex = SlotIndex;
	}
	SaveToConfig();
}

FUDS_ApiKeySlot FUDS_ApiKeyManager::GetSlot(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= UDS_MAX_API_KEY_SLOTS) return FUDS_ApiKeySlot();
	FScopeLock Lock(&SlotsLock);
	return Slots[SlotIndex];
}

void FUDS_ApiKeyManager::ClearSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= UDS_MAX_API_KEY_SLOTS) return;
	{
		FScopeLock Lock(&SlotsLock);
		Slots[SlotIndex].Clear();
		Slots[SlotIndex].SlotIndex = SlotIndex;
	}
	SaveToConfig();
}

TArray<FUDS_ApiKeySlot> FUDS_ApiKeyManager::GetAllSlots() const
{
	FScopeLock Lock(&SlotsLock);
	TArray<FUDS_ApiKeySlot> Result;
	for (int32 i = 0; i < UDS_MAX_API_KEY_SLOTS; ++i) Result.Add(Slots[i]);
	return Result;
}

int32 FUDS_ApiKeyManager::GetActiveSlotIndex() const
{
	FScopeLock Lock(&SlotsLock);
	return ActiveSlotIndex;
}

void FUDS_ApiKeyManager::SetActiveSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= UDS_MAX_API_KEY_SLOTS) return;
	{
		FScopeLock Lock(&SlotsLock);
		ActiveSlotIndex = SlotIndex;
	}
	SaveToConfig();
}

FUDS_ApiKeySlot FUDS_ApiKeyManager::GetActiveSlot() const
{
	FScopeLock Lock(&SlotsLock);
	return Slots[ActiveSlotIndex];
}

FString FUDS_ApiKeyManager::GetActiveApiKey() const   { return GetActiveSlot().ApiKey; }
FString FUDS_ApiKeyManager::GetActiveProvider() const { return GetActiveSlot().Provider; }

FString FUDS_ApiKeyManager::GetActiveModelName() const
{
	FUDS_ApiKeySlot Slot = GetActiveSlot();
	if (Slot.IsCustomProvider() && !Slot.CustomModelName.IsEmpty()) return Slot.CustomModelName;
	if (Slot.Provider.Equals(TEXT("OpenAI"),    ESearchCase::IgnoreCase) && !Slot.OpenAIModel.IsEmpty())   return Slot.OpenAIModel;
	if ((Slot.Provider.Equals(TEXT("Anthropic"), ESearchCase::IgnoreCase) || Slot.Provider.Equals(TEXT("Claude"), ESearchCase::IgnoreCase)) && !Slot.ClaudeModel.IsEmpty()) return Slot.ClaudeModel;
	if (Slot.Provider.Equals(TEXT("Gemini"),    ESearchCase::IgnoreCase) && !Slot.GeminiModel.IsEmpty())   return Slot.GeminiModel;
	if (Slot.Provider.Equals(TEXT("DeepSeek"),  ESearchCase::IgnoreCase) && !Slot.DeepSeekModel.IsEmpty()) return Slot.DeepSeekModel;

	if (Slot.Provider.Equals(TEXT("OpenAI"),    ESearchCase::IgnoreCase)) return DefaultOpenAIModel;
	if (Slot.Provider.Equals(TEXT("Anthropic"), ESearchCase::IgnoreCase) || Slot.Provider.Equals(TEXT("Claude"), ESearchCase::IgnoreCase)) return DefaultClaudeModel;
	if (Slot.Provider.Equals(TEXT("Gemini"),    ESearchCase::IgnoreCase)) return DefaultGeminiModel;
	if (Slot.Provider.Equals(TEXT("DeepSeek"),  ESearchCase::IgnoreCase)) return DefaultDeepSeekModel;
	return DefaultOpenAIModel;
}

bool FUDS_ApiKeyManager::HasAnyConfiguredSlot() const
{
	FScopeLock Lock(&SlotsLock);
	for (int32 i = 0; i < UDS_MAX_API_KEY_SLOTS; ++i)
	{
		if (!Slots[i].ApiKey.IsEmpty() && !Slots[i].Provider.IsEmpty()) return true;
		if (Slots[i].IsCustomProvider() && !Slots[i].CustomBaseURL.IsEmpty()) return true;
	}
	return false;
}
