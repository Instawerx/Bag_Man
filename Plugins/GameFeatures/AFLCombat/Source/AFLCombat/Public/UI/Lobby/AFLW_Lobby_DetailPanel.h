// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Online/AFLLobbyTypes.h"
#include "Online/AFLPayoutPreview.h"

#include "AFLW_Lobby_DetailPanel.generated.h"

class UCommonButtonBase;
class UCommonTextBlock;
class UPanelWidget;
class UWidgetSwitcher;

/** Three sections, not five. See the class comment for why that decided the layout. */
UENUM(BlueprintType)
enum class EAFLQueueDetailTab : uint8
{
	Overview,
	Payouts,
	Rules
};

/**
 * UAFLW_Lobby_PayoutRow -- one rung of the ladder.
 *
 * Plain UUserWidget (passive), same shape as `UAFLW_ScoreboardRow`: C++ owns the bindings, the WBP owns the
 * layout. One row = PLACE | ~SHARE | ~PAYOUT | ~MULTIPLE.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_Lobby_PayoutRow : public UUserWidget
{
	GENERATED_BODY()

public:
	/** CurrencySuffix is empty on the league route -- which has no ladder, so in practice it is always set. */
	void SetRung(const FAFLPayoutRung& Rung, const FString& CurrencySuffix);

protected:
	/**
	 * WBP hook for the MIN-CASH mark.
	 *
	 * ⚠ THE MARK IS A NEUTRAL WHITE RING, NEVER THE NEON EDGE. It marks a THRESHOLD, and the brand's lit
	 * edge would read as "best" -- the opposite of what the last paid place means.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Lobby", meta = (DisplayName = "On Rung Set"))
	void BP_OnRungSet(bool bIsMinCash);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> PlaceText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> PayoutText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> MultipleText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> ShareText;
};

/**
 * UAFLW_Lobby_DetailPanel -- **S2 QueueDetail**, region D's right column at 420px.
 *
 * ══ OPTION B, AND THE REASON IS ARITHMETIC ════════════════════════════════════════════════════════════
 *
 * A tab row across the top rather than a left nav. `IRONICS_LOBBY_UX_HANDOFF.md` §4: Option D's left rail
 * carries five sections because *a poker tournament has that much internal structure*. **A queue has
 * three** -- Overview, Payouts, Rules -- and a five-slot nav holding three items reads as unfinished while
 * costing the vertical space the payout ladder needs.
 *
 * ══ EVERY FIGURE IS AN ESTIMATE AND IS RENDERED AS ONE ════════════════════════════════════════════════
 *
 * Prefixed `~`, labelled `est.`, without exception. The ladder is a generating rule solved per exact field
 * size, and *the field is not final until the match starts*; a hard number here would be the same class of
 * promise R20 §4.2 forbids on the stake band. The rake behind those figures is a **working assumption**
 * (§15.7), not a ruling -- every one of them moves if it changes.
 *
 * ══ THE PAYOUTS TAB IS LIVE UNDER BOTH RULESETS ═══════════════════════════════════════════════════════
 *
 * §16.7 disabled it once, when TURBO held the second slot and produced no finishing positions to key on.
 * R41 replaced TURBO with MATCH PLAY, which has two -- so the rule resolves to a single winner-takes-all
 * row. **Defined content, not empty.** There is deliberately no disabled variant: a tab that greys out on
 * one ruleset teaches players it is sometimes broken.
 *
 * ⚠ WINNER-TAKES-ALL IS NOT A MODE FLAG. `ceil(0.15 x 2) = 1` for any team series however many players;
 * nine BR positions is under the small-field threshold, so also 1. It falls out of the division, and the
 * panel says so rather than announcing a special case.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_Lobby_DetailPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Show a queue.
	 *
	 * @param InQueue          The joined cell -- carries positions, slots and the population reading.
	 * @param StakePerPlayer   0 on the league route. One position's entry is this x the team size.
	 * @param BandLabel        Server-resolved; empty where there is no buy-in.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void SetQueue(const FAFLLobbyQueue& InQueue, int32 StakePerPlayer, const FText& BandLabel);

	UFUNCTION(BlueprintCallable, Category = "AFL|Lobby")
	void SelectTab(EAFLQueueDetailTab Tab);

	/** The solved ladder for the queue currently shown. Invalid until SetQueue lands a real cell. */
	const FAFLPayoutLadder& GetLadder() const { return Ladder; }

	/**
	 * How many positions this cell resolves to, and what ONE of them pays in.
	 *
	 * ⚠ A TEAM IS ONE POSITION. A 5v5 is ten players and TWO finishing positions, so a position's entry is
	 * the stake times the team size -- which is what makes a multiple read the same per-player as
	 * per-position under an even split. Static so a test can hold it without a widget.
	 */
	static void ResolveField(const FAFLLobbyQueue& InQueue, int32 StakePerPlayer,
		int32& OutPositions, int32& OutEntryPerPosition);

protected:
	virtual void NativeOnInitialized() override;

	/** The WBP paints the tab row's selected state; C++ owns which tab is current. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Lobby", meta = (DisplayName = "On Tab Changed"))
	void BP_OnTabChanged(EAFLQueueDetailTab Tab);

	/** Fired after the ladder is rebuilt, so the WBP can play the §13 fade-and-rise. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Lobby", meta = (DisplayName = "On Queue Shown"))
	void BP_OnQueueShown(const FAFLLobbyQueue& InQueue, bool bWinnerTakesAll);

	// -- header
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby")         TObjectPtr<UCommonTextBlock> TitleText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> BandText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> QueueButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> BackButton;

	// -- the three metric cards. PokerStars 1:1 (handoff §9).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> StakeValue;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> StakeSub;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> PoolValue;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> PoolSub;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> PlayersValue;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> PlayersSub;

	// -- tabs + body
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> OverviewTab;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> PayoutsTab;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonButtonBase> RulesTab;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UWidgetSwitcher> BodySwitcher;

	/** Overview + Rules bodies. Venue disclosure appears on both -- it is the R18 statement, not a detail. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> OverviewText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> RulesText;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> VenueNote;

	/** The ladder's rows go here. REQUIRED: without it the Payouts tab is the empty thing R41 closed. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Lobby")         TObjectPtr<UPanelWidget> PayoutLadderBox;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Lobby") TObjectPtr<UCommonTextBlock> PayoutFootnote;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Lobby")
	TSubclassOf<UAFLW_Lobby_PayoutRow> PayoutRowClass;

private:
	void RebuildLadder();
	void RefreshBodies();
	FString CurrencySuffix() const;

	UPROPERTY() FAFLLobbyQueue Queue;
	UPROPERTY() FAFLPayoutLadder Ladder;
	UPROPERTY() int32 StakePerPlayer = 0;
	UPROPERTY() FText Band;
	UPROPERTY() EAFLQueueDetailTab CurrentTab = EAFLQueueDetailTab::Overview;
	UPROPERTY() TArray<TObjectPtr<UAFLW_Lobby_PayoutRow>> SpawnedRungs;
};
