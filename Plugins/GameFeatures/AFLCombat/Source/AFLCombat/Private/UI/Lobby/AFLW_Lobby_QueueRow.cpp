// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/Lobby/AFLW_Lobby_QueueRow.h"

#include "CommonTextBlock.h"

#define LOCTEXT_NAMESPACE "AFLLobby"

namespace
{
	/** `~20s` / `~2m` / `~1h`. Under a minute reads in seconds because that is the unit the wait is felt in. */
	FText HumaniseWait(int32 Seconds)
	{
		if (Seconds < 60)
		{
			return FText::Format(LOCTEXT("WaitSeconds", "~{0}s"), FText::AsNumber(Seconds));
		}
		if (Seconds < 3600)
		{
			const int32 Minutes = FMath::Max(1, FMath::RoundToInt(Seconds / 60.0f));
			return FText::Format(LOCTEXT("WaitMinutes", "~{0}m"), FText::AsNumber(Minutes));
		}
		const int32 Hours = FMath::Max(1, FMath::RoundToInt(Seconds / 3600.0f));
		return FText::Format(LOCTEXT("WaitHours", "~{0}h"), FText::AsNumber(Hours));
	}
}

void UAFLW_Lobby_QueueRow::SetQueue(const FAFLLobbyQueue& InQueue, const FText& BandLabel)
{
	Queue = InQueue;

	if (BracketText)    { BracketText->SetText(FormatBracket(Queue)); }
	if (PopulationText) { PopulationText->SetText(FormatPopulation(Queue)); }
	if (WaitText)       { WaitText->SetText(FormatWait(Queue)); }

	if (BandText)
	{
		// A league WBP does not author this slot at all, so reaching here with a bound-but-empty band means
		// a STAKED row was handed no boundary. Collapse rather than print a bare dash: an empty stake field
		// on a wagering row is the greyed-out stake control R98 removed, wearing a different name.
		BandText->SetText(BandLabel);
		BandText->SetVisibility(BandLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// ⚠ INTERACTABILITY IS DRIVEN BY IsSelectable(), NOT BY "is this queue busy".
	//
	// Cold stays pressable on purpose (handoff §12, league door §7): every band renders with honest counts
	// and the commit control stays live, because "waiting deliberately is a different experience from
	// waiting silently". The ONLY row that refuses a press is NotOpen -- there is no queue behind it (R63),
	// so accepting the press would be an offer the game cannot honour.
	SetIsEnabled(Queue.IsSelectable());

	BP_OnQueueSet(Queue, Queue.State);
}

void UAFLW_Lobby_QueueRow::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Rows and size tiles are BOTH this class (WBP_IRONICS_Lobby_QueueRow and WBP_IRONICS_Lobby_SizeTile),
	// and they are spawned into the list at RUNTIME -- so there is no instance in any WBP to tick a
	// focusable box on. Unfocusable, they log "does not support focus" and cannot be reached by keyboard or
	// gamepad, which makes the queue list mouse-only. Set here rather than on the WBP so every child and
	// every runtime-created row gets it without anyone remembering to.
	SetIsFocusable(true);

	// Designer-time only: without this the row shows whatever placeholder strings the WBP was authored with,
	// which is how a mocked-up "88 waiting" ends up looking like a live reading in a screenshot.
	if (IsDesignTime() && BracketText && BracketText->GetText().IsEmpty())
	{
		BracketText->SetText(LOCTEXT("RowPreviewBracket", "5v5"));
	}
}

FText UAFLW_Lobby_QueueRow::FormatBracket(const FAFLLobbyQueue& InQueue)
{
	// The bracket id IS the player-facing label -- `5v5`, `BR_36` (R99). Nothing is prettified, because a
	// second spelling of a bracket is a second place it can drift from the registry.
	return FText::FromString(InQueue.Bracket);
}

FText UAFLW_Lobby_QueueRow::FormatPopulation(const FAFLLobbyQueue& InQueue)
{
	switch (InQueue.State)
	{
	case EAFLPopulationState::NotOpen:
		return FText::GetEmpty();   // the wait column carries `Not open yet`; a count here would be noise

	case EAFLPopulationState::Unknown:
		// ⚠ NEVER `0`. §3.2: "we could not find out" and "nobody is there" are opposite claims, and this is
		// the one surface whose entire job is honesty about population.
		return LOCTEXT("PopUnavailable", "Count unavailable");

	default:
		break;
	}

	// ══ THE SIT-AND-GO READ: X OF N, ALWAYS ════════════════════════════════════════════════════════════
	//
	// This used to be a bare count -- `3` -- with Cold rendering the word "Quiet". Both hid the number that
	// actually decides whether to press: how many are NEEDED. `3` means something completely different in a
	// 1v1 than in a BR_36, and "Quiet" says nothing at all about whether being first is worth it.
	//
	// ⚠ AND THE OMISSION HAD TEETH ON THE STAKED LADDER. Cold is deliberately still selectable -- "someone
	// has to be first" -- which is TRUE for LEAGUE PLAY, where bot fill completes the field at the commit
	// deadline. It is FALSE for staked, where bots are barred and nothing completes it. A staked BR_36
	// showing "Quiet" invited a player into a queue that could never fire. `3 / 36` tells them the truth and
	// lets them choose, which is exactly what a poker lobby does with `6/9 seated`.
	//
	// ⚠ THIS IS ALSO WHY THERE IS NO `Unfillable` STATE GATING THE PRESS. Disabling an empty cell guarantees
	// it stays empty -- nobody can be first, so the count never reaches one, so it never enables. That is a
	// queue death spiral, self-inflicted. The count makes "be first" an informed choice instead of a trap.
	// ⚠ THE DENOMINATOR ONLY MEANS SOMETHING WHILE THE CELL IS SHORT, and getting that wrong is how this was
	// first written. PlayersMatching counts players WAITING, not players seated -- so a healthy cell can hold
	// far more than one match's worth. `88 / 10` is not an over-full table, it is eight matches forming, and
	// rendering it as a fraction states a shortfall that does not exist.
	//
	// So the fraction appears exactly where it earns its place: a cell that cannot yet fill. At or above the
	// field size the bare count returns, because the question "how many more?" has already been answered.
	if (!InQueue.HasCount())
	{
		return LOCTEXT("PopUnavailable_Fallback", "Count unavailable");
	}
	if (InQueue.PlayersMatching >= InQueue.Slots)
	{
		return FText::AsNumber(InQueue.PlayersMatching);
	}
	return FText::Format(LOCTEXT("PopOfSlots", "{0} / {1}"),
		FText::AsNumber(InQueue.PlayersMatching), FText::AsNumber(InQueue.Slots));
}

FText UAFLW_Lobby_QueueRow::FormatWait(const FAFLLobbyQueue& InQueue)
{
	// ══ WHEN A CELL IS NEARLY FULL, THE GAP BEATS EVERY ESTIMATE ═══════════════════════════════════════
	//
	// `needs 2 more` is a fact and an instruction. A wait estimate at the same moment is a guess about human
	// behaviour, and this surface refuses guesses on principle. Checked BEFORE the state switch because it
	// outranks all of them: a Stalled cell one player short is not "no recent match", it is nearly away.
	//
	// It deliberately does NOT fire at a gap of zero or on an empty cell -- a full cell is about to place and
	// says so, and `needs 36 more` on a dead ladder is a taunt rather than an instruction. The count column
	// already carries `0 / 36` for that case, which is the honest read without the false urgency.
	// ⚠ ONLY WHILE THE CELL IS SHORT. PlayersMatching counts players WAITING, so a busy cell holds more than
	// one match's worth and has a real measured wait -- suppressing that in favour of a fabricated "full"
	// would replace the one honest figure on the row with a guess. A gap at or below zero is not a shortfall
	// and falls straight through to the estimate.
	if (InQueue.State != EAFLPopulationState::NotOpen && InQueue.HasCount() && InQueue.PlayersMatching > 0)
	{
		const int32 Gap = InQueue.Slots - InQueue.PlayersMatching;
		if (Gap > 0 && Gap <= NearlyFullGap)
		{
			return FText::Format(LOCTEXT("WaitNeedsMore", "needs {0} more"), FText::AsNumber(Gap));
		}
	}

	switch (InQueue.State)
	{
	case EAFLPopulationState::NotOpen:
		return LOCTEXT("WaitNotOpen", "Not open yet");

	case EAFLPopulationState::Stalled:
		// The reading that had no way to be said before: people are here and NOTHING has matched. It is the
		// difference between "you will wait a while" and "nobody here has got a game at all".
		return LOCTEXT("WaitStalled", "no recent match");

	case EAFLPopulationState::Cold:
	case EAFLPopulationState::Unknown:
		return LOCTEXT("WaitNoEstimate", "no estimate");

	default:
		break;
	}

	// Live / Warm. The server suppresses the estimate on a cold or unknown cell, so an absent figure here
	// means the classification and the estimate disagree -- report the absence rather than inventing one.
	return InQueue.HasEstimate()
		? HumaniseWait(InQueue.EstimatedWaitSeconds)
		: LOCTEXT("WaitNoEstimate_Fallback", "no estimate");
}

FText UAFLW_Lobby_QueueRow::FormatAccessibleSummary(const FAFLLobbyQueue& InQueue, const FText& BandLabel)
{
	// Handoff §14: rows are a listbox and each row announces itself. The population STATE is spoken, because
	// the visual cue for cold is opacity and opacity does not reach a screen reader.
	FText StateSpoken;
	switch (InQueue.State)
	{
	case EAFLPopulationState::NotOpen: StateSpoken = LOCTEXT("SpokenNotOpen", "not open yet"); break;
	case EAFLPopulationState::Cold:    StateSpoken = LOCTEXT("SpokenCold", "quiet, nobody waiting"); break;
	case EAFLPopulationState::Stalled: StateSpoken = LOCTEXT("SpokenStalled", "nothing has matched recently"); break;
	case EAFLPopulationState::Unknown: StateSpoken = LOCTEXT("SpokenUnknown", "population unavailable"); break;
	default:
		StateSpoken = InQueue.HasCount()
			? FText::Format(LOCTEXT("SpokenWaiting", "{0} waiting"), FText::AsNumber(InQueue.PlayersMatching))
			: LOCTEXT("SpokenUnknown_Fallback", "population unavailable");
		break;
	}

	if (BandLabel.IsEmpty())
	{
		return FText::Format(LOCTEXT("SpokenRow", "{0}, {1}, estimated wait {2}"),
			FormatBracket(InQueue), StateSpoken, FormatWait(InQueue));
	}
	return FText::Format(LOCTEXT("SpokenRowBanded", "{0}, {1}, {2}, estimated wait {3}"),
		FormatBracket(InQueue), BandLabel, StateSpoken, FormatWait(InQueue));
}

#undef LOCTEXT_NAMESPACE
