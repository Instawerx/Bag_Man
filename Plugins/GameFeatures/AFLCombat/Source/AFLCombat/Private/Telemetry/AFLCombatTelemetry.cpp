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
