// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Match/AFLMatchResultTypes.h"   // EAFLRuleset
#include "Subsystems/GameInstanceSubsystem.h"

#include "AFLCareerSubsystem.generated.h"

/**
 * One OpenSkill ladder. R64: TWO of these and only two, keyed by RULESET.
 *
 * ⚠ NOT PER CURRENCY. R82: "a player has two ratings, not four" -- WattsPlay and VoltsPlay differ in what
 * a match COSTS and PAYS, never in what it COUNTS TOWARD, so currency does not appear here at all.
 */
USTRUCT(BlueprintType)
struct AFLGAMECORE_API FAFLCareerLadder
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Career")
	EAFLRuleset Ruleset = EAFLRuleset::MatchPlay;

	/**
	 * The displayed rating, or INDEX_NONE when this ladder has never been played.
	 *
	 * ⚠ INDEX_NONE, NEVER 0 -- the same discipline the lobby's population counts use. "Has not played this
	 * ruleset" and "is rated zero" are opposite claims about a player, and the second one is an insult if
	 * it is really the first. The server sends `null` for exactly this reason; collapsing it here would
	 * throw away the distinction it went to the trouble of preserving.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Career")
	int32 Rating = INDEX_NONE;

	/** Matches on THIS ladder. ⚠ NOT career volume -- R10's volume is eliminations, which nothing counts. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Career")
	int32 MatchCount = 0;

	/** True once the ladder has a real rating. The predicate the UI branches on, not `Rating > 0`. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Career")
	bool bPlaced = false;
};

/** Everything the RANK tab draws. */
USTRUCT(BlueprintType)
struct AFLGAMECORE_API FAFLCareer
{
	GENERATED_BODY()

	/** Always both rulesets, played or not -- an untouched ladder is part of a career, not an omission. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Career")
	TArray<FAFLCareerLadder> Ladders;

	/**
	 * R10's cumulative-eliminations axis. FALSE as of 2026-08-10 and the server says so explicitly:
	 * nothing in the backend counts eliminations, so there is no volume to show. Carried as a flag rather
	 * than inferred from an absent field, so the VOLUME tab can be disabled for the real reason.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Career")
	bool bVolumeAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Career")
	FString VolumeUnavailableReason;

	/** True once a real response populated this. Absent data must never render as an unranked player. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Career")
	bool bKnown = false;

	const FAFLCareerLadder* Find(EAFLRuleset InRuleset) const
	{
		return Ladders.FindByPredicate([InRuleset](const FAFLCareerLadder& L) { return L.Ruleset == InRuleset; });
	}
};

DECLARE_DELEGATE_TwoParams(FAFLOnCareer, bool /*bSuccess*/, const FAFLCareer& /*Career*/);

/**
 * UAFLCareerSubsystem -- GET /career, for the Career hub's RANK tab.
 *
 * Its own subsystem for the reason `UAFLPlayLimitsSubsystem` is: the queue directory publishes properties
 * of the SYSTEM, served unauthenticated and identical for everyone, while this is a property of a PERSON.
 * Not cached -- a rating moves when a match settles, and a stale one shown next to the word RANK is worse
 * than a slow one.
 */
UCLASS()
class AFLGAMECORE_API UAFLCareerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UAFLCareerSubsystem* Get(const UObject* WorldContext);

	/** Ask the server. Fires exactly once. ON FAILURE IT REPORTS FAILURE -- never an unranked player. */
	void FetchCareer(FAFLOnCareer OnDone);

	/** Parse a ruleset id as the backend spells it. False for anything else, without guessing. */
	static bool TryParseRuleset(const FString& Id, EAFLRuleset& OutRuleset);
};
