// Copyright C12 AI Gaming. All Rights Reserved.
//
// AFL.Lobby — S1 LobbyRoot's product rules, proved without a world, a widget tree or PIE.
//
// The home-screen spec makes the argument these tests inherit: a widget graph can be rewired by anyone, so a
// rule that matters has to live somewhere a test can hold it. S1 carries three of those, and each one fails
// SILENTLY if it breaks — which is why they are checked in CI rather than in a play-through:
//
//   1. R98 — the league route never exposes a stake or a denomination axis. A regression here does not
//      crash; it puts a buy-in control in front of the majority of players.
//   2. R86 — the staked route never exposes a league axis. A regression offers a choice with no queue
//      behind it, which is R18's broken promise.
//   3. §3.2 — the four kinds of absence never collapse into each other. A regression here renders "we could
//      not find out" as "nobody is there", on the one surface whose whole job is honesty about population.
//
// Simple-automation macros rather than BEGIN_DEFINE_SPEC, matching AFLHomeScreenSpec and AFLZonePlanSpec —
// the Spec variant does not auto-register in this module.

#include "Misc/AutomationTest.h"
#include "Online/AFLLobbyTypes.h"
#include "Online/AFLQueueDirectorySubsystem.h"
#include "UI/Lobby/AFLW_Lobby_QueueRow.h"
#include "UI/Lobby/AFLW_Lobby_Root.h"

