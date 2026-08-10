// Copyright C12 AI Gaming. All Rights Reserved.

#include "Online/AFLPlayLimitsSubsystem.h"

#include "AFLGameCore.h"            // LogAFLGameCore
#include "AFLOnlineSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLPlayLimitsSubsystem)

namespace
{
	/**
	 * `afl.Lobby.Limits` -- read the guardrails without opening S4.
	 *
	 * Same reason the other probes exist: a headless session has no mouse, and the alternative is shipping
	 * on the strength of the code reading correctly. It also answers the question a support conversation
	 * actually asks -- "why was I refused?" -- without needing the player to reproduce it.
	 */
	FAutoConsoleCommandWithWorldAndArgs GAFLLobbyLimitsCmd(
		TEXT("afl.Lobby.Limits"),
		TEXT("Fetch this player's play limits (GET /limits) and log them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* World)
			{
				UAFLPlayLimitsSubsystem* Limits = UAFLPlayLimitsSubsystem::Get(World);
				if (!Limits)
				{
					UE_LOG(LogAFLGameCore, Error, TEXT("AFL_LIMITS: no subsystem -- not logged in?"));
					return;
				}
				// Logs rather than writing to an FOutputDevice: the device is long gone by the time an HTTP
				// round trip returns, and holding a reference to it is a dangling pointer waiting to happen.
				Limits->FetchLimits(FAFLOnPlayLimits::CreateLambda(
					[](bool bOk, const FAFLPlayLimits& L)
					{
						if (!bOk)
						{
							UE_LOG(LogAFLGameCore, Error, TEXT("AFL_LIMITS: fetch FAILED."));
							return;
						}
						UE_LOG(LogAFLGameCore, Log,
							TEXT("AFL_LIMITS: window=%dh cap=%d%% provisional=%s"),
							L.WindowHours, L.EntryCapPercentOfBalance, L.bProvisional ? TEXT("yes") : TEXT("no"));
						UE_LOG(LogAFLGameCore, Log,
							TEXT("AFL_LIMITS:   VO balance=%lld entryCap=%lld staked=%lld/%lld loss=%lld"),
							L.Volts.Balance, L.Volts.EntryCap, L.Volts.WindowStaked, L.Volts.WindowCeiling,
							L.Volts.WindowLoss);
						UE_LOG(LogAFLGameCore, Log,
							TEXT("AFL_LIMITS:   WA balance=%lld entryCap=%lld staked=%lld/%lld loss=%lld"),
							L.Watts.Balance, L.Watts.EntryCap, L.Watts.WindowStaked, L.Watts.WindowCeiling,
							L.Watts.WindowLoss);
					}));
			}));
}

UAFLPlayLimitsSubsystem* UAFLPlayLimitsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAFLPlayLimitsSubsystem>() : nullptr;
}

FAFLPlayLimit UAFLPlayLimitsSubsystem::ParseCurrency(const TSharedPtr<FJsonObject>& Obj)
{
	FAFLPlayLimit Out;
	if (!Obj.IsValid())
	{
		return Out;   // bKnown stays false: absent is not zero
	}
	Out.Balance         = static_cast<int64>(Obj->GetNumberField(TEXT("balance")));
	Out.EntryCap        = static_cast<int64>(Obj->GetNumberField(TEXT("entryCap")));
	Out.WindowStaked    = static_cast<int64>(Obj->GetNumberField(TEXT("windowStaked")));
	Out.WindowCeiling   = static_cast<int64>(Obj->GetNumberField(TEXT("windowCeiling")));
	Out.WindowRemaining = static_cast<int64>(Obj->GetNumberField(TEXT("windowRemaining")));
	Out.WindowLoss      = static_cast<int64>(Obj->GetNumberField(TEXT("windowLoss")));
	Out.bKnown = true;
	return Out;
}

void UAFLPlayLimitsSubsystem::FetchLimits(FAFLOnPlayLimits OnDone)
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	const FString BaseUrl = Online ? Online->PlayerApiBaseUrl() : FString();
	const FString SessionTicket = Online ? Online->GetSessionTicket() : FString();

	if (BaseUrl.IsEmpty() || SessionTicket.IsEmpty())
	{
		// FAIL, do not substitute. The caller is about to draw a money surface; "we could not ask" and
		// "there are no limits" are opposite claims and must not share a representation.
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_LIMITS: no API base or no session ticket -- cannot read play limits."));
		OnDone.ExecuteIfBound(false, FAFLPlayLimits());
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl + TEXT("/limits"));
	Request->SetVerb(TEXT("GET"));
	// Identity comes from the ticket alone. The request names nobody, so one client cannot read another's
	// balance by asking nicely -- the same discipline /heartbeat uses.
	Request->SetHeader(TEXT("X-SessionTicket"), SessionTicket);

	Request->OnProcessRequestComplete().BindLambda(
		[OnDone](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
		{
			const int32 Code = Res.IsValid() ? Res->GetResponseCode() : 0;
			if (!bOk || !Res.IsValid() || Code < 200 || Code >= 300)
			{
				UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_LIMITS: GET /limits failed (http %d)."), Code);
				OnDone.ExecuteIfBound(false, FAFLPlayLimits());
				return;
			}

			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_LIMITS: /limits returned unparseable JSON."));
				OnDone.ExecuteIfBound(false, FAFLPlayLimits());
				return;
			}

			FAFLPlayLimits Limits;
			Limits.EntryCapPercentOfBalance = static_cast<int32>(Root->GetNumberField(TEXT("entryCapPercentOfBalance")));
			Limits.bProvisional = Root->GetBoolField(TEXT("provisional"));

			const TSharedPtr<FJsonObject>* Window = nullptr;
			if (Root->TryGetObjectField(TEXT("window"), Window) && Window)
			{
				Limits.WindowHours = static_cast<int32>((*Window)->GetNumberField(TEXT("hours")));
			}

			const TSharedPtr<FJsonObject>* Currencies = nullptr;
			if (Root->TryGetObjectField(TEXT("currencies"), Currencies) && Currencies)
			{
				const TSharedPtr<FJsonObject>* Vo = nullptr;
				const TSharedPtr<FJsonObject>* Wa = nullptr;
				if ((*Currencies)->TryGetObjectField(TEXT("VO"), Vo) && Vo) { Limits.Volts = ParseCurrency(*Vo); }
				if ((*Currencies)->TryGetObjectField(TEXT("WA"), Wa) && Wa) { Limits.Watts = ParseCurrency(*Wa); }
			}

			// A 200 that carried neither currency is a malformed answer, not an answer of zero.
			if (!Limits.Volts.bKnown && !Limits.Watts.bKnown)
			{
				UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_LIMITS: /limits carried no currency figures."));
				OnDone.ExecuteIfBound(false, FAFLPlayLimits());
				return;
			}

			OnDone.ExecuteIfBound(true, Limits);
		});

	Request->ProcessRequest();
}
