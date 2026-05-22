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
};
