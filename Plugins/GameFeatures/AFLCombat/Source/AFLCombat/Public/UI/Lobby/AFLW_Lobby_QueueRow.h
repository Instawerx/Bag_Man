// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"
#include "Online/AFLLobbyTypes.h"

#include "AFLW_Lobby_QueueRow.generated.h"

class UCommonTextBlock;

/**
 * UAFLW_Lobby_QueueRow -- ONE ROW OF S1 REGION D. A ROW IS A QUEUE, NOT AN EVENT.
 *
 * ══ THE R18 LINE, AND WHY IT LIVES IN C++ ═════════════════════════════════════════════════════════════
 *
 * `IRONICS_LOBBY_UX_HANDOFF.md` §3.3: **no map name, no venue art, no per-row identity** -- and it names
 * this as *"the single most likely thing to drift back toward PokerStars under pressure"*, because event
 * rows are more visually interesting and that is exactly why they are tempting.
 *
 * The reason the row has a C++ chassis rather than being a pure widget graph is the same reason
 * `UAFLW_HomeScreen` does: the rule is only worth what enforces it. This class exposes ONLY the fields a
 * queue has -- bracket, band, population, wait. There is no venue field to bind, so a designer cannot add a
 * map name to the row without first adding it here, in a diff somebody has to read.
 *
 * ══ THE ROW NEVER COMPUTES A POPULATION READING ═══════════════════════════════════════════════════════
 *
 * `IRONICS_LEAGUE_DOOR_SPEC.md` §3.1: *"the server classifies; the door renders."* `SetQueue` takes the
 * state already decided and turns it into strings. Every string it produces is honest about absence:
 *
 *   - an unknown count renders `Count unavailable`, **never `0`** -- §3.2 calls those opposite claims
 *   - an unknown wait renders `no estimate`, never an optimistic number
 *   - a stalled queue says `no recent match`, which is a different claim from "you will wait a while"
 *
 * ══ COLD IS A VARIANT, NOT A DISABLED STATE ═══════════════════════════════════════════════════════════
 *
 * Handoff §9 marks cold as a first-class variant and §12 keeps it **selectable**: the player who then waits
 * is waiting *deliberately*, which §5.2 calls a completely different experience from the same wait imposed
 * without explanation. Only `NotOpen` disables a row -- there is genuinely no queue behind it (R63).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_Lobby_QueueRow : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/**
	 * Fill the row from a joined /queues + /population cell. Also sets interactability from IsSelectable().
	 *
	 * ⚠ THE BAND IS PASSED IN, NOT DERIVED HERE, and that is R59. Stake is NOT a queue dimension
	 * (`registry.ts`: amounts within a currency can settle together, so the SERVER bands them rather than
	 * tiering the queue by them) -- so a band is a property of the player's current stake applied to this
	 * cell, not of the cell. The row displays whatever boundary the server resolved and re-implements
	 * nothing: "the UI displays; it never re-implements the rule" (`STAKED_DOOR_SPEC.md` §3).
	 *
	 * Empty BandLabel is the LEAGUE PLAY case -- there is no buy-in, so there is no band to state (R98).
	 */
	void SetQueue(const FAFLLobbyQueue& InQueue, const FText& BandLabel = FText::GetEmpty());

	const FAFLLobbyQueue& GetQueue() const { return Queue; }
	const FString& GetQueueId() const { return Queue.QueueId; }

	/**
	 * Mark this row as the cell the player currently holds an entry in, so the WBP can offer LEAVE instead of
	 * a bare selection.
	 *
	 * ⚠ SET FROM MATCHMAKING STATE, NOT FROM THE CLICK THAT COMMITTED. A row that latched "queued" on its own
	 * press would keep saying so after a refusal, a timeout, or the sweeper releasing the entry -- the four
	 * cases where the label matters most and where the row has no idea anything happened. The root re-derives
	 * it from UAFLMatchmakingSubsystem on every state transition, so the row is a renderer here exactly as it
	 * is for population: the subsystem classifies, the row displays.
	 */
	void SetIsQueued(bool bInIsQueued);

	bool IsQueued() const { return bIsQueued; }

	/**
	 * LEAVE THIS CELL AND STAY IN ANY OTHERS. What the row's LEAVE button calls.
	 *
	 * Named for what it does rather than "Cancel": cancel is the whole search, and this is one cell. The
	 * distinction is the entire reason /cancel-ticket grew a selector.
	 *
	 * ⚠ REFUSES WHEN THE ROW IS NOT THE QUEUED ONE, rather than sending the request and letting the server
	 * answer `noSuchEntry`. A row that is not queued has no business withdrawing anything, and the guard means
	 * a WBP that leaves the button visible by mistake cannot pull a player out of a DIFFERENT cell -- the
	 * failure a bare "cancel everything" call would produce from exactly this bug.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void RequestLeave();

private:
	/** State the LEAVE control's resting appearance on a freshly-spawned row. See the .cpp -- the WBP default
	 *  cannot be authored, so the row asserts it instead of inheriting it. */
	void EstablishQueuedVisual();

