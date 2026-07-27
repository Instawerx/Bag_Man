// Copyright 2025, BlueprintsLab, All rights reserved
#pragma once

#include "CoreMinimal.h"

#define UDS_MAX_API_KEY_SLOTS 5

/**
 * A single bring-your-own-key API slot. Five of these live in this plugin's own
 * (isolated) api_keys.json — independent of any other BlueprintsLab plugin.
 * Provider literals: "OpenAI" | "Anthropic" | "Gemini" | "DeepSeek" | "Custom".
 * Custom targets any OpenAI-compatible base URL (Ollama, LM Studio, OpenRouter, vLLM, ...).
 */
struct FUDS_ApiKeySlot
{
	int32   SlotIndex = -1;
	FString Name;
	FString Provider;
	FString ApiKey;

	// Custom provider only
	FString CustomBaseURL;
	FString CustomModelName;

	// Remembered per-provider model so swapping providers in the dropdown keeps your choice.
	FString GeminiModel;
	FString OpenAIModel;
	FString ClaudeModel;
	FString DeepSeekModel;

	bool IsCustomProvider() const { return Provider.Equals(TEXT("Custom"), ESearchCase::IgnoreCase); }

	void Clear()
	{
		const int32 Saved = SlotIndex;
		*this = FUDS_ApiKeySlot();
		SlotIndex = Saved;
	}
};
