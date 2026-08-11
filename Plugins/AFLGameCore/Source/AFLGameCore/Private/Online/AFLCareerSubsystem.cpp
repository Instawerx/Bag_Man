// Copyright C12 AI Gaming. All Rights Reserved.

#include "Online/AFLCareerSubsystem.h"

#include "AFLGameCore.h"            // LogAFLGameCore
#include "AFLOnlineSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCareerSubsystem)

namespace
{
	/** `afl.Career.Rank` -- read the ladders without opening the hub. */
	FAutoConsoleCommandWithWorldAndArgs GAFLCareerRankCmd(
		TEXT("afl.Career.Rank"),
		TEXT("Fetch this player's ladders (GET /career) and log them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* World)
			{
				UAFLCareerSubsystem* Career = UAFLCareerSubsystem::Get(World);
				if (!Career)
				{
					UE_LOG(LogAFLGameCore, Error, TEXT("AFL_CAREER: no subsystem -- not logged in?"));
					return;
				}
				// Logs rather than writing to an FOutputDevice, which dangles after an HTTP round trip.
				Career->FetchCareer(FAFLOnCareer::CreateLambda(
					[](bool bOk, const FAFLCareer& C)
					{
						if (!bOk)
						{
							UE_LOG(LogAFLGameCore, Error, TEXT("AFL_CAREER: fetch FAILED."));
							return;
						}
						for (const FAFLCareerLadder& L : C.Ladders)
						{
							UE_LOG(LogAFLGameCore, Log, TEXT("AFL_CAREER:   %s rating=%s matches=%d placed=%s"),
								L.Ruleset == EAFLRuleset::BattleRoyale ? TEXT("BATTLE ROYALE") : TEXT("MATCH PLAY   "),
								L.Rating == INDEX_NONE ? TEXT("unplaced") : *FString::FromInt(L.Rating),
								L.MatchCount, L.bPlaced ? TEXT("yes") : TEXT("no"));
						}
						UE_LOG(LogAFLGameCore, Log, TEXT("AFL_CAREER:   volume available=%s (%s)"),
							C.bVolumeAvailable ? TEXT("yes") : TEXT("no"), *C.VolumeUnavailableReason);
					}));
			}));
}

UAFLCareerSubsystem* UAFLCareerSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAFLCareerSubsystem>() : nullptr;
}

bool UAFLCareerSubsystem::TryParseRuleset(const FString& Id, EAFLRuleset& OutRuleset)
{
	// Spelled exactly as the backend spells it. No fuzzy matching: an unrecognised ruleset is a contract
	// change between client and server, and guessing would turn that into a silently mis-labelled ladder --
	// a player's BATTLE ROYALE rating shown under MATCH PLAY.
	//
	// ⚠ CaseSensitive EXPLICITLY, because FString::operator== IS NOT. Written as `Id == TEXT("BattleRoyale")`
	// this accepted "battleroyale" and "BATTLEROYALE", which is exactly the guessing the comment above
	// forbids -- and worse, it would have swallowed a real casing change on the server silently instead of
	// reporting the contract break. Caught by the test that asserts the refusals.
	if (Id.Equals(TEXT("BattleRoyale"), ESearchCase::CaseSensitive)) { OutRuleset = EAFLRuleset::BattleRoyale; return true; }
	if (Id.Equals(TEXT("MatchPlay"),    ESearchCase::CaseSensitive)) { OutRuleset = EAFLRuleset::MatchPlay;    return true; }
	return false;
}

void UAFLCareerSubsystem::FetchCareer(FAFLOnCareer OnDone)
{
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	const FString BaseUrl = Online ? Online->PlayerApiBaseUrl() : FString();
	const FString SessionTicket = Online ? Online->GetSessionTicket() : FString();

	if (BaseUrl.IsEmpty() || SessionTicket.IsEmpty())
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_CAREER: no API base or session ticket -- cannot read career."));
		OnDone.ExecuteIfBound(false, FAFLCareer());
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl + TEXT("/career"));
	Request->SetVerb(TEXT("GET"));
	// Identity from the ticket alone -- the request names nobody, so one client cannot read another's rank.
	Request->SetHeader(TEXT("X-SessionTicket"), SessionTicket);

	Request->OnProcessRequestComplete().BindLambda(
		[OnDone](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
		{
			const int32 Code = Res.IsValid() ? Res->GetResponseCode() : 0;
			if (!bOk || !Res.IsValid() || Code < 200 || Code >= 300)
			{
				UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_CAREER: GET /career failed (http %d)."), Code);
				OnDone.ExecuteIfBound(false, FAFLCareer());
				return;
			}

			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_CAREER: /career returned unparseable JSON."));
				OnDone.ExecuteIfBound(false, FAFLCareer());
				return;
			}

			FAFLCareer Career;
			const TArray<TSharedPtr<FJsonValue>>* Ladders = nullptr;
			if (Root->TryGetArrayField(TEXT("ladders"), Ladders) && Ladders)
			{
				for (const TSharedPtr<FJsonValue>& Value : *Ladders)
				{
					const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
					if (!Obj.IsValid())
					{
						continue;
					}
					FAFLCareerLadder Ladder;
					if (!TryParseRuleset(Obj->GetStringField(TEXT("ruleset")), Ladder.Ruleset))
					{
						UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_CAREER: unknown ruleset '%s' -- skipped."),
							*Obj->GetStringField(TEXT("ruleset")));
						continue;
					}

					// ⚠ null RATING IS PRESERVED AS INDEX_NONE. GetNumberField would fold null to 0 and the
					// screen would tell an unplayed player they are rated zero.
					double Raw = 0.0;
					Ladder.Rating = Obj->TryGetNumberField(TEXT("rating"), Raw)
						? FMath::RoundToInt(Raw)
						: INDEX_NONE;

					Ladder.MatchCount = static_cast<int32>(Obj->GetNumberField(TEXT("matchCount")));
					Ladder.bPlaced    = Obj->GetBoolField(TEXT("placed"));
					Career.Ladders.Add(Ladder);
				}
			}

			const TSharedPtr<FJsonObject>* Volume = nullptr;
			if (Root->TryGetObjectField(TEXT("volume"), Volume) && Volume)
			{
				Career.bVolumeAvailable = (*Volume)->GetBoolField(TEXT("available"));
				(*Volume)->TryGetStringField(TEXT("reason"), Career.VolumeUnavailableReason);
			}

			// A 200 carrying no ladders is a malformed answer, not a player with no career -- R64 says
			// there are exactly two, always.
			if (Career.Ladders.Num() == 0)
			{
				UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_CAREER: /career carried no ladders."));
				OnDone.ExecuteIfBound(false, FAFLCareer());
				return;
			}

			Career.bKnown = true;
			OnDone.ExecuteIfBound(true, Career);
		});

	Request->ProcessRequest();
}
