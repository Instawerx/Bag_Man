// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Match/AFLMatchResultTypes.h"

#include "AFLLobbyTypes.generated.h"

/**
 * ══ THE LOBBY VOCABULARY ══════════════════════════════════════════════════════════════════════════════
 *
 * The queue dimensions are a 1:1 mirror of `Bag_Man_Backend/lambda/queue-registry/registry.ts`. The server
 * owns them -- `matchmaking.md` §4.2 makes the queue count a function of the dimensions ONLY, and §4.3
 * protects that with "content growth must never increase the queue count". A client that invented its own
 * parallel vocabulary would be a second place the dimensions live, which is the drift mechanism.
 *
 * ⚠ THREE OF THEM ARE NOT DECLARED HERE, ON PURPOSE. `EAFLPlayTier` (R85), `EAFLLeague` (R86) and
 * `EAFLRuleset` (R41) already live in `AFLGameCore/Public/Match/AFLMatchResultTypes.h`, the ALWAYS-LOADED
 * module, because the match result carries them and its consumers outlive the GameFeature that produced
 * it. Redeclaring them for the lobby would be the same value with two homes -- and UHT catches that as a
 * name collision, which is how this was found rather than shipped.
 *
 * ⚠ NOTHING HERE IS AUTHORED AS A LIST FOR THE UI TO OFFER. These types name what a queue CAN be; which
 * queues EXIST comes from `GET /queues`, and how busy they are comes from `GET /population`. The handoff's
 * §3.2.1 rule is that "options are read off the live queues, never authored as a list" -- an axis that
 * offers a value with no queue behind it is R18's broken promise.
 */

/**
 * The staked denomination -- `STAKED_DOOR_SPEC.md` §1. SEALED POOLS, NO CONVERSION (R81).
 *
 * ⚠ NOT `EAFLPayCurrency`, and they must not be merged. That enum is a STORE concept and carries an `Auto`
 * value meaning "let the server pick the cheaper price" -- which is exactly the conversion R81 forbids on
 * a stake. A wager has to name its pool; there is no auto.
 *
 * ⚠ SWITCHING DENOMINATION RE-BASES EVERYTHING -- ladder, suffix, balance reference, band -- and **the
 * entered amount does NOT carry across** (§2). 1,000 W and 1,000 V are not the same bet, and carrying the
 * number would imply they are.
 *
 * There is deliberately no `None`: this enum only ever applies behind the staked door. The league route has
 * no denomination because it has no buy-in, and modelling that as a third value would put a currency
 * concept on the free side of R98.
 */
UENUM(BlueprintType)
enum class EAFLDenomination : uint8
{
	Watts,
	Volts
};

/**
 * R97. Two STYLES of play, not two scenery sets -- which is the only reason a terrain-shaped word appears
 * on a list of contest properties.
 *
 * ⚠ R18 IS INTACT. The MAP is still a server outcome WITHIN the chosen class. The player says "an Arena
 * fight"; the server still says which Arena. This axis must never grow into a venue picker.
 */
UENUM(BlueprintType)
enum class EAFLVenueClass : uint8
{
	Arena,
	Map
};

/**
 * THE SIX READINGS -- `IRONICS_LEAGUE_DOOR_SPEC.md` §3.2, and they must never collapse into each other.
 *
 * Five come from the server (`lambda/population/classify.ts`); the sixth is a registry fact, not a
 * population reading, and is synthesized here. Each is a fact about a DIFFERENT thing:
 *
 *     NotOpen   about the CONTENT.  No map backs this cell (R63), so it is not a queue yet.
 *     Cold      about the QUEUE'S POPULATION. It exists, anyone may enter, nobody has.
 *     Stalled   about the QUEUE'S BEHAVIOUR. People are in it and it has produced nothing.
 *     Unknown   about US. The queue exists and we failed to read it.
 *
 * ⚠ THE CLIENT NEVER DERIVES THIS. §3.1: "the server classifies; the door renders." A threshold two
 * surfaces disagree about is a threshold that means nothing, and a door that computed its own `Cold` would
 * eventually disagree with the queue it is describing. The only value this file produces locally is
 * `NotOpen`, and only because /population deliberately omits unpublished cells entirely.
 */
