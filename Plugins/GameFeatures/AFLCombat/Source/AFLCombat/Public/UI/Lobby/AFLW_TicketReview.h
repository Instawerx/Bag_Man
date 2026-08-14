// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Online/AFLPlayLimitsSubsystem.h"

#include "AFLW_TicketReview.generated.h"

class UCommonButtonBase;
class UCommonTextBlock;
class UProgressBar;

/**
 * UAFLW_TicketReview -- S4, the last screen before currency moves.
 *
 * `IRONICS_LOBBY_UX_HANDOFF.md` §6 draws it; R22 makes it unskippable; R23 is why it exists at all.
 *
 * ══ THIS SCREEN IS A GUARDRAIL, NOT A CONFIRMATION DIALOG ═════════════════════════════════════════════
 *
 * The tempting reading is "are you sure?", and that reading produces a useless screen -- players learn to
 * dismiss a confirm step in about three uses. §6's actual content is four facts a player cannot get
 * anywhere else at the moment they need them:
 *
 *     what they are entering        BATTLE ROYALE · Duo · 450 V, matching 400-500
 *     what it costs                 balance 12,480 -> 12,030
 *     where they stand in a window  session meter, VISIBLE BEFORE IT BINDS
 *     the range they have           cap this entry: 1,248 V max
 *
 * `ui-frontend.md` §7 is explicit about the last two: "A rejection teaches the player the number they
 * wanted; a visible cap frames the range they have", and a session limit "that only announces itself when
 * it triggers arrives as a punishment rather than a boundary". Both are the same design decision as the
 * payout ladder they sit beside -- showing potential winnings next to a stake control is an interface
 * designed to increase stakes, and R23 refuses to let that ship without its counterweight.
 *
 * ══ WHY EVERY NUMBER IS FETCHED AND NONE IS COMPUTED ══════════════════════════════════════════════════
 *
 * The cap is a percentage of a balance this client already knows, so it could do the multiplication. It
 * must not, for the same reason it does not snap its own stake band (R59): the number that BINDS lives in
 * `/create-ticket`, so the number DISPLAYED has to be the same object. Two implementations of one rule is
 * how a player gets shown one limit and refused against another -- which is the exact failure §7 names.
 *
 * ⚠ CONFIRM IS DISABLED UNTIL THE LIMITS ARRIVE, and stays disabled if they cannot be read. That is not
 * defensive coding, it is the rule: a commit taken without the guardrails on screen is the skip R22
 * forbids, and it would be indistinguishable to the player from one taken with them.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_TicketReview : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/**
	 * The entry under review. Called by the lobby immediately after this widget is pushed.
	 *
	 * `QueueId` and `TypedStake` are exactly what `UAFLW_Lobby_Root::CommitQueue` broadcast -- S4 re-reads
	 * neither from a cache nor from its own state, so what is confirmed is what was chosen.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|S4")
	void SetTicket(const FString& InQueueId, int64 InTypedStake, bool bInVolts,
	               const FText& InQueueLabel, const FText& InBandLabel);

	/** Fires when the player commits and the entry has been handed to matchmaking. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAFLTicketConfirmed, const FString&, QueueId, int64, Stake);
	UPROPERTY(BlueprintAssignable, Category = "AFL|S4")
	FAFLTicketConfirmed OnTicketConfirmed;

	/** Fires when the player backs out. The lobby is still behind this screen; nothing was spent. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAFLTicketCancelled);
	UPROPERTY(BlueprintAssignable, Category = "AFL|S4")
	FAFLTicketCancelled OnTicketCancelled;

	/** Commit. Public because it is the screen's verb and the `afl.Lobby.Ticket` probe drives it. */
	UFUNCTION(BlueprintCallable, Category = "AFL|S4")
	void Confirm();

	UFUNCTION(BlueprintCallable, Category = "AFL|S4")
	void Cancel();

	/**
	 * One line of state for the `afl.Lobby.Ticket` probe: which ticket, at what stake, whether the limits
	 * arrived, and whether the entry passes them. The last two are DIFFERENT failures and the string says so
	 * -- `limits UNKNOWN` means /limits never answered, `entry REFUSED` means it did and the answer was no.
	 * Reading a disabled Confirm button cannot tell those apart, and they send an investigator to opposite
	 * halves of the system.
	 */
	FString DescribeTicket() const;

	/** True when the limits are known AND this entry passes both of them. Mirrors the server's evaluator. */
	UFUNCTION(BlueprintPure, Category = "AFL|S4")
	bool IsEntryPermitted() const;

	/**
	 * THE CLIENT-SIDE MIRROR OF THE SERVER'S RULE, as a static so a test can hold it without a widget.
	 *
	 * ⚠ THIS IS A DISPLAY AID AND NOT THE ENFORCEMENT. `/create-ticket` decides; this exists so the CTA can
	 * be disabled with a stated reason instead of letting the player press it and meet a 409. If the two
	 * ever disagree the server wins and the player sees a refusal -- which is why neither side owns a
	 * private copy of the numbers: both read them from `/limits`.
	 *
	 * Reasons are ordered as the server orders them: the entry cap first, because it is the one a player
	 * can act on right now by staking less.
	 */
	static bool EvaluateEntry(const FAFLPlayLimit& Limit, int64 Stake, FText& OutRefusal);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Blueprint hook for §6's meter treatment -- `Glass.Tint.Danger` as it approaches the ceiling. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|S4", meta = (DisplayName = "On Limits Changed"))
	void BP_OnLimitsChanged(const FAFLPlayLimit& Limit, bool bPermitted);

	// -- §6's content, all BindWidgetOptional so a WBP can stage the screen incrementally. The two BUTTONS
	//    are required: a review screen you cannot act on is a trap, not a review.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|S4")
	TObjectPtr<UCommonButtonBase> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|S4")
	TObjectPtr<UCommonButtonBase> CancelButton;

	/** "BATTLE ROYALE · Duo" and "matching 400-500 V" -- what is being entered, and on what terms. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|S4")
	TObjectPtr<UCommonTextBlock> QueueLine;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|S4")
	TObjectPtr<UCommonTextBlock> BandLine;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|S4")
	TObjectPtr<UCommonTextBlock> StakeValue;

	/** "12,480 V  →  12,030 V". One line, because the arrow IS the information. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|S4")
	TObjectPtr<UCommonTextBlock> BalanceValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|S4")
	TObjectPtr<UCommonTextBlock> SessionMeterLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|S4")
	TObjectPtr<UProgressBar> SessionMeter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|S4")
	TObjectPtr<UCommonTextBlock> EntryCapLabel;

	/** Why CONFIRM is unavailable. Empty and collapsed when it is available. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|S4")
	TObjectPtr<UCommonTextBlock> RefusalLine;

private:
	void RequestLimits();
	void ApplyLimits();
	void SetBusy(bool bInBusy);

	/** The entry, exactly as the lobby handed it over. */
	FString QueueId;
	int64   TypedStake = 0;
	bool    bVolts = true;
	FText   QueueLabel;
	FText   BandLabel;

	FAFLPlayLimits Limits;
	bool bLimitsKnown = false;

	/** True from the moment CONFIRM is pressed. Stops a double-press becoming two tickets. */
	bool bCommitting = false;
};
