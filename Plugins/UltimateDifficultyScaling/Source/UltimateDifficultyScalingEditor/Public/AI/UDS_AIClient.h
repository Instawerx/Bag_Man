// Copyright 2025, BlueprintsLab, All rights reserved
#pragma once

#include "CoreMinimal.h"

/**
 * Minimal single-shot chat client over the active FUDS_ApiKeyManager slot.
 * Supports OpenAI, Anthropic, Gemini, DeepSeek, and any OpenAI-compatible Custom endpoint.
 * Non-streaming, no tool-calling — just "system + user prompt in, assistant text out".
 */
class FUDS_AIClient
{
public:
	/** OnDone(bSuccess, ReplyText, ErrorMessage). Always invoked on the game thread. */
	static void Send(const FString& SystemPrompt, const FString& UserPrompt,
		TFunction<void(bool, const FString&, const FString&)> OnDone);
};