UENUM(BlueprintType)
enum class EAFLPopulationState : uint8
{
	/** Measured median wait <= 60s. Matches forming inside a minute. Filled dot, pulsing. */
	Live,
	/** Measured median wait > 60s. Matches forming, but you will wait. Filled dot, no pulse. */
	Warm,
	/** People here and nothing matched recently. Hollow DOUBLE ring at FULL opacity -- occupied but unproven. */
	Stalled,
	/** Nobody is here. Hollow ring, 62% opacity. **Still selectable** -- cold is a variant, not a disabled state. */
	Cold,
	/** Published, but we could not read the count. Dashed ring, `Count unavailable`. NEVER rendered as 0. */
	Unknown,
	/** No map backs this cell (R63). Drawn disabled at 42% rather than hidden -- the ladder is information. */
	NotOpen
};

/**
 * One stake band's population -- `STAKED_DOOR_SPEC.md` §5.
 *
 * EVERY BAND CARRIES ITS OWN MEASURED WAIT. The allocator banks a separate sample ring per rung precisely so
 * a band never inherits the cell's speed: without it a busy 250 rung lends its number to a dead 25,000,
 * which is the exact laundering §5.2 names, performed by the field meant to prevent it.
 *
 * ⚠ LEAGUE PLAY CELLS CARRY AN EMPTY BAND ARRAY, and that is NOT the same as bands that are empty. The two
 * must not render alike (§5.1).
 */
USTRUCT(BlueprintType)
struct FAFLLobbyBand
{
	GENERATED_BODY()

	/** The rung this band centres on. Snapping is the SERVER's (R59); this is what it snapped to. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	int32 Centre = 0;

	/**
	 * Players standing in this band, or INDEX_NONE when the count could not be read.
	 *
	 * ⚠ INDEX_NONE RATHER THAN 0, DELIBERATELY. §3.2: "we could not find out" and "nobody is there" are
	 * OPPOSITE claims, and a sentinel of 0 invites a widget to render the second when it means the first.
	 * A negative value cannot be mistaken for a population.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	int32 Waiting = INDEX_NONE;

	/** Measured median wait IN THIS BAND, or INDEX_NONE when nothing matched recently. Never optimistic. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	int32 EstimatedWaitSeconds = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	EAFLPopulationState State = EAFLPopulationState::Unknown;

	bool HasCount() const { return Waiting >= 0; }
	bool HasEstimate() const { return EstimatedWaitSeconds >= 0; }
};

/**
 * One concrete queue cell -- the thing a ROW is (R18), joined from `GET /queues` and `GET /population`.
 *
 * The two reads are joined on `QueueId` because they answer different questions: /queues says which cells
 * EXIST and which are published; /population reports only the published ones. A published cell missing from
 * the population response is `Unknown`; an UNPUBLISHED cell is `NotOpen` and never had a reading to miss.
 */
USTRUCT(BlueprintType)
struct FAFLLobbyQueue
{
	GENERATED_BODY()