public:

	/**
	 * The strings, as static functions so a test can hold the honesty rules without a widget tree -- the same
	 * pattern as `UAFLW_HomeScreen::IsStakeLegalForDoor`.
	 */

	/** `5v5` / `BR_36`. The bracket id verbatim: it is already the player-facing label (R99). */
	static FText FormatBracket(const FAFLLobbyQueue& InQueue);

	/**
	 * `7 / 9` -- the sit-and-go read. `Count unavailable` when unknown, and NEVER `0` for an unknown count.
	 *
	 * OF SLOTS, not a bare number. `3` means something completely different in a 1v1 than in a BR_36, and the
	 * denominator is what tells a player whether being first is worth it -- which is the whole question on a
	 * staked cell, where bots are barred and nothing completes a short field.
	 */
	static FText FormatPopulation(const FAFLLobbyQueue& InQueue);

	/** `needs 2 more` / `~40s` / `no recent match` / `no estimate` / `Not open yet`. Never fabricates a figure. */
	static FText FormatWait(const FAFLLobbyQueue& InQueue);

	/**
	 * How close counts as nearly full -- the gap at which the wait column names the shortfall instead of
	 * estimating a time.
	 *
	 * 3 is a judgement call. Small enough that `needs 3 more` is a call to action rather than a status, and
	 * large enough to cover the last stretch of a 9-slot field where the estimate is worthless anyway.
	 */
	static constexpr int32 NearlyFullGap = 3;

	/**
	 * The full spoken row -- *"5v5, 400 to 500 Volts, 88 waiting, estimated wait 40 seconds"* (handoff §14).
	 *
	 * ⚠ A COLD ROW ANNOUNCES ITS STATE. The visual cue for cold is opacity, which is not available to a
	 * screen reader, so the state has to be in the text or it does not exist for that player.
	 */
	static FText FormatAccessibleSummary(const FAFLLobbyQueue& InQueue, const FText& BandLabel = FText::GetEmpty());

protected:
	virtual void NativePreConstruct() override;

	/**
	 * WBP hook: C++ has filled the row; the WBP applies the §3.2 dot treatment (filled/pulsing, hollow,
	 * hollow double ring, dashed) and the row opacity. The four absences must stay visually distinct.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Lobby", meta = (DisplayName = "On Queue Set"))
	void BP_OnQueueSet(const FAFLLobbyQueue& InQueue, EAFLPopulationState State);

	/**
	 * WBP hook: this row is (or is no longer) the cell the player is queued in. The WBP shows or hides its
	 * LEAVE affordance and any "searching" treatment.
	 *
	 * ⚠ THE WBP MUST NOT INVENT A SECOND SOURCE OF TRUTH HERE. It renders what this says; if it also tracked
	 * its own idea of "queued" from button presses, the two would disagree the first time a ticket ended
	 * without a press -- which is every timeout, every sweep, and every match that forms.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Lobby", meta = (DisplayName = "On Queued State Changed"))
	void BP_OnQueuedStateChanged(bool bInIsQueued);

	// -- Bindings. Only what a QUEUE has. There is deliberately no venue slot here; see the class comment.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby")
	TObjectPtr<UCommonTextBlock> BracketText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby")
	TObjectPtr<UCommonTextBlock> PopulationText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby")
	TObjectPtr<UCommonTextBlock> WaitText;

	/**
	 * The stake band, e.g. `400-500 V`. **BindWidgetOptional, and that is a design fact rather than
	 * convenience:** a LEAGUE PLAY row has no band because there is no buy-in (R98), so the league WBP does
	 * not author this slot at all. Making it required would force a league row to carry an empty stake
	 * field -- precisely the greyed-out stake control the league door spec forbids.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby")
	TObjectPtr<UCommonTextBlock> BandText;

private:
	UPROPERTY()
	FAFLLobbyQueue Queue;

	bool bIsQueued = false;
};
