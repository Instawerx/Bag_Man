// Copyright 2025, BlueprintsLab, All rights reserved

#include "AI/UDS_AIClient.h"
#include "AI/UDS_ApiKeyManager.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString AISerializeObject(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	TSharedPtr<FJsonObject> MsgObj(const FString& Role, const FString& Content)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("role"), Role);
		O->SetStringField(TEXT("content"), Content);
		return O;
	}

	FString BuildCustomUrl(const FString& BaseUrl)
	{
		FString Url = BaseUrl.TrimStartAndEnd();
		if (Url.IsEmpty()) return TEXT("https://api.openai.com/v1/chat/completions");
		if (!Url.Contains(TEXT("anthropic.com")) && !Url.Contains(TEXT("?"))
			&& !Url.EndsWith(TEXT("/chat/completions")))
		{
			Url.RemoveFromEnd(TEXT("/"));
			Url += TEXT("/chat/completions");
		}
		return Url;
	}

	/** Pull the assistant text out of any of the three response shapes we support. */
	FString ExtractReply(const TSharedPtr<FJsonObject>& Root)
	{
		if (!Root.IsValid()) return FString();
		FString Out;

		// OpenAI / DeepSeek / Custom: choices[0].message.content
		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (Root->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
		{
			const TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
			const TSharedPtr<FJsonObject>* Message = nullptr;
			if (Choice.IsValid() && Choice->TryGetObjectField(TEXT("message"), Message))
			{
				(*Message)->TryGetStringField(TEXT("content"), Out);
			}
			return Out;
		}

		// Gemini: candidates[0].content.parts[0].text
		const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
		if (Root->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
		{
			const TSharedPtr<FJsonObject> Cand = (*Candidates)[0]->AsObject();
			const TSharedPtr<FJsonObject>* Content = nullptr;
			if (Cand.IsValid() && Cand->TryGetObjectField(TEXT("content"), Content))
			{
				const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
				if ((*Content)->TryGetArrayField(TEXT("parts"), Parts) && Parts->Num() > 0)
				{
					(*Parts)[0]->AsObject()->TryGetStringField(TEXT("text"), Out);
				}
			}
			return Out;
		}

		// Anthropic: content[0].text
		const TArray<TSharedPtr<FJsonValue>>* ContentBlocks = nullptr;
		if (Root->TryGetArrayField(TEXT("content"), ContentBlocks) && ContentBlocks->Num() > 0)
		{
			const TSharedPtr<FJsonObject> First = (*ContentBlocks)[0]->AsObject();
			if (First.IsValid()) First->TryGetStringField(TEXT("text"), Out);
		}
		return Out;
	}

	FString ExtractError(const FString& Body)
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return FString();
		const TSharedPtr<FJsonObject>* Err = nullptr;
		if (Root->TryGetObjectField(TEXT("error"), Err))
		{
			FString Msg;
			if ((*Err)->TryGetStringField(TEXT("message"), Msg)) return Msg;
		}
		FString Msg;
		if (Root->TryGetStringField(TEXT("message"), Msg)) return Msg;
		return FString();
	}
}

