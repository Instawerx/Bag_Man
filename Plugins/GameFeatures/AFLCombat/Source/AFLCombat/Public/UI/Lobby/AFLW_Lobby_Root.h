// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "UI/AFLW_HomeScreen.h"
#include "UI/Lobby/AFLLobbyTypes.h"

#include "AFLW_Lobby_Root.generated.h"

class UAFLW_Lobby_QueueRow;
class UCommonButtonBase;
class UCommonTextBlock;
class UEditableTextBox;
class UPanelWidget;
class UWidget;

/** The axes S1 can render. `IsAxisLegalForDoor` decides which of them a given door is allowed to show. */
UENUM(BlueprintType)
enum class EAFLLobbyAxis : uint8
{
	/** Region B. MATCH PLAY · BATTLE ROYALE. Both doors, always two tabs, never a dropdown (R19). */
	Ruleset,
	/** HAYWIRE · PRO MOD. **LEAGUE DOOR ONLY** -- staked is Pro Mod only (R86), a stated fact not a control. */
	League,
	/** WATTS · VOLTS. **STAKED DOOR ONLY** -- the league route has no buy-in, so it has no denomination. */
	Denomination,
	/** ARENA · MAP (R97). Both doors. The venue itself stays a server outcome; the CLASS is the choice. */
	VenueClass,
	/** The bracket ladder: 1v1…8v8, or BR_9 · BR_20 · BR_36 under Battle Royale (R99). Both doors. */
	Size,
	/** Presets + numeric + live band. **STAKED DOOR ONLY** -- R98's sharpest clause. */
	Stake
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAFLLobbyQueueCommitted, const FString&, QueueId, int32, Stake);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAFLLobbySelectionChanged, const FAFLLobbyQueue&, Queue);

