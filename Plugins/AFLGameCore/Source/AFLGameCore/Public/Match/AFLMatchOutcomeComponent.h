// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "AFLMatchOutcomeComponent.generated.h"

class APlayerState;

/**
 * FAFLPlayerOutcome -- what a match COST and PAID a single player, once the backend has answered.
 *
 * Keyed by PlayerState rather than PlayFab id ON PURPOSE. The reconcile id lives on a server-side
 * UAFLReconcileIdComponent and is not replicated, so a client handed a PlayFab id could not turn it back into
 * a player. An actor reference replicates and resolves on both sides, so the server does the id->player
 * lookup once, while it still has the information to do it correctly.
 */
USTRUCT()
struct FAFLPlayerOutcome
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<APlayerState> Player = nullptr;

	/** What this player put in escrow (integer currency -- E1). 0 in an unstaked match. */
	UPROPERTY() int32 Stake = 0;

	/** What settlement paid back. 0 for a loss; == Stake for a cancelled-refund; > Stake for a win. */
	UPROPERTY() int32 Payout = 0;

	/** displayDelta from the rating service. Signed -- a loss is negative. */
	UPROPERTY() float RatingDelta = 0.f;

	/** Set only when the corresponding service ANSWERED. Absent != zero (see the component note). */
	UPROPERTY() bool bHasSettle = false;
	UPROPERTY() bool bHasRating = false;
};

/**
 * UAFLMatchOutcomeComponent  (GameState) -- carries the ECONOMY result of a match to clients.
 *
 * WHY THIS EXISTS. Settlement and rating resolve entirely server-side in FAFLMatchReporter and were never
 * sent anywhere. A client could see who won a round but had no way to learn what the match paid, so the
 * results board could show a score and nothing about the stake it was played for.
 *
 * ⚠ THE TIMING IS THE WHOLE DESIGN. These are HTTP round-trips that resolve AFTER the match ends, and the
 * results board is already on screen by then. Measured, 2026-08-09:
 *
 *     19:51:46.950  MATCH END        -> board pushed
 *     19:51:48.889  rating OK        (+1.9s)
 *     19:51:49.757  settle OK        (+2.8s)
 *
 * So this is NOT a render-once payload. The board binds OnOutcomesChanged and repaints when each answer
 * lands. Anything that reads this component the instant the match ends will correctly find nothing.
 *
 * That is also why bHasSettle/bHasRating exist rather than treating 0 as "no payout". A losing player is
 * genuinely paid 0, and "lost your stake" must not be indistinguishable from "the settle call has not come
 * back yet" -- one is a result, the other is a pending state, and showing the first while the second is true
 * would tell a player they lost money before anyone knows whether they did.
 *
 * Created server-side on demand (EnsureOn) rather than through the experience's AddComponents, so no asset
 * edit is required to land it. It is a replicated component on the GameState, which is always relevant.
 */
UCLASS()
class AFLGAMECORE_API UAFLMatchOutcomeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLMatchOutcomeComponent();

	/** Find the GameState's component, creating it SERVER-SIDE if absent. Returns null on a client. */
	static UAFLMatchOutcomeComponent* EnsureOn(const UObject* WorldContext);

	/** Read-only find -- never creates. The client path. */
	static UAFLMatchOutcomeComponent* Find(const UObject* WorldContext);

	/** Server: record what each player staked, at escrow time (before any result exists). */
	void ServerRecordStake(APlayerState* Player, int32 InStake);

	/** Server: settlement answered -- what this player was paid. */
	void ServerRecordPayout(APlayerState* Player, int32 InPayout);

	/** Server: rating answered -- this player's signed displayDelta. */
	void ServerRecordRatingDelta(APlayerState* Player, float InDelta);

	/** The local view. Returns null if this player has no recorded outcome yet. */
	const FAFLPlayerOutcome* FindOutcome(const APlayerState* Player) const;

	/** Fires on the server after any Server* write and on clients from OnRep. Rebind-safe. */
	FSimpleMulticastDelegate OnOutcomesChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION() void OnRep_Outcomes();

	/** Server-only: the entry for this player, created if absent. */
	FAFLPlayerOutcome& FindOrAddMutable(APlayerState* Player);

	UPROPERTY(ReplicatedUsing = OnRep_Outcomes)
	TArray<FAFLPlayerOutcome> Outcomes;
};
