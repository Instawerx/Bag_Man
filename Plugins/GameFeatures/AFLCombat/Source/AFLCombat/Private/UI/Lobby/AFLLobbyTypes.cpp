// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/Lobby/AFLLobbyTypes.h"

FString FAFLLobbyQueueId::Compose(EAFLPlayTier Tier, EAFLLeague League, EAFLRuleset Ruleset,
                                  EAFLVenueClass Venue, const FString& Bracket)
{
	// Mirrors queueIdFor() in registry.ts. UNDERSCORES, NOT DOTS: this id is the PlayFab matchmaking queue
	// name verbatim, and PlayFab rejects a dotted name with {"Name":["InvalidCharacters"]}.
	return FString::Printf(TEXT("%s_%s_%s_%s_%s"),
		ToWire(Tier), ToWire(League), ToWire(Ruleset), ToWire(Venue), *Bracket);
}

bool FAFLLobbyQueueId::Parse(const FString& QueueId, EAFLPlayTier& OutTier, EAFLLeague& OutLeague,
                             EAFLRuleset& OutRuleset, EAFLVenueClass& OutVenue, FString& OutBracket)
{
	// LEFT-ANCHORED, four separators, remainder is the bracket. `BR_9` / `BR_20` / `BR_36` carry their own
	// underscore, so a plain token split yields six fields for a BR cell and five for a Match Play one --
	// and code that indexes [4] would silently read `BR` as the whole bracket on exactly the queues R99
	// added. Walking the first four separators is length-independent.
	FString Rest = QueueId;
	FString Fields[4];
	for (int32 Index = 0; Index < 4; ++Index)
	{
		int32 Sep = INDEX_NONE;
		if (!Rest.FindChar(TEXT('_'), Sep))
		{
			return false;
		}
		Fields[Index] = Rest.Left(Sep);
		Rest = Rest.Mid(Sep + 1);
	}
	if (Rest.IsEmpty())
	{
		return false;   // trailing separator, no bracket
	}

	if (!FromWire(Fields[0], OutTier)) { return false; }
	if (!FromWire(Fields[1], OutLeague)) { return false; }
	if (!FromWire(Fields[2], OutRuleset)) { return false; }
	if (!FromWire(Fields[3], OutVenue)) { return false; }
	OutBracket = Rest;
	return true;
}

const TCHAR* FAFLLobbyQueueId::ToWire(EAFLPlayTier Tier)
{
	switch (Tier)
	{
	case EAFLPlayTier::WattsPlay: return TEXT("WattsPlay");
	case EAFLPlayTier::VoltsPlay: return TEXT("VoltsPlay");
	default:                      return TEXT("LeaguePlay");
	}
}

const TCHAR* FAFLLobbyQueueId::ToWire(EAFLLeague League)
{
	return League == EAFLLeague::ProMod ? TEXT("ProMod") : TEXT("Haywire");
}

const TCHAR* FAFLLobbyQueueId::ToWire(EAFLRuleset Ruleset)
{
	return Ruleset == EAFLRuleset::BattleRoyale ? TEXT("BattleRoyale") : TEXT("MatchPlay");
}

const TCHAR* FAFLLobbyQueueId::ToWire(EAFLVenueClass Venue)
{
	return Venue == EAFLVenueClass::Map ? TEXT("Map") : TEXT("Arena");
}

bool FAFLLobbyQueueId::FromWire(const FString& In, EAFLPlayTier& Out)
{
	if (In == TEXT("LeaguePlay")) { Out = EAFLPlayTier::LeaguePlay; return true; }
	if (In == TEXT("WattsPlay"))  { Out = EAFLPlayTier::WattsPlay;  return true; }
	if (In == TEXT("VoltsPlay"))  { Out = EAFLPlayTier::VoltsPlay;  return true; }
	return false;
}

bool FAFLLobbyQueueId::FromWire(const FString& In, EAFLLeague& Out)
{
	if (In == TEXT("Haywire")) { Out = EAFLLeague::Haywire; return true; }
	if (In == TEXT("ProMod"))  { Out = EAFLLeague::ProMod;  return true; }
	return false;
}

bool FAFLLobbyQueueId::FromWire(const FString& In, EAFLRuleset& Out)
{
	if (In == TEXT("MatchPlay"))    { Out = EAFLRuleset::MatchPlay;    return true; }
	if (In == TEXT("BattleRoyale")) { Out = EAFLRuleset::BattleRoyale; return true; }
	return false;
}

bool FAFLLobbyQueueId::FromWire(const FString& In, EAFLVenueClass& Out)
{
	if (In == TEXT("Arena")) { Out = EAFLVenueClass::Arena; return true; }
	if (In == TEXT("Map"))   { Out = EAFLVenueClass::Map;   return true; }
	return false;
}

EAFLPopulationState FAFLLobbyQueueId::PopulationStateFromWire(const FString& In)
{
	if (In == TEXT("live"))    { return EAFLPopulationState::Live;    }
	if (In == TEXT("warm"))    { return EAFLPopulationState::Warm;    }
	if (In == TEXT("stalled")) { return EAFLPopulationState::Stalled; }
	if (In == TEXT("cold"))    { return EAFLPopulationState::Cold;    }

	// ⚠ AN UNRECOGNISED STATE FALLS TO Unknown, NEVER Cold. They are opposite claims (§3.2): Unknown says
	// we could not find out, Cold says nobody is there. A parser that guessed Cold on a server field it did
	// not recognise would render an empty queue as a fact -- the one thing §5 exists to prevent.
	return EAFLPopulationState::Unknown;
}