	/** `Tier_League_Ruleset_Venue_Bracket`. Used VERBATIM as the PlayFab queue name -- underscores, no dots. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	FString QueueId;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	EAFLPlayTier Tier = EAFLPlayTier::LeaguePlay;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	EAFLLeague League = EAFLLeague::Haywire;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	EAFLRuleset Ruleset = EAFLRuleset::MatchPlay;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	EAFLVenueClass Venue = EAFLVenueClass::Arena;

	/** `1v1` … `8v8`, or `BR_9` / `BR_20` / `BR_36` (R99). The label the size/field tile carries. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	FString Bracket;

	/** Finishing positions -- 2 for a team series however many players, N for a BR field. Drives payouts. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	int32 Positions = 2;

	/** Human slots one match consumes. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	int32 Slots = 2;

	/** R63. False means no map backs this cell; it renders `Not open yet`, disabled, and is NOT hidden. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	bool bPublished = false;

	/** Players queued here, or INDEX_NONE for unknown. See FAFLLobbyBand::Waiting for why not 0. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	int32 PlayersMatching = INDEX_NONE;

	/** Measured median ticket-to-match time, or INDEX_NONE. Suppressed by the server on a cold cell. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	int32 EstimatedWaitSeconds = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	EAFLPopulationState State = EAFLPopulationState::NotOpen;

	/** Per stake band. EMPTY on a LEAGUE PLAY cell, which has no bands to report -- see FAFLLobbyBand. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	TArray<FAFLLobbyBand> Bands;

	/** The preset rungs this tier publishes. Empty for LEAGUE PLAY: there is no stake to pick. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Lobby")
	TArray<int32> StakeRungs;

	bool HasCount() const { return PlayersMatching >= 0; }
	bool HasEstimate() const { return EstimatedWaitSeconds >= 0; }

	/** Selectable covers every state except NotOpen. Cold is a VARIANT, not a disabled state (handoff §9). */
	bool IsSelectable() const { return bPublished && State != EAFLPopulationState::NotOpen; }
};

/**
 * FAFLLobbyQueueId -- parse and compose the server's queue id.
 *
 * ⚠ THE BRACKET CONTAINS UNDERSCORES. `BR_9`, `BR_20` and `BR_36` are real bracket ids, so a naive
 * `ParseIntoArray("_")` produces six tokens for a BR cell and five for a Match Play one. The parse is
 * therefore LEFT-ANCHORED: the first four fields are fixed, and everything after the fourth separator is
 * the bracket. Splitting from the right would work today and break the first time a tier is renamed.
 */
struct AFLCOMBAT_API FAFLLobbyQueueId
{
	static FString Compose(EAFLPlayTier Tier, EAFLLeague League, EAFLRuleset Ruleset,
	                       EAFLVenueClass Venue, const FString& Bracket);

	/** False when the id does not carry five fields or names a value outside the vocabulary above. */
	static bool Parse(const FString& QueueId, EAFLPlayTier& OutTier, EAFLLeague& OutLeague,
	                  EAFLRuleset& OutRuleset, EAFLVenueClass& OutVenue, FString& OutBracket);

	// Wire spellings, matching registry.ts exactly. Kept here so exactly one file knows them.
	static const TCHAR* ToWire(EAFLPlayTier Tier);
	static const TCHAR* ToWire(EAFLLeague League);
	static const TCHAR* ToWire(EAFLRuleset Ruleset);
	static const TCHAR* ToWire(EAFLVenueClass Venue);

	static bool FromWire(const FString& In, EAFLPlayTier& Out);
	static bool FromWire(const FString& In, EAFLLeague& Out);
	static bool FromWire(const FString& In, EAFLRuleset& Out);
	static bool FromWire(const FString& In, EAFLVenueClass& Out);

	/** `live` / `warm` / `stalled` / `cold` / `unknown`. Anything unrecognised is Unknown, never Cold. */
	static EAFLPopulationState PopulationStateFromWire(const FString& In);
};

/** Derived, never configured separately -- the same derivations registry.ts makes, so they cannot disagree. */
namespace AFLLobby
{
	inline bool IsStaked(EAFLPlayTier Tier) { return Tier != EAFLPlayTier::LeaguePlay; }
	/** R85/R87: the staked tiers are rated; LEAGUE PLAY is not. */
	inline bool IsRated(EAFLPlayTier Tier) { return IsStaked(Tier); }
	/** `ai-bots` §6.3 + R87: bots only where the outcome moves neither a balance nor a rating. */
	inline bool BotsPermitted(EAFLPlayTier Tier) { return !IsStaked(Tier); }
}
