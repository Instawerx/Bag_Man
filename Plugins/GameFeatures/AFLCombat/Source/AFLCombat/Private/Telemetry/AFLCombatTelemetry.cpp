// Copyright C12 AI Gaming. All Rights Reserved.

#include "Telemetry/AFLCombatTelemetry.h"

#include "AFLCombat.h"
#include "GameFramework/Actor.h"


// All three methods funnel through a single UE_LOG site so the AFL-1307
// PlayFab swap touches one function body, not three. Stable line prefix is
// `AFL_TELEMETRY: <event>` — log scrapers (and the AFL-0215 cheat matrix)
// grep on that prefix.

void FAFLCombatTelemetry::EmitRejection(const FName& Reason, const AActor* Source, const FString& Detail)
{
	const FString SourceName = GetNameSafe(Source);

	if (Detail.IsEmpty())
	{
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_TELEMETRY: hitscan_reject reason=%s source=%s"),
			*Reason.ToString(),
			*SourceName);
	}
	else
	{
		UE_LOG(LogAFLCombat, Log,
			TEXT("AFL_TELEMETRY: hitscan_reject reason=%s source=%s %s"),
			*Reason.ToString(),
			*SourceName,
			*Detail);
	}
}

void FAFLCombatTelemetry::EmitAngularAnomaly(const AActor* Source, float AngularVelocityDegPerSec, float BudgetDegPerSec)
{
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_TELEMETRY: ang_anomaly source=%s ang=%.1f budget=%.1f"),
		*GetNameSafe(Source),
		AngularVelocityDegPerSec,
		BudgetDegPerSec);
}

void FAFLCombatTelemetry::EmitHeadshotRatio(const AActor* Source, int32 Headshots, int32 TotalHits)
{
	// Guard divide-by-zero. Zero hits is a legitimate input (a fresh pawn that
	// hasn't fired yet); we report ratio=0.0 rather than dropping the sample
	// so downstream aggregation sees an explicit zero data point.
	const float Ratio = (TotalHits > 0)
		? (static_cast<float>(Headshots) / static_cast<float>(TotalHits))
		: 0.0f;

	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_TELEMETRY: headshot_ratio source=%s headshots=%d hits=%d ratio=%.3f"),
		*GetNameSafe(Source),
		Headshots,
		TotalHits,
		Ratio);
}

// --- Round / extraction / spatial events (ADDITIVE; same sink + prefix as the three above) ---

void FAFLCombatTelemetry::EmitRoundStart(int32 Round)
{
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_TELEMETRY: afl_round_start round=%d"),
		Round);
}

void FAFLCombatTelemetry::EmitRoundResolved(int32 Round, int32 WinningTeam, const FName& Reason)
{
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_TELEMETRY: afl_round_resolved round=%d team=%d reason=%s"),
		Round,
		WinningTeam,
		*Reason.ToString());
}

void FAFLCombatTelemetry::EmitExtractContest(const AActor* Channeler, bool bContested, const FVector& Location)
{
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_TELEMETRY: afl_extract_contest source=%s contested=%d x=%.0f y=%.0f z=%.0f"),
		*GetNameSafe(Channeler),
		bContested ? 1 : 0,
		Location.X, Location.Y, Location.Z);
}

void FAFLCombatTelemetry::EmitExtractOutcome(const AActor* Channeler, int32 TeamId, bool bSuccess, const FVector& Location)
{
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_TELEMETRY: afl_extract_outcome source=%s team=%d success=%d x=%.0f y=%.0f z=%.0f"),
		*GetNameSafe(Channeler),
		TeamId,
		bSuccess ? 1 : 0,
		Location.X, Location.Y, Location.Z);
}

void FAFLCombatTelemetry::EmitElimination(const AActor* Victim, const AActor* Killer, int32 VictimTeam, const FVector& Location)
{
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_TELEMETRY: afl_elimination victim=%s killer=%s team=%d x=%.0f y=%.0f z=%.0f"),
		*GetNameSafe(Victim),
		*GetNameSafe(Killer),
		VictimTeam,
		Location.X, Location.Y, Location.Z);
}

void FAFLCombatTelemetry::EmitTraverse(const AActor* Pawn, int32 TeamId, const FVector& Location)
{
	// AFL-0213 / s6 traversal-density source: a periodic per-LIVING-pawn position sample. The round manager
	// throttles the cadence + iterates living pawns; this is the pure log sink (same prefix as every emit).
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_TELEMETRY: afl_traverse pawn=%s team=%d x=%.0f y=%.0f z=%.0f"),
		*GetNameSafe(Pawn),
		TeamId,
		Location.X, Location.Y, Location.Z);
}