/**
 * UAFLW_Lobby_Root -- **S1 LobbyRoot**, the surface behind the R98 split.
 *
 * ══ WHAT THIS SCREEN IS ═══════════════════════════════════════════════════════════════════════════════
 *
 * `IRONICS_HOME_SCREEN_SPEC.md` §9.4 ends the home spec AT the split: *"the two lobbies behind it are
 * separate surfaces."* This is that surface, and there is **one class for both**, parameterised by
 * `EAFLHomeDoor`. `STAKED_DOOR_SPEC.md` §1 is explicit about why:
 *
 *     "Chrome is identical to the League door, DELIBERATELY. The doors differ by CONTENT and DENSITY,
 *      never by palette -- colour-separating this surface would make chrome carry meaning that belongs
 *      to identity."
 *
 * Two classes would be two places the chrome lives, and R100 rules the chrome identical. So the door
 * changes which AXES exist, never how the screen looks.
 *
 * ══ THE FIVE PINNED REGIONS ═══════════════════════════════════════════════════════════════════════════
 *
 * `IRONICS_Lobby_Mockup.html` pins `grid-template-rows: 64px 48px 96px 1fr 72px`, and
 * `IRONICS_LOBBY_UX_HANDOFF.md` §3.1 is the PokerStars skeleton 1:1:
 *
 *     A  header       64px    wallet left of centre, population right
 *     B  ruleset tabs 48px    MATCH PLAY · BATTLE ROYALE -- **never a dropdown** (R19)
 *     C  axis row     96px    every axis this door owns, **all live at once** -- R19: no wizard
 *     D  list/detail  1fr     `1fr / 420px`. List-left, S2 card-right
 *     E  commit bar   72px    venue disclosure + the CTA. **Pinned** -- re-queue speed depends on it
 *
 * ══ THE INVARIANT THIS CLASS EXISTS TO HOLD ═══════════════════════════════════════════════════════════
 *
 *     A LEAGUE PLAY PLAYER NEVER ENCOUNTERS A BUY-IN.
 *
 * `LEAGUE_DOOR_SPEC.md` §1 states the stake axis is **"— absent —. Not hidden, not zeroed, not disabled"**,
 * and warns that the temptation to render a greyed-out stake field *"for consistency with the staked door"*
 * reintroduces exactly the framing R98 removed. `IsAxisLegalForDoor` is that rule as a static predicate a
 * test can hold with no world and no widget tree -- the same argument, and the same shape, as
 * `UAFLW_HomeScreen::IsStakeLegalForDoor`.
 *
 * ══ WHAT THIS CLASS DELIBERATELY DOES NOT DO ══════════════════════════════════════════════════════════
 *
 * **It classifies no population.** `LEAGUE_DOOR_SPEC.md` §3.1: *"the server classifies; the door renders."*
 * A threshold two surfaces disagree about is a threshold that means nothing.
 *
 * **It bands no stake.** R59 puts snapping on the server; §3 of the staked spec: *"The UI displays; it
 * never re-implements the rule."* `SetStakeBand` is a setter fed from the server, not a local computation.
 *
 * **It fetches nothing.** The queue set arrives through `SetQueueSet` -- the same setter-rather-than-guess
 * choice `UAFLW_HomeScreen::SetWalletReadout` made, and for the same reason: a widget that owns its own
 * HTTP is a widget that cannot be tested without a network.
 *
 * **It does not skip S4.** R22 makes TicketReview unskippable on a staked entry, so a staked commit raises
 * `OnTicketReviewRequested` and queues NOTHING. Only the league route -- which has no ticket to review,
 * because there is no stake, no cap and no session meter -- goes straight to matchmaking.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_Lobby_Root : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UAFLW_Lobby_Root(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ── THE INVARIANT ────────────────────────────────────────────────────────────────────────────────

	/**
	 * Is this axis permitted on this door? The two door specs' §1 tables, as code.
	 *
	 * | Axis        | League | Staked | Why                                                              |
	 * |-------------|--------|--------|------------------------------------------------------------------|
	 * | Ruleset     |  yes   |  yes   | both products exist on both sides                                |
	 * | VenueClass  |  yes   |  yes   | R97, unchanged by the split                                      |
	 * | Size        |  yes   |  yes   | carries the population readout on both                           |
	 * | League      |  yes   |  NO    | staked is Pro Mod only (R86) -- a stated fact, not a control     |
	 * | Denomination|  NO    |  yes   | the league route has no buy-in, so it has no currency to pick    |
	 * | Stake       |  NO    |  yes   | **R98.** Absent, not hidden, not zeroed, not disabled            |
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Lobby")
	static bool IsAxisLegalForDoor(EAFLHomeDoor Door, EAFLLobbyAxis Axis);

	/** LEAGUE → LeaguePlay. STAKED → WattsPlay or VoltsPlay by denomination. There is no fourth tier. */
	UFUNCTION(BlueprintPure, Category = "AFL|Lobby")
	static EAFLPlayTier ResolveTier(EAFLHomeDoor Door, EAFLDenomination Denomination);

	// ── INTAKE ───────────────────────────────────────────────────────────────────────────────────────

	/**
	 * Which door this lobby is. Set BEFORE activation; changing it re-scopes every axis and reselects.
	 *
	 * Defaults to League because that is the majority path (R98) and a default that silently opened a
	 * wagering surface would be the wrong way round.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void SetDoor(EAFLHomeDoor InDoor);

	EAFLHomeDoor GetDoor() const { return Door; }

	/**
	 * The joined `GET /queues` + `GET /population` set -- **published AND unpublished cells**.
	 *
	 * ⚠ UNPUBLISHED CELLS MUST BE INCLUDED. `LEAGUE_DOOR_SPEC.md` §3.2: *"An unopened bracket is still
	 * drawn. It is disabled rather than hidden, because the whole designed ladder is information."* Hiding
	 * it would be honest but mute. /population reports only published cells, so an unpublished one arrives
	 * from /queues alone and carries `NotOpen` -- which is a fact about the CONTENT, never about the count.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void SetQueueSet(const TArray<FAFLLobbyQueue>& InQueues);

	/**
	 * Wallet readout. Pass INDEX_NONE for a balance not yet known.
	 *
	 * ⚠ UNKNOWN IS NOT ZERO. `STAKED_DOOR_SPEC.md` §6: an unknown balance renders as a skeleton and the CTA
	 * stays disabled, because *"a wrong balance is worse than no balance on a wagering surface"*.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void SetWalletReadout(int64 Watts, int64 Volts);

	/**
	 * The header's "N online". INDEX_NONE while unknown.
	 *
	 * ⚠ DO NOT FEED THIS BY SUMMING /population. That endpoint deliberately publishes no headline total --
	 * *"a healthy total conceals a dead band, and this endpoint exists to stop that being the number anyone
	 * renders"* -- and "queued right now" is a different quantity from "online" anyway. The real source is
	 * owed; until it lands this stays INDEX_NONE and the chip reads nothing rather than something false.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void SetOnlineCount(int32 Count);

	/**
	 * The band the SERVER resolved for the current stake (R59, R60). Pass an empty label to clear it.
	 *
	 * Width is ±20% of the snapped rung and **widens with wait, centre never moving** -- so the label is a
	 * live server fact, not a formatting of the typed number. Nothing in this class computes it.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void SetStakeBand(const FText& BandLabel, bool bValueInSomeBand);

	// ── SELECTION ────────────────────────────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby") void SelectRuleset(EAFLRuleset InRuleset);
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby") void SelectLeague(EAFLLeague InLeague);
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby") void SelectDenomination(EAFLDenomination InDenomination);
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby") void SelectVenueClass(EAFLVenueClass InVenue);

	/** The single mutation the size grid AND the queue list both call -- they are two views of one queue. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby") void SelectBracket(const FString& InBracket);

	/** Typed or preset stake. Travels UNROUNDED (§5.1, R59); the server snaps it. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby") void SetStake(int32 InStake);

	/** The currently selected cell, or nullptr when nothing is open in this scope. */
	const FAFLLobbyQueue* GetSelectedQueue() const;

	// ── COMMIT ───────────────────────────────────────────────────────────────────────────────────────

	/**
	 * Press the CTA. LEAGUE goes straight to matchmaking; STAKED raises `OnTicketReviewRequested` and
	 * queues nothing, because R22 makes S4 unskippable **including from re-queue**.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void CommitQueue();

	/** R22. True on any staked entry -- there is a cap and a session meter that must be legible first. */
	UFUNCTION(BlueprintPure, Category = "AFL|Lobby")
	bool RequiresTicketReview() const;

	/** Fires when the league route actually enters matchmaking. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Lobby")
	FAFLLobbyQueueCommitted OnQueueCommitted;

	/** Fires INSTEAD of committing on a staked entry. S4 is owed; this is the hand-off point for it. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Lobby")
	FAFLLobbyQueueCommitted OnTicketReviewRequested;

	UPROPERTY(BlueprintAssignable, Category = "AFL|Lobby")
	FAFLLobbySelectionChanged OnSelectionChanged;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ── WBP HOOKS ────────────────────────────────────────────────────────────────────────────────────

	/** Region C changed shape for this door. The WBP shows/hides the axis panels it owns. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Lobby", meta = (DisplayName = "On Door Scoped"))
	void BP_OnDoorScoped(EAFLHomeDoor InDoor, EAFLPlayTier Tier);

	/** Rows and tiles have been rebuilt. The WBP plays the §13 motion. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Lobby", meta = (DisplayName = "On Queues Rebuilt"))
	void BP_OnQueuesRebuilt(int32 OpenCount, int32 TotalCount);

	/** The CTA's enabled state changed. Reason is non-empty exactly when disabled -- never a silent no-op. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Lobby", meta = (DisplayName = "On Commit State Changed"))
	void BP_OnCommitStateChanged(bool bEnabled, const FText& Reason);

	// ── BINDINGS ─────────────────────────────────────────────────────────────────────────────────────
	//
	// REQUIRED are the three without which this is not S1: the bracket ladder, the queue list and the CTA.
	// Everything else is optional so the WBP can be built in stages -- and because the door legitimately
	// omits some of them. A league WBP does not author a stake panel AT ALL, which is the whole point of
	// R98; making StakePresetBox required would force the greyed-out stake field the spec forbids.

	/** C · the bracket ladder. Filled with `SizeTileClass`; one tile per bracket, open or not. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby")
	TObjectPtr<UPanelWidget> SizeAxisBox;

	/** D · the queue list. Filled with `QueueRowClass`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby")
	TObjectPtr<UPanelWidget> QueueListBox;

	/** E · the CTA. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby")
	TObjectPtr<UCommonButtonBase> CommitButton;

	// A · header
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> WattsChip;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> VoltsChip;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> OnlineChip;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> BackButton;

	// B · ruleset tabs. Two buttons, both live, no disabled variant (R41 §3.3: a tab either works or does
	// not exist), and never a dropdown (R19 wants them visible and comparable).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> MatchPlayTab;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> BattleRoyaleTab;

	// C · league axis -- LEAGUE DOOR ONLY.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UWidget> LeagueAxisPanel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> HaywireButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> ProModButton;

	// C · denomination axis -- STAKED DOOR ONLY.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UWidget> DenominationAxisPanel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> WattsButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> VoltsButton;

	// C · venue class -- both doors (R97).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> ArenaButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> MapButton;

	/** C · the size axis label. Re-labels FORMAT → FIELD under Battle Royale (R99, league spec §7). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> SizeAxisLabel;

	// C · stake -- STAKED DOOR ONLY. Presets primary, numeric secondary but NEVER hidden, NO SLIDER (R20).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UWidget> StakeAxisPanel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UPanelWidget> StakePresetBox;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UEditableTextBox> StakeNumericField;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> BandReadout;

	// D/E · chrome
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> ListFooter;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> VenueNote;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> CommitSummary;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> CommitReason;

	// ── CLASSES THE WBP SUPPLIES ─────────────────────────────────────────────────────────────────────

	/**
	 * The row WBP for region D.
	 *
	 * The SIZE TILE uses the same chassis by design, not by shortcut: with ruleset, league/denomination and
	 * venue fixed, a bracket IS exactly one cell, so a tile and a row carry the same four facts (bracket,
	 * band, population, wait). `STAKED_DOOR_SPEC.md` §5 says so of the size grid and the stake ladder --
	 * *"two views of one queue, so they are looked up once and rendered twice"* -- and two classes would be
	 * two places that could disagree about how busy the queue actually is.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Lobby")
	TSubclassOf<UAFLW_Lobby_QueueRow> QueueRowClass;

	/** The tile WBP for region C's ladder. Same chassis, different layout. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Lobby")
	TSubclassOf<UAFLW_Lobby_QueueRow> SizeTileClass;

	/** The stake preset tile. Rungs come from the cell's `StakeRungs`, never authored here (R20 §16.1). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Lobby")
	TSubclassOf<UCommonButtonBase> StakePresetClass;

private:
	/**
	 * UEditableTextBox::OnTextChanged is a DYNAMIC multicast, unlike UCommonButtonBase::OnClicked() -- so
	 * this one genuinely has to be a UFUNCTION bound with AddDynamic. Mixing the two idioms up is how a
	 * binding silently does nothing.
	 */
	UFUNCTION()
	void HandleStakeTextChanged(const FText& Text);

	void ApplyDoorScoping();
	void RebuildAxisOptions();
	void RebuildQueueList();
	void RefreshCommitState();
	void RefreshWalletChips();
	void BroadcastSelection();

	/** Rebuild both views, then fire any selection change ReconcileSelection deferred. One tail, one order. */
	void FinishRefresh();

	/**
	 * Restate the existing rows and tiles without destroying them.
	 *
	 * ⚠ THE REBUILD PATH IS UNSAFE FROM A ROW'S OWN CLICK HANDLER -- `SelectBracket` runs inside one, and a
	 * rebuild would `ClearChildren` the panel holding the widget whose click is still on the stack. This is
	 * the path those callers take.
	 */
	void RefreshRowsInPlace();

	/** Light the preset matching the current stake. Separate beat from selection, so separate function. */
	void RefreshPresetSelection();

	/** Cells matching the current ruleset / league / venue scope, sorted for display. */
	void CollectScopedQueues(TArray<const FAFLLobbyQueue*>& Out) const;

	/** Bind a two-button axis without repeating the same four lines six times. */
	void BindAxisButton(UCommonButtonBase* Button, TFunction<void()> Handler);

	/** Keep the player's bracket if it is still open; otherwise move to the first that is (league §7). */
	void ReconcileSelection();

	FText FormatStakeWithSuffix(int32 Amount) const;
	const TCHAR* CurrencySuffix() const;

	// -- state

	UPROPERTY() EAFLHomeDoor Door = EAFLHomeDoor::League;
	UPROPERTY() EAFLRuleset Ruleset = EAFLRuleset::MatchPlay;
	UPROPERTY() EAFLLeague League = EAFLLeague::Haywire;
	UPROPERTY() EAFLDenomination Denomination = EAFLDenomination::Watts;
	UPROPERTY() EAFLVenueClass Venue = EAFLVenueClass::Arena;

	UPROPERTY() FString SelectedBracket;
	UPROPERTY() int32 Stake = 0;

	UPROPERTY() TArray<FAFLLobbyQueue> Queues;
	UPROPERTY() TArray<TObjectPtr<UAFLW_Lobby_QueueRow>> SpawnedRows;
	UPROPERTY() TArray<TObjectPtr<UAFLW_Lobby_QueueRow>> SpawnedTiles;
	UPROPERTY() TArray<TObjectPtr<UCommonButtonBase>> SpawnedPresets;

	/** Parallel to SpawnedPresets -- UCommonButtonBase carries no value field, so the rung is tracked here. */
	UPROPERTY() TArray<int32> SpawnedPresetRungs;

	/** Set by ReconcileSelection, consumed by FinishRefresh -- see the deferred-broadcast note in the .cpp. */
	UPROPERTY() bool bSelectionChangedPending = false;

	/** Server-resolved, never computed here. Empty means "no band stated". */
	UPROPERTY() FText StakeBandLabel;
	UPROPERTY() bool bStakeInSomeBand = true;

	UPROPERTY() int64 WattsBalance = INDEX_NONE;
	UPROPERTY() int64 VoltsBalance = INDEX_NONE;
	UPROPERTY() int32 OnlineCount = INDEX_NONE;
};
