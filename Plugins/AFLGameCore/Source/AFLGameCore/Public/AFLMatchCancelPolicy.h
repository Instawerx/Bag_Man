// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "AFLMatchCancelPolicy.generated.h"

UINTERFACE(MinimalAPI)
class UAFLMatchCancelPolicy : public UInterface
{
	GENERATED_BODY()
};

/**
 * IAFLMatchCancelPolicy  (the always-loaded abandonment seam)
 *
 * ONE HUMANLESS WATCH, TWO MODES. The watch used to live inside UAFLRoundManagerComponent, which is in the
 * MATCH PLAY experiences and in neither battle royale one -- so an abandoned staked BR held its pot until the
 * server process tore down and the EndPlay backstop recovered it. That recovers money; it does not notice a
 * problem.
 *
 * The watch now lives on UAFLMatchPhaseComponent, which is resident in BOTH modes, already counts humans for
 * the match-start arrival gate, and already runs its transitions on timers. It cannot reference either mode
 * component concretely -- UAFLRoundManagerComponent and UAFLBattleRoyaleComponent are GameFeature types and
 * the phase component must work when only one of them is loaded -- so it asks through this interface, exactly
 * as AAFLGameMode asks IAFLRoundRestartPolicy. Dependency direction stays GameFeature(implementer) ->
 * always-loaded(interface).
 *
 * ⚠ DUPLICATING THE CLOCK WAS THE ALTERNATIVE AND IT IS THE WRONG ONE. Two accumulators against two copies of
 * a 60s grace drift, and the drift is invisible until a refund fires in one mode and not the other. This is
 * the same consolidation FAFLMatchReporter::AreBotsPermitted got after three copies of one policy disagreed.
 */
class IAFLMatchCancelPolicy
{
	GENERATED_BODY()

public:
	/**
	 * Is there a live match to abandon? True only between match start and match conclusion.
	 *
	 * The watch needs this because "nobody is here" is meaningless before a match exists and after it ends --
	 * and because the accumulator must RESET rather than merely pause across those edges, or a stale count
	 * survives into the next match on a reused component.
	 */
	virtual bool IsMatchLiveForAbandonment() const = 0;

	/**
	 * Every human has gone and stayed gone for the grace. End the match with NO RESULT and refund the pot.
	 *
	 * ⚠ THE ONLY REFUND CASE THERE IS. A single leaver FORFEITS (ruling 2026-08-15) and the match settles
	 * normally around them; refunding one is exploitable, because whoever is behind simply leaves. This fires
	 * only when the field is empty, which nobody can farm.
	 *
	 * The implementer supplies its own reason text and its own terminal bookkeeping -- MATCH PLAY has a round
	 * FSM and a score to stop, BR has a placement ladder -- so the seam carries no reason argument.
	 */
	virtual void ServerCancelAbandoned() = 0;
};
