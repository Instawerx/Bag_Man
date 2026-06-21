// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;


/**
 * FAFLCombatTelemetry
 *
 * AFL-0213 telemetry sink for combat events. Three semantic event families:
 *
 *   * Rejection       — server rejected a client-built target-data payload
 *                       (schema, geometry, lag-comp re-trace). Today only
 *                       schema rejects are wired; AFL-0211 fills in the rest.
 *   * AngularAnomaly  — claimed aim angular velocity exceeded the per-pawn
 *                       budget. Today logs only; AFL-0211 will additionally
 *                       discard the shot.
 *   * HeadshotRatio   — per-pawn running headshot/total-hit ratio. Today the
 *                       caller computes and passes the ratio; centralised
 *                       aggregation lands when PlayFab streams arrive.
 *
 * Sink: every method writes a single UE_LOG(LogAFLCombat, Log) line in the
 * stable parseable form
 *
 *     AFL_TELEMETRY: <event> key1=val1 key2=val2 ...
 *
 * S13 / AFL-1307 will swap the UE_LOG sink for a PlayFab Player Streams write
 * inside these methods *without touching call sites*. That's the whole point
 * of the class: every combat telemetry path funnels through these three
 * static methods, so the PlayFab cutover is a one-file change.
 *
 * Pure static utility — not a UObject. No state, no engine coupling beyond
 * the log category. Safe to call from server, client, or shared paths; the
 * caller is responsible for guarding with WITH_SERVER_CODE when the event
 * is server-only (e.g. rejections from UAFLDamageExecCalc).
 */
class AFLCOMBAT_API FAFLCombatTelemetry
{
public:

	/**
	 * Server rejected a client-built target-data payload.
	 *
	 *   Reason  — short stable token: "schema", "geom", "lagcomp", "ang".
	 *             Treat as an enum-by-string until AFL-0211 promotes it.
	 *   Source  — instigating actor (the firing pawn). May be null.
	 *   Detail  — free-form key=value tail appended verbatim. Empty by default.
	 *
	 * Emits:
	 *   AFL_TELEMETRY: hitscan_reject reason=<Reason> source=<SourceName> <Detail>
	 */
	static void EmitRejection(const FName& Reason, const AActor* Source, const FString& Detail = FString());

	/**
	 * Claimed aim angular velocity exceeded the per-pawn budget. The shot is
	 * NOT discarded by this call — the lag-comp re-trace owns that decision
	 * (AFL-0211). This event is a separate family from EmitRejection because
	 * it is informational today and the rate is what we want to graph.
	 *
	 * Emits:
	 *   AFL_TELEMETRY: ang_anomaly source=<SourceName> ang=<value> budget=<value>
	 */
	static void EmitAngularAnomaly(const AActor* Source, float AngularVelocityDegPerSec, float BudgetDegPerSec);

	/**
	 * Per-pawn headshot ratio sample. Caller computes the ratio from its own
	 * running counters; we just log it. Centralised aggregation arrives with
	 * PlayFab streams (AFL-1307).
	 *
	 * Emits:
	 *   AFL_TELEMETRY: headshot_ratio source=<SourceName> headshots=<H> hits=<N> ratio=<R>
	 */
	static void EmitHeadshotRatio(const AActor* Source, int32 Headshots, int32 TotalHits);

	// ---------------------------------------------------------------------------------------------
	// Round / extraction / spatial events (the Arena round wrapper + Task-2 per-level z-heatmaps).
	// ADDITIVE -- the three events above are unchanged in meaning. Same UE_LOG sink + line prefix.
	// ---------------------------------------------------------------------------------------------

	/** Round began. Emits: AFL_TELEMETRY: afl_round_start round=<R> */
	static void EmitRoundStart(int32 Round);

	/** Round resolved. Emits: AFL_TELEMETRY: afl_round_resolved round=<R> team=<T> reason=<Reason> */
	static void EmitRoundResolved(int32 Round, int32 WinningTeam, const FName& Reason);

	/** Was an extraction contested (a live enemy near the bank point). World-Z carried for per-level heatmaps.
	 *  Emits: AFL_TELEMETRY: afl_extract_contest source=<S> contested=<0|1> x=<X> y=<Y> z=<Z> */
	static void EmitExtractContest(const AActor* Channeler, bool bContested, const FVector& Location);

	/** Extraction outcome (success/fail) by team, with world-Z.
	 *  Emits: AFL_TELEMETRY: afl_extract_outcome source=<S> team=<T> success=<0|1> x=<X> y=<Y> z=<Z> */
	static void EmitExtractOutcome(const AActor* Channeler, int32 TeamId, bool bSuccess, const FVector& Location);

	/** A player elimination, with world-Z (the kill/death spatial datum Task-2 per-level heatmaps need --
	 *  there was no prior kill/death emit; this adds one WITH Z, without touching the combat/damage code).
	 *  Emits: AFL_TELEMETRY: afl_elimination victim=<V> killer=<K> team=<T> x=<X> y=<Y> z=<Z> */
	static void EmitElimination(const AActor* Victim, const AActor* Killer, int32 VictimTeam, const FVector& Location);
};