namespace
{
	FAFLLobbyQueue MakeQueue(EAFLPopulationState State, int32 Players, int32 WaitSeconds, bool bPublished = true)
	{
		FAFLLobbyQueue Queue;
		Queue.Bracket = TEXT("5v5");
		Queue.Slots = 10;
		Queue.bPublished = bPublished;
		Queue.State = State;
		Queue.PlayersMatching = Players;
		Queue.EstimatedWaitSeconds = WaitSeconds;
		return Queue;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLLobby_LeagueDoorHasNoBuyIn, "AFL.Lobby.R98_LeagueDoorHasNoBuyIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLLobby_LeagueDoorHasNoBuyIn::RunTest(const FString&)
{
	// `LEAGUE_DOOR_SPEC.md` §1: the stake axis is "— absent —. Not hidden, not zeroed, not disabled." Both
	// wagering axes are refused, because a denomination picker on the free side is the same category error
	// as a stake field — it asks a league player which currency they are risking.
	TestFalse(TEXT("league door refuses the STAKE axis"),
		UAFLW_Lobby_Root::IsAxisLegalForDoor(EAFLHomeDoor::League, EAFLLobbyAxis::Stake));
	TestFalse(TEXT("league door refuses the DENOMINATION axis"),
		UAFLW_Lobby_Root::IsAxisLegalForDoor(EAFLHomeDoor::League, EAFLLobbyAxis::Denomination));

	// The other half, and the one a too-permissive fix breaks the other way: the staked door MUST have both,
	// or staked play is unenterable and the first assertion still passes.
	TestTrue(TEXT("staked door offers the STAKE axis"),
		UAFLW_Lobby_Root::IsAxisLegalForDoor(EAFLHomeDoor::Staked, EAFLLobbyAxis::Stake));
	TestTrue(TEXT("staked door offers the DENOMINATION axis"),
		UAFLW_Lobby_Root::IsAxisLegalForDoor(EAFLHomeDoor::Staked, EAFLLobbyAxis::Denomination));

	// R86 — the mirror rule. Staked is PRO MOD ONLY, so the league axis is a stated fact there, not a
	// control; rendering it would offer a choice that does not exist.
	TestTrue(TEXT("league door offers the LEAGUE axis"),
		UAFLW_Lobby_Root::IsAxisLegalForDoor(EAFLHomeDoor::League, EAFLLobbyAxis::League));
	TestFalse(TEXT("staked door refuses the LEAGUE axis"),
		UAFLW_Lobby_Root::IsAxisLegalForDoor(EAFLHomeDoor::Staked, EAFLLobbyAxis::League));

	// Shared axes stay shared. R100 rules the chrome identical, so the doors differ by CONTENT only — if
	// these ever diverge, the split has started colour-separating by another route.
	for (const EAFLLobbyAxis Axis : { EAFLLobbyAxis::Ruleset, EAFLLobbyAxis::VenueClass, EAFLLobbyAxis::Size })
	{
		TestTrue(TEXT("shared axis is on the league door"),
			UAFLW_Lobby_Root::IsAxisLegalForDoor(EAFLHomeDoor::League, Axis));
		TestTrue(TEXT("shared axis is on the staked door"),
			UAFLW_Lobby_Root::IsAxisLegalForDoor(EAFLHomeDoor::Staked, Axis));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLLobby_TierResolvesFromDoor, "AFL.Lobby.R85_TierResolvesFromDoorAndDenomination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLLobby_TierResolvesFromDoor::RunTest(const FString&)
{
	// The league door resolves to LeaguePlay REGARDLESS of the denomination field's value. That is the point
	// of testing both: a stale denomination left over from a visit to the staked door must not be able to
	// turn the free route into a staked one.
	TestTrue(TEXT("league + Watts is LeaguePlay"),
		UAFLW_Lobby_Root::ResolveTier(EAFLHomeDoor::League, EAFLDenomination::Watts) == EAFLPlayTier::LeaguePlay);
	TestTrue(TEXT("league + Volts is still LeaguePlay"),
		UAFLW_Lobby_Root::ResolveTier(EAFLHomeDoor::League, EAFLDenomination::Volts) == EAFLPlayTier::LeaguePlay);

	TestTrue(TEXT("staked + Watts is WattsPlay"),
		UAFLW_Lobby_Root::ResolveTier(EAFLHomeDoor::Staked, EAFLDenomination::Watts) == EAFLPlayTier::WattsPlay);
	TestTrue(TEXT("staked + Volts is VoltsPlay"),
		UAFLW_Lobby_Root::ResolveTier(EAFLHomeDoor::Staked, EAFLDenomination::Volts) == EAFLPlayTier::VoltsPlay);

	// The derivations registry.ts makes, made here too so they cannot disagree.
	TestFalse(TEXT("LeaguePlay is unstaked"), AFLLobby::IsStaked(EAFLPlayTier::LeaguePlay));
	TestTrue(TEXT("LeaguePlay permits bots (R87)"), AFLLobby::BotsPermitted(EAFLPlayTier::LeaguePlay));
	TestFalse(TEXT("WattsPlay bars bots (ai-bots §6.3)"), AFLLobby::BotsPermitted(EAFLPlayTier::WattsPlay));
	TestFalse(TEXT("VoltsPlay bars bots (ai-bots §6.3)"), AFLLobby::BotsPermitted(EAFLPlayTier::VoltsPlay));
	TestTrue(TEXT("staked is rated (R85)"), AFLLobby::IsRated(EAFLPlayTier::VoltsPlay));
	TestFalse(TEXT("league is unrated (R85)"), AFLLobby::IsRated(EAFLPlayTier::LeaguePlay));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLLobby_QueueIdSurvivesBRBrackets, "AFL.Lobby.R99_QueueIdParsesBracketsWithUnderscores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLLobby_QueueIdSurvivesBRBrackets::RunTest(const FString&)
{
	// ⚠ THE TRAP THIS TEST EXISTS FOR. R99's bracket ids carry their OWN underscore — `BR_9`, `BR_20`,
	// `BR_36` — so a naive token split yields six fields for a BR cell and five for a Match Play one, and
	// code indexing [4] reads the bracket as bare `BR`. It would work on every Match Play queue and fail on
	// exactly the queues Battle Royale added, which is the worst possible distribution for finding it.
	const TCHAR* Brackets[] = { TEXT("1v1"), TEXT("5v5"), TEXT("8v8"), TEXT("BR_9"), TEXT("BR_20"), TEXT("BR_36") };
	for (const TCHAR* Bracket : Brackets)
	{
		const bool bIsBR = FString(Bracket).StartsWith(TEXT("BR_"));
		const EAFLRuleset Ruleset = bIsBR ? EAFLRuleset::BattleRoyale : EAFLRuleset::MatchPlay;

		const FString Id = FAFLLobbyQueueId::Compose(
			EAFLPlayTier::VoltsPlay, EAFLLeague::ProMod, Ruleset, EAFLVenueClass::Arena, Bracket);

		EAFLPlayTier Tier{}; EAFLLeague League{}; EAFLRuleset OutRuleset{}; EAFLVenueClass Venue{};
		FString OutBracket;
		if (!TestTrue(FString::Printf(TEXT("parses %s"), *Id),
			FAFLLobbyQueueId::Parse(Id, Tier, League, OutRuleset, Venue, OutBracket)))
		{
			continue;
		}

		TestEqual(TEXT("bracket round-trips intact"), OutBracket, FString(Bracket));
		TestTrue(TEXT("tier round-trips"), Tier == EAFLPlayTier::VoltsPlay);
		TestTrue(TEXT("league round-trips"), League == EAFLLeague::ProMod);
		TestTrue(TEXT("ruleset round-trips"), OutRuleset == Ruleset);
		TestTrue(TEXT("venue round-trips"), Venue == EAFLVenueClass::Arena);
	}

	// Malformed input is refused rather than half-parsed. A queue id we do not understand must not resolve
	// to a plausible-looking cell — the id is used verbatim as the PlayFab queue name.
	EAFLPlayTier Tier{}; EAFLLeague League{}; EAFLRuleset Ruleset{}; EAFLVenueClass Venue{}; FString Bracket;
	TestFalse(TEXT("too few fields is refused"),
		FAFLLobbyQueueId::Parse(TEXT("VoltsPlay_ProMod_MatchPlay"), Tier, League, Ruleset, Venue, Bracket));
	TestFalse(TEXT("a trailing separator with no bracket is refused"),
		FAFLLobbyQueueId::Parse(TEXT("VoltsPlay_ProMod_MatchPlay_Arena_"), Tier, League, Ruleset, Venue, Bracket));
	TestFalse(TEXT("an unknown tier is refused, not defaulted"),
		FAFLLobbyQueueId::Parse(TEXT("BitcoinPlay_ProMod_MatchPlay_Arena_5v5"), Tier, League, Ruleset, Venue, Bracket));
	// ⚠ A STAKED-HAYWIRE ID PARSES, AND THAT IS CORRECT. The parser is a GRAMMAR check, not a registry
	// check: R86 is enforced by TIER_LEAGUE_PAIRS on the server, where no such cell can be composed at all.
	// Teaching the id parser to reject the combination would put the rule in a second place, and the second
	// place is always the one that goes stale.
	TestTrue(TEXT("an illegal COMBINATION still parses -- R86 is the registry's job, not the grammar's"),
		FAFLLobbyQueueId::Parse(TEXT("VoltsPlay_Haywire_MatchPlay_Arena_5v5"), Tier, League, Ruleset, Venue, Bracket));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLLobby_AbsencesNeverCollapse, "AFL.Lobby.Pop_FourAbsencesNeverCollapse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLLobby_AbsencesNeverCollapse::RunTest(const FString&)
{
	// §3.2's table, as assertions. Each of the four absences is a fact about a DIFFERENT thing, and the
	// failure mode is not a crash — it is a queue that quietly claims to be empty when we simply could not
	// read it, on the surface whose entire job is honesty about population.

	// UNKNOWN — about US. Must never render as a number, and never as `0`.
	{
		const FAFLLobbyQueue Queue = MakeQueue(EAFLPopulationState::Unknown, INDEX_NONE, INDEX_NONE);
		const FString Pop = UAFLW_Lobby_QueueRow::FormatPopulation(Queue).ToString();
		TestFalse(TEXT("an unknown count never renders as a number"), Pop.IsNumeric());
		TestNotEqual(TEXT("an unknown count is never `0`"), Pop, FString(TEXT("0")));
		TestFalse(TEXT("an unknown count says something"), Pop.IsEmpty());
	}

	// COLD — about the queue's POPULATION. Nobody here, and it must stay SELECTABLE.
	{
		const FAFLLobbyQueue Queue = MakeQueue(EAFLPopulationState::Cold, 0, INDEX_NONE);
		TestTrue(TEXT("a cold queue is still selectable"), Queue.IsSelectable());
		TestFalse(TEXT("a cold queue does not render a bare 0"),
			UAFLW_Lobby_QueueRow::FormatPopulation(Queue).ToString().IsNumeric());
	}

	// STALLED — about the queue's BEHAVIOUR. People here, nothing matched. The count IS shown, because there
	// genuinely are people in it; what is absent is the evidence that it works.
	{
		const FAFLLobbyQueue Queue = MakeQueue(EAFLPopulationState::Stalled, 9, INDEX_NONE);
		TestEqual(TEXT("a stalled queue shows its real count"),
			UAFLW_Lobby_QueueRow::FormatPopulation(Queue).ToString(), FString(TEXT("9")));
		const FString Wait = UAFLW_Lobby_QueueRow::FormatWait(Queue).ToString();
		TestFalse(TEXT("a stalled queue quotes no wait figure"), Wait.Contains(TEXT("~")));
		TestTrue(TEXT("a stalled queue is selectable"), Queue.IsSelectable());
	}

	// NOT OPEN — about the CONTENT. The ONLY state that refuses a press (R63).
	{
		const FAFLLobbyQueue Queue = MakeQueue(EAFLPopulationState::NotOpen, INDEX_NONE, INDEX_NONE, /*bPublished=*/false);
		TestFalse(TEXT("an unopened bracket is not selectable"), Queue.IsSelectable());
	}

	// A LIVE queue still reports its real numbers — the honesty rules must not have made everything mute.
	{
		const FAFLLobbyQueue Queue = MakeQueue(EAFLPopulationState::Live, 88, 40);
		TestEqual(TEXT("a live queue shows its count"),
			UAFLW_Lobby_QueueRow::FormatPopulation(Queue).ToString(), FString(TEXT("88")));
		TestTrue(TEXT("a live queue shows an approximate wait"),
			UAFLW_Lobby_QueueRow::FormatWait(Queue).ToString().Contains(TEXT("40")));
	}

	// An unrecognised server state falls to Unknown, NEVER Cold. This is the same collapse in the parser
	// rather than the renderer, and it is the one a new server-side state would introduce.
	TestTrue(TEXT("`live` parses"), FAFLLobbyQueueId::PopulationStateFromWire(TEXT("live")) == EAFLPopulationState::Live);
	TestTrue(TEXT("`stalled` parses"), FAFLLobbyQueueId::PopulationStateFromWire(TEXT("stalled")) == EAFLPopulationState::Stalled);
	TestTrue(TEXT("an unrecognised state is Unknown, not Cold"),
		FAFLLobbyQueueId::PopulationStateFromWire(TEXT("draining")) == EAFLPopulationState::Unknown);
	TestTrue(TEXT("an empty state is Unknown, not Cold"),
		FAFLLobbyQueueId::PopulationStateFromWire(FString()) == EAFLPopulationState::Unknown);

	return true;
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  THE WIRE CONTRACT
//
//  The honesty rules are only as good as the parser feeding them. `playersMatching` and
//  `estimatedWaitSeconds` are documented `number | null`, and a JSON reader that treats a null as a no-op
//  leaves whatever was already in the variable — which for a default-constructed struct is 0. That single
//  slip renders "we could not find out" as "nobody is there" on every unread cell, and it would look
//  completely correct in a screenshot. These run the real parsers over captured bodies, no network.
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLLobby_QueuesResponseParses, "AFL.Lobby.Wire_QueuesResponseParses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLLobby_QueuesResponseParses::RunTest(const FString&)
{
	// Shaped exactly like GET /queues?includeUnpublished=true — one published Match Play cell, one
	// unpublished BR cell (the live state today: no BR playlist ships), and one staked cell with rungs.
	const FString Body = TEXT(R"JSON({
	  "dimensions": { "totalCells": 3, "publishedCells": 2 },
	  "queues": [
	    { "queueId":"LeaguePlay_Haywire_MatchPlay_Arena_5v5", "tier":"LeaguePlay", "league":"Haywire",
	      "ruleset":"MatchPlay", "venue":"Arena", "bracket":"5v5", "positions":2, "slots":10,
	      "staked":false, "rated":false, "botsPermitted":true, "currency":null, "stakeRungs":[],
	      "published":true, "mapPool":["L_Arena_04"] },
	    { "queueId":"LeaguePlay_Haywire_BattleRoyale_Map_BR_36", "tier":"LeaguePlay", "league":"Haywire",
	      "ruleset":"BattleRoyale", "venue":"Map", "bracket":"BR_36", "positions":36, "slots":36,
	      "staked":false, "rated":false, "botsPermitted":true, "currency":null, "stakeRungs":[],
	      "published":false, "mapPool":[] },
	    { "queueId":"VoltsPlay_ProMod_MatchPlay_Arena_3v3", "tier":"VoltsPlay", "league":"ProMod",
	      "ruleset":"MatchPlay", "venue":"Arena", "bracket":"3v3", "positions":2, "slots":6,
	      "staked":true, "rated":true, "botsPermitted":false, "currency":"VO",
	      "stakeRungs":[100,500,2500,10000], "published":true, "mapPool":["L_Arena_01"] }
	  ]
	})JSON");

	TArray<FAFLLobbyQueue> Queues;
	if (!TestTrue(TEXT("the body parses"), UAFLQueueDirectorySubsystem::ParseQueuesResponse(Body, Queues)))
	{
		return false;
	}
	if (!TestEqual(TEXT("all three cells survive, published and not"), Queues.Num(), 3))
	{
		return false;
	}

	// PUBLISHED starts Unknown — the cell exists and we have not read its population yet. NOT Cold.
	TestTrue(TEXT("a published cell starts Unknown, not Cold"), Queues[0].State == EAFLPopulationState::Unknown);
	TestTrue(TEXT("a published cell is selectable"), Queues[0].IsSelectable());
	TestFalse(TEXT("a fresh cell has no count"), Queues[0].HasCount());

	// UNPUBLISHED starts NotOpen — a fact about the CONTENT (R63), and it must still be in the array so the
	// ladder can draw it disabled rather than hiding the unshipped half.
	TestTrue(TEXT("an unpublished cell is NotOpen"), Queues[1].State == EAFLPopulationState::NotOpen);
	TestFalse(TEXT("an unpublished cell is not selectable"), Queues[1].IsSelectable());
	TestEqual(TEXT("the BR bracket keeps its underscore"), Queues[1].Bracket, FString(TEXT("BR_36")));
	TestTrue(TEXT("the BR ruleset reads off the field, not the id"), Queues[1].Ruleset == EAFLRuleset::BattleRoyale);
	TestEqual(TEXT("BR positions is the field size (R99)"), Queues[1].Positions, 36);

	// LEAGUE cells carry NO rungs. stakeRungs for LeaguePlay returns [] — there is no buy-in to pick.
	TestEqual(TEXT("a league cell publishes no stake rungs"), Queues[0].StakeRungs.Num(), 0);
	TestEqual(TEXT("a Volts cell publishes four rungs (R69)"), Queues[2].StakeRungs.Num(), 4);
	TestEqual(TEXT("the first Volts rung is 100"), Queues[2].StakeRungs[0], 100);
	TestTrue(TEXT("the staked tier resolves"), Queues[2].Tier == EAFLPlayTier::VoltsPlay);
	TestTrue(TEXT("staked is ProMod (R86)"), Queues[2].League == EAFLLeague::ProMod);

	// A malformed body is refused rather than yielding a half-populated ladder.
	TArray<FAFLLobbyQueue> Nothing;
	TestFalse(TEXT("a non-JSON body is refused"),
		UAFLQueueDirectorySubsystem::ParseQueuesResponse(TEXT("<html>502</html>"), Nothing));
	TestFalse(TEXT("a body with no queues array is refused"),
		UAFLQueueDirectorySubsystem::ParseQueuesResponse(TEXT(R"({"error":"registry invalid"})"), Nothing));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAFLLobby_PopulationOverlay, "AFL.Lobby.Wire_PopulationNullIsNotZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAFLLobby_PopulationOverlay::RunTest(const FString&)
{
	const FString Ladder = TEXT(R"JSON({"queues":[
	  { "queueId":"LeaguePlay_Haywire_MatchPlay_Arena_5v5", "tier":"LeaguePlay", "league":"Haywire",
	    "ruleset":"MatchPlay", "venue":"Arena", "bracket":"5v5", "positions":2, "slots":10,
	    "stakeRungs":[], "published":true, "mapPool":["L_Arena_04"] },
	  { "queueId":"LeaguePlay_Haywire_MatchPlay_Arena_8v8", "tier":"LeaguePlay", "league":"Haywire",
	    "ruleset":"MatchPlay", "venue":"Arena", "bracket":"8v8", "positions":2, "slots":16,
	    "stakeRungs":[], "published":true, "mapPool":["L_Arena_04"] },
	  { "queueId":"VoltsPlay_ProMod_MatchPlay_Arena_3v3", "tier":"VoltsPlay", "league":"ProMod",
	    "ruleset":"MatchPlay", "venue":"Arena", "bracket":"3v3", "positions":2, "slots":6,
	    "stakeRungs":[100,500], "published":true, "mapPool":["L_Arena_01"] }
	]})JSON");

	TArray<FAFLLobbyQueue> Queues;
	if (!TestTrue(TEXT("the ladder parses"), UAFLQueueDirectorySubsystem::ParseQueuesResponse(Ladder, Queues)))
	{
		return false;
	}

	// Cell 1: live and fully reported. Cell 2: reported but UNREADABLE — both nullable fields are null, which
	// classify() calls `unknown`. Cell 3: staked, with one live band and one stalled band on its OWN wait.
	// A fourth entry names a cell the ladder does not have, which must be ignored rather than appended.
	const FString Population = TEXT(R"JSON({"cells":[
	  { "queueId":"LeaguePlay_Haywire_MatchPlay_Arena_5v5", "bracket":"5v5", "slots":10,
	    "playersMatching":5, "estimatedWaitSeconds":20, "state":"live", "bands":[] },
	  { "queueId":"LeaguePlay_Haywire_MatchPlay_Arena_8v8", "bracket":"8v8", "slots":16,
	    "playersMatching":null, "estimatedWaitSeconds":null, "state":"unknown", "bands":[] },
	  { "queueId":"VoltsPlay_ProMod_MatchPlay_Arena_3v3", "bracket":"3v3", "slots":6,
	    "playersMatching":9, "estimatedWaitSeconds":38, "state":"live", "bands":[
	      { "centre":100, "waiting":7, "estimatedWaitSeconds":38, "state":"live" },
	      { "centre":500, "waiting":2, "estimatedWaitSeconds":null, "state":"stalled" } ] },
	  { "queueId":"VoltsPlay_ProMod_BattleRoyale_Map_BR_20", "bracket":"BR_20", "slots":20,
	    "playersMatching":3, "estimatedWaitSeconds":null, "state":"stalled", "bands":[] }
	]})JSON");

	const int32 Matched = UAFLQueueDirectorySubsystem::ApplyPopulationResponse(Population, Queues);
	TestEqual(TEXT("only the three cells the ladder knows are matched"), Matched, 3);
	TestEqual(TEXT("an unmatched reading never grows the ladder"), Queues.Num(), 3);

	// A fully-reported cell carries its real numbers through.
	TestTrue(TEXT("cell 1 is live"), Queues[0].State == EAFLPopulationState::Live);
	TestEqual(TEXT("cell 1 count survives"), Queues[0].PlayersMatching, 5);
	TestEqual(TEXT("cell 1 wait survives"), Queues[0].EstimatedWaitSeconds, 20);

	// ⚠ THE ONE THAT MATTERS. Both nullable fields were JSON null. Had the parser let TryGetNumberField fail
	// silently onto a default-constructed 0, this cell would render "0 waiting" — a claim that nobody is
	// there, made about a queue we could not read at all.
	TestTrue(TEXT("a null-reported cell is Unknown"), Queues[1].State == EAFLPopulationState::Unknown);
	TestFalse(TEXT("a null count is NOT a count"), Queues[1].HasCount());
	TestFalse(TEXT("a null wait is NOT an estimate"), Queues[1].HasEstimate());
	TestNotEqual(TEXT("a null count is not zero"), Queues[1].PlayersMatching, 0);
	TestFalse(TEXT("and it never renders as a number"),
		UAFLW_Lobby_QueueRow::FormatPopulation(Queues[1]).ToString().IsNumeric());

	// Bands: present on the staked cell, ABSENT on the league ones. Staked §5.1 — "bands that do not exist"
	// and "bands that are empty" are different things and must not render alike.
	TestEqual(TEXT("a league cell has no bands at all"), Queues[0].Bands.Num(), 0);
	if (TestEqual(TEXT("the staked cell carries both rungs"), Queues[2].Bands.Num(), 2))
	{
		TestTrue(TEXT("the cheap rung is live"), Queues[2].Bands[0].State == EAFLPopulationState::Live);
		// EVERY BAND ON ITS OWN MEASURED WAIT: the cell reads live at 38s while its 500 rung is stalled.
		// Letting the band inherit the cell's number is the laundering §5.2 names.
		TestTrue(TEXT("the dear rung is stalled on its OWN wait, not the cell's"),
			Queues[2].Bands[1].State == EAFLPopulationState::Stalled);
		TestFalse(TEXT("a stalled band quotes no estimate"), Queues[2].Bands[1].HasEstimate());
		TestEqual(TEXT("the stalled band still reports its real count"), Queues[2].Bands[1].Waiting, 2);
	}

	// A population body we cannot read overlays nothing and leaves the ladder standing at Unknown.
	TArray<FAFLLobbyQueue> Fresh;
	UAFLQueueDirectorySubsystem::ParseQueuesResponse(Ladder, Fresh);
	TestEqual(TEXT("a garbage population body matches nothing"),
		UAFLQueueDirectorySubsystem::ApplyPopulationResponse(TEXT("<html>502</html>"), Fresh), 0);
	TestEqual(TEXT("the ladder survives a failed population read"), Fresh.Num(), 3);
	TestTrue(TEXT("and every cell reads Unknown rather than Cold"),
		Fresh[0].State == EAFLPopulationState::Unknown && Fresh[2].State == EAFLPopulationState::Unknown);

	return true;
}