void FUDS_AIClient::Send(const FString& SystemPrompt, const FString& UserPrompt,
	TFunction<void(bool, const FString&, const FString&)> OnDone)
{
	const FUDS_ApiKeySlot Slot   = FUDS_ApiKeyManager::Get().GetActiveSlot();
	const FString Provider       = Slot.Provider;
	const FString ApiKey         = Slot.ApiKey;
	const FString Model          = FUDS_ApiKeyManager::Get().GetActiveModelName();

	const bool bIsAnthropic = Provider.Equals(TEXT("Anthropic"), ESearchCase::IgnoreCase) || Provider.Equals(TEXT("Claude"), ESearchCase::IgnoreCase);
	const bool bIsGemini    = Provider.Equals(TEXT("Gemini"),   ESearchCase::IgnoreCase);
	const bool bIsCustom    = Slot.IsCustomProvider();
	const bool bIsDeepSeek  = Provider.Equals(TEXT("DeepSeek"), ESearchCase::IgnoreCase);
	const bool bIsOpenAI    = Provider.Equals(TEXT("OpenAI"),   ESearchCase::IgnoreCase);

	if (Provider.IsEmpty() || (ApiKey.IsEmpty() && !(bIsCustom && !Slot.CustomBaseURL.IsEmpty())))
	{
		OnDone(false, FString(), TEXT("No API key configured. Open Settings → AI and set up a provider key."));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetTimeout(90.0f);

	FString Url;
	FString Body;

	if (bIsGemini)
	{
		Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s"), *Model, *ApiKey);

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		// system instruction
		{
			TSharedPtr<FJsonObject> SysPart = MakeShared<FJsonObject>();
			SysPart->SetStringField(TEXT("text"), SystemPrompt);
			TArray<TSharedPtr<FJsonValue>> SysParts; SysParts.Add(MakeShared<FJsonValueObject>(SysPart));
			TSharedPtr<FJsonObject> SysInstr = MakeShared<FJsonObject>();
			SysInstr->SetArrayField(TEXT("parts"), SysParts);
			Root->SetObjectField(TEXT("systemInstruction"), SysInstr);
		}
		// user content
		{
			TSharedPtr<FJsonObject> UserPart = MakeShared<FJsonObject>();
			UserPart->SetStringField(TEXT("text"), UserPrompt);
			TArray<TSharedPtr<FJsonValue>> UserParts; UserParts.Add(MakeShared<FJsonValueObject>(UserPart));
			TSharedPtr<FJsonObject> ContentObj = MakeShared<FJsonObject>();
			ContentObj->SetStringField(TEXT("role"), TEXT("user"));
			ContentObj->SetArrayField(TEXT("parts"), UserParts);
			TArray<TSharedPtr<FJsonValue>> Contents; Contents.Add(MakeShared<FJsonValueObject>(ContentObj));
			Root->SetArrayField(TEXT("contents"), Contents);
		}
		// ask for strict JSON
		{
			TSharedPtr<FJsonObject> GenCfg = MakeShared<FJsonObject>();
			GenCfg->SetStringField(TEXT("responseMimeType"), TEXT("application/json"));
			Root->SetObjectField(TEXT("generationConfig"), GenCfg);
		}
		Body = AISerializeObject(Root);
	}
	else if (bIsAnthropic)
	{
		Url = TEXT("https://api.anthropic.com/v1/messages");
		Request->SetHeader(TEXT("x-api-key"), ApiKey);
		Request->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("model"), Model);
		Root->SetNumberField(TEXT("max_tokens"), 4096);
		Root->SetStringField(TEXT("system"), SystemPrompt);
		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(MsgObj(TEXT("user"), UserPrompt)));
		Root->SetArrayField(TEXT("messages"), Messages);
		Body = AISerializeObject(Root);
	}
	else // OpenAI / DeepSeek / Custom (OpenAI-compatible)
	{
		if (bIsCustom)
		{
			Url = BuildCustomUrl(Slot.CustomBaseURL);
			if (Slot.CustomBaseURL.Contains(TEXT("anthropic.com")))
			{
				Request->SetHeader(TEXT("x-api-key"), ApiKey);
				Request->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
			}
			else
			{
				Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
			}
		}
		else if (bIsDeepSeek)
		{
			Url = TEXT("https://api.deepseek.com/v1/chat/completions");
			Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
		}
		else // OpenAI default
		{
			Url = TEXT("https://api.openai.com/v1/chat/completions");
			Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
		}

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("model"), Model);
		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(MsgObj(TEXT("system"), SystemPrompt)));
		Messages.Add(MakeShared<FJsonValueObject>(MsgObj(TEXT("user"), UserPrompt)));
		Root->SetArrayField(TEXT("messages"), Messages);
		// Strict JSON for the hosted providers (skip for Custom — many local servers reject it).
		if (bIsOpenAI || bIsDeepSeek)
		{
			TSharedPtr<FJsonObject> Fmt = MakeShared<FJsonObject>();
			Fmt->SetStringField(TEXT("type"), TEXT("json_object"));
			Root->SetObjectField(TEXT("response_format"), Fmt);
		}
		Body = AISerializeObject(Root);
	}

	Request->SetURL(Url);
	Request->SetContentAsString(Body);

	Request->OnProcessRequestComplete().BindLambda(
		[OnDone](FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (!bWasSuccessful || !Response.IsValid())
			{
				OnDone(false, FString(), TEXT("Network error — could not reach the AI provider. Check your connection (or that your local server is running)."));
				return;
			}

			const int32 Code = Response->GetResponseCode();
			const FString Content = Response->GetContentAsString();

			TSharedPtr<FJsonObject> Root;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
			FJsonSerializer::Deserialize(Reader, Root);

			if (Code < 200 || Code >= 300)
			{
				const FString Msg = ExtractError(Content);
				OnDone(false, FString(), FString::Printf(TEXT("Provider error (HTTP %d). %s"), Code, Msg.IsEmpty() ? TEXT("") : *Msg));
				return;
			}

			const FString Reply = ExtractReply(Root);
			if (Reply.IsEmpty())
			{
				OnDone(false, FString(), TEXT("The model returned an empty response."));
				return;
			}
			OnDone(true, Reply, FString());
		});

	Request->ProcessRequest();
}
