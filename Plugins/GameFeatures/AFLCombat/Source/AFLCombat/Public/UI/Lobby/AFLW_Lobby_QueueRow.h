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
	 * The strings, as static functions so a test can hold the honesty rules without a widget tree -- the same
	 * pattern as `UAFLW_HomeScreen::IsStakeLegalForDoor`.
	 */

	/** `5v5` / `BR_36`. The bracket id verbatim: it is already the player-facing label (R99). */
	static FText FormatBracket(const FAFLLobbyQueue& InQueue);

	/** `88` when known, `--` when not. NEVER `0` for an unknown count. */
	static FText FormatPopulation(const FAFLLobbyQueue& InQueue);

	/** `~40s` / `~2m` / `no recent match` / `no estimate` / `Not open yet`. Never fabricates a figure. */
	static FText FormatWait(const FAFLLobbyQueue& InQueue);

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
};
