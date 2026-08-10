// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "AFLPlayLimitsSubsystem.generated.h"

/**
 * One currency's standing against the two guardrails. Every figure comes from GET /limits; NOTHING here is
 * computed on the client.
 *
 * ⚠ THE CLIENT DOES NOT DO THE ARITHMETIC, even though it easily could. It knows its own balance, so
 * multiplying by a percentage is one line -- and it must not, for the same reason it does not resolve its
 * own stake band (R59): the number that BINDS is the server's, so the number DISPLAYED has to be the same
 * object or they will drift the day the percentage is re-ruled. A player on a stale build would then be
 * shown a limit that no longer exists and refused against one they never saw, which is precisely the
 * "rejection teaches you the number you wanted" failure `ui-frontend.md` §7 exists to prevent.
 */
USTRUCT(BlueprintType)
struct AFLGAMECORE_API FAFLPlayLimit
{
	GENERATED_BODY()

	/** Held right now, in this currency. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") int64 Balance = 0;

	/** The largest single entry this balance permits. May be BELOW the lowest preset -- see IsEntryPossible. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") int64 EntryCap = 0;

	/** ENFORCED against this: cumulative amount staked inside the rolling window. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") int64 WindowStaked = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") int64 WindowCeiling = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") int64 WindowRemaining = 0;

	/**
	 * DISPLAYED as the meter's meaning: realized net loss in the same window.
	 *
	 * A different quantity from WindowStaked, deliberately (operator ruling, 2026-08-10). "Loss" is what
	 * the guardrail means to a person and it is the word §7 uses; "staked" is the only one of the two that
	 * can bind AT ENTRY, because at entry there is no outcome to have lost yet. S4 shows both and says
	 * which is which rather than pretending they are one number.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") int64 WindowLoss = 0;

	/** True once a real response has populated this. Absent data must never render as a zero limit. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") bool bKnown = false;

	/** Can this player enter a staked queue at all? False when the cap cannot cover the lowest rung. */
	bool IsEntryPossible(int64 LowestPreset) const { return bKnown && EntryCap >= LowestPreset; }
};

/** The whole response. */
USTRUCT(BlueprintType)
struct AFLGAMECORE_API FAFLPlayLimits
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") FAFLPlayLimit Volts;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") FAFLPlayLimit Watts;

	/** Rolling window length, from the server. Shown to the player as "in the last N hours". */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") int32 WindowHours = 0;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") int32 EntryCapPercentOfBalance = 0;

	/** The server says so: every figure is a placeholder pending an operator ruling. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Limits") bool bProvisional = false;

	const FAFLPlayLimit& ForCurrency(bool bVolts) const { return bVolts ? Volts : Watts; }
};

DECLARE_DELEGATE_TwoParams(FAFLOnPlayLimits, bool /*bSuccess*/, const FAFLPlayLimits& /*Limits*/);

/**
 * UAFLPlayLimitsSubsystem -- GET /limits, for S4 TicketReview.
 *
 * ══ WHY THIS EXISTS SEPARATELY FROM THE QUEUE DIRECTORY ═══════════════════════════════════════════════
 *
 * `UAFLQueueDirectorySubsystem` already speaks to the player API and is already the lobby's data source,
 * so folding one more GET into it would have been the shorter diff. The split is deliberate and follows
 * `UAFLPresenceSubsystem`: the directory publishes properties of the SYSTEM -- which queues exist, how busy
 * they are, what band an amount falls in -- and every one of them is the same for every player and served
 * unauthenticated. These are properties of a PERSON: their balance, their exposure, how close they are to
 * a limit. Mixing a per-player authenticated read into a subsystem whose whole contract is "public facts
 * about the ladder" is how a cache written for the public half ends up serving one player another's
 * figures.
 *
 * ⚠ NOT CACHED, AND THAT IS A DECISION. Every S4 opening re-fetches. The window moves, the balance moves,
 * and a stale cap is worse than a slow one: it is a number the player may act on that the server will then
 * refuse. §7's whole claim is that the displayed cap and the enforced cap are the same object.
 */
UCLASS()
class AFLGAMECORE_API UAFLPlayLimitsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UAFLPlayLimitsSubsystem* Get(const UObject* WorldContext);

	/**
	 * Ask the server where this player stands. The callback fires exactly once.
	 *
	 * ⚠ ON FAILURE IT REPORTS FAILURE. It does not hand back zeros, and S4 must not present the screen
	 * without an answer: a cap rendered as 0 reads as "you may stake nothing", and a meter rendered as
	 * 0/0 reads as "no limit". Both are claims about a player's money that we would be inventing. The two
	 * honest outcomes are the real figures or a stated inability to fetch them.
	 */
	void FetchLimits(FAFLOnPlayLimits OnDone);

private:
	static FAFLPlayLimit ParseCurrency(const TSharedPtr<class FJsonObject>& Obj);
};
