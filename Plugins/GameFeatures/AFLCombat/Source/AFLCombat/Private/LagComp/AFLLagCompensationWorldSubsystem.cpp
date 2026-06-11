// Copyright C12 AI Gaming. All Rights Reserved.

#include "LagComp/AFLLagCompensationWorldSubsystem.h"

#include "AFLCombat.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/StringBuilder.h"
#include "GameFramework/PlayerController.h"
#include "LagComp/AFLPawnHitboxHistoryComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLagCompensationWorldSubsystem)

// ─── Cycle-3 sweep diagnostic (instrument-then-fix) ─────────────────────────────────────────────
// Observe-only: when enabled, every ConfirmHit ALSO evaluates candidate rewind depths and logs one
// AFL_LAGCOMP_SWEEP line per shot. The REAL verdict path is untouched -- the passed delta (today:
// FULL RTT at the Pulse call site) still decides accept/reject. Under test: the call-site divergence
// from this subsystem's own documented formula (RewindWorldFor's contract + master doc Sec. 7.4 both
// say ClampedRTT = min(RTT/2 + interp, 0.2)). The sweep names the winning dt EMPIRICALLY before any
// formula change. Tokens are non-mutating + re-entrant (header contract), so N rewinds per confirm
// are structurally safe; cost ~= N * (3 components x <=8 bone lerps) -- well under the 200us budget.
static TAutoConsoleVariable<int32> CVarAFLLagCompSweepDiagnostic(
	TEXT("afl.LagComp.SweepDiagnostic"),
	0,
	TEXT("1 = log an AFL_LAGCOMP_SWEEP line per ConfirmHit evaluating candidate rewind depths {0, RTT/4, RTT/2, RTT/2+interp, RTT}. Observe-only; the real verdict is unchanged."));

static TAutoConsoleVariable<float> CVarAFLLagCompInterpEstimateMs(
	TEXT("afl.LagComp.InterpEstimateMs"),
	30.0f,
	TEXT("Estimated client view staleness (ms) beyond one-way transit, used by the sweep's RTT/2+interp candidate. Calibrated by the sweep itself (the miss-cm gradient)."));


bool FAFLLagRewindToken::QueryBoneTransform(const AActor* Target, FName BoneName, FTransform& OutWorldXForm) const
{
	if (!bValid || !Target)
	{
		return false;
	}
	for (const FAFLRewindPawnEntry& Entry : Entries)
	{
		if (Entry.Pawn.Get() != Target)
		{
			continue;
		}
		for (const FAFLHitboxBoneSample& Sample : Entry.RewoundBones)
		{
			if (Sample.Bone == BoneName)
			{
				OutWorldXForm = Sample.WorldXForm;
				return true;
			}
		}
		return false;
	}
	return false;
}

bool FAFLLagRewindToken::BuildBoundingBox(const AActor* Target, FBox& OutBox) const
{
	if (!bValid || !Target)
	{
		return false;
	}
	for (const FAFLRewindPawnEntry& Entry : Entries)
	{
		if (Entry.Pawn.Get() != Target)
		{
			continue;
		}
		if (Entry.RewoundBones.Num() == 0)
		{
			return false;
		}
		// Build from bone world locations. The bounding-box check in master
		// doc Sec. 7.4 expands this by a fixed 30cm pad for rig accuracy; we
		// leave the pad to the caller so the box stays a faithful snapshot
		// of the rewound rig.
		OutBox = FBox(ForceInit);
		for (const FAFLHitboxBoneSample& Sample : Entry.RewoundBones)
		{
			OutBox += Sample.WorldXForm.GetLocation();
		}
		return OutBox.IsValid != 0;
	}
	return false;
}


UAFLLagCompensationWorldSubsystem::UAFLLagCompensationWorldSubsystem() = default;

bool UAFLLagCompensationWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	if (!World)
	{
		return false;
	}
	// Editor / preview / inactive worlds never simulate pawns, so the
	// registry is wasted memory there.
	switch (World->WorldType)
	{
		case EWorldType::Game:
		case EWorldType::PIE:
			return true;
		default:
			return false;
	}
}

void UAFLLagCompensationWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogAFLCombat, Verbose, TEXT("AFL_LAGCOMP: subsystem initialized for world %s"),
		*GetNameSafe(GetWorld()));
}

void UAFLLagCompensationWorldSubsystem::Deinitialize()
{
	RegisteredComponents.Reset();
	Super::Deinitialize();
}

void UAFLLagCompensationWorldSubsystem::RegisterComponent(UAFLPawnHitboxHistoryComponent* Component)
{
	if (!Component)
	{
		return;
	}
	for (const TWeakObjectPtr<UAFLPawnHitboxHistoryComponent>& Existing : RegisteredComponents)
	{
		if (Existing.Get() == Component)
		{
			return;
		}
	}
	RegisteredComponents.Add(Component);
}

void UAFLLagCompensationWorldSubsystem::UnregisterComponent(UAFLPawnHitboxHistoryComponent* Component)
{
	if (!Component)
	{
		return;
	}
	RegisteredComponents.RemoveAllSwap(
		[Component](const TWeakObjectPtr<UAFLPawnHitboxHistoryComponent>& Existing)
		{
			return !Existing.IsValid() || Existing.Get() == Component;
		});
}

FAFLLagRewindToken UAFLLagCompensationWorldSubsystem::RewindWorldFor(APlayerController* RequestingPC, float ServerTime)
{
	FAFLLagRewindToken Token;
	Token.bValid = true;
	Token.SampledServerTime = static_cast<double>(ServerTime);

	// Hard server gate. Lag comp is server-only — returning a valid-but-empty
	// token lets shared code call this without an IsServer guard and still
	// scope-guard the matching RestoreWorld.
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return Token;
	}

	const APawn* ExcludedPawn = RequestingPC ? RequestingPC->GetPawn() : nullptr;

	Token.Entries.Reserve(RegisteredComponents.Num());

	// Sweep dangling weak ptrs as we go to keep the registry tight.
	for (int32 Idx = RegisteredComponents.Num() - 1; Idx >= 0; --Idx)
	{
		UAFLPawnHitboxHistoryComponent* Component = RegisteredComponents[Idx].Get();
		if (!Component)
		{
			RegisteredComponents.RemoveAtSwap(Idx);
			continue;
		}

		const AActor* Owner = Component->GetOwner();
		if (!Owner || Owner == ExcludedPawn)
		{
			continue;
		}

		TArray<FAFLHitboxBoneSample> Samples;
		if (!Component->SampleAtTime(static_cast<double>(ServerTime), Samples))
		{
			// No history yet (just spawned) or component is on a client.
			continue;
		}

		FAFLRewindPawnEntry Entry;
		Entry.Pawn          = Owner;
		Entry.Component     = Component;
		Entry.RewoundBones  = MoveTemp(Samples);
		Token.Entries.Add(MoveTemp(Entry));
	}

	return Token;
}

void UAFLLagCompensationWorldSubsystem::RestoreWorld(FAFLLagRewindToken& Token)
{
	// AFL-0211 rewind is non-mutating, so this does not touch the live world.
	// Invalidating the token catches double-restores and keeps the scope-
	// guard pattern meaningful. A future mutating-rewind impl can drop into
	// this entry point without changing call sites.
	if (!Token.bValid)
	{
		return;
	}
	Token.Entries.Reset();
	Token.bValid = false;
}

bool UAFLLagCompensationWorldSubsystem::ConfirmHit(APlayerController* RequestingPC, float RewindDeltaSeconds,
	const AActor* TargetActor, const FVector& ImpactPoint)
{
	const UWorld* World = GetWorld();
	const float RewindTime = (World ? static_cast<float>(World->GetTimeSeconds()) : 0.0f) - RewindDeltaSeconds;

	FAFLLagRewindToken Token = RewindWorldFor(RequestingPC, RewindTime);

	// Empty-token default-accept: a degenerate rewind (client world, no
	// registered components, no history yet) yields no entries, and the live
	// path must not be gated by missing snapshots — so we accept. The geometric
	// reject only fires when there is a real rewound box to test against.
	bool bGeometricallyValid = true;
	FBox RewoundBox(ForceInit);
	if (Token.Entries.Num() > 0 && Token.BuildBoundingBox(TargetActor, RewoundBox))
	{
		// 30cm rig-accuracy pad per master doc Sec. 7.4 (BuildBoundingBox returns
		// a tight bone-derived box; the pad lives at the confirm site per the
		// header's contract).
		const FBox PaddedBox = RewoundBox.ExpandBy(30.0f);
		bGeometricallyValid = PaddedBox.IsInsideOrOn(ImpactPoint);
	}

	// 2-client watch instrumentation (watch a): this line IS the lag-comp proof -- the host's own
	// shot rewinds dt=0 (~0 real latency to itself) while a real client's shot rewinds a non-zero
	// dt. That contrast is unreadable unless the line says WHICH instance fired, so stamp the
	// requesting pawn's name (C_0 host vs C_1 client) + net role. Promoted Verbose->Log so the dt
	// shows in a plain log -- the watch must not depend on a verbose flag being on (the first c0
	// run only saw this line because verbose happened to be enabled).
	const APawn* RequestingPawn = RequestingPC ? RequestingPC->GetPawn() : nullptr;
	UE_LOG(LogAFLCombat, Log,
		TEXT("AFL_LAGCOMP: rewind dt=%.3f entries=%d verdict=%s by %s (role=%d)"),
		RewindDeltaSeconds, Token.Entries.Num(),
		bGeometricallyValid ? TEXT("accept") : TEXT("reject"),
		*GetNameSafe(RequestingPawn),
		RequestingPawn ? static_cast<int32>(RequestingPawn->GetLocalRole()) : 0);

	RestoreWorld(Token);

	// Cycle-3 multi-dt sweep (cvar-gated, observe-only -- see the cvar comment at file top). Candidates
	// derive from the PASSED delta, which at today's Pulse call site is full RTT -- the quantity whose
	// correctness is under test.
	if (CVarAFLLagCompSweepDiagnostic.GetValueOnGameThread() != 0 && World)
	{
		const float InterpS = CVarAFLLagCompInterpEstimateMs.GetValueOnGameThread() * 0.001f;
		const float RTT = RewindDeltaSeconds;
		const float Candidates[5] = { 0.0f, RTT * 0.25f, RTT * 0.5f, RTT * 0.5f + InterpS, RTT };

		static int32 GSweepShotCounter = 0;
		++GSweepShotCounter;

		TStringBuilder<512> Line;
		Line.Appendf(TEXT("AFL_LAGCOMP_SWEEP shot=%d claimed=(%.0f, %.0f, %.0f) rtt=%.3f"),
			GSweepShotCounter, ImpactPoint.X, ImpactPoint.Y, ImpactPoint.Z, RTT);

		for (const float CandDt : Candidates)
		{
			FAFLLagRewindToken CandToken = RewindWorldFor(RequestingPC,
				static_cast<float>(World->GetTimeSeconds()) - CandDt);

			// Mirror the real path exactly: tight bone box -> 30cm pad -> point test; empty-token
			// default-accept (miss reads -1 = no data, distinguishable from a true inside-hit 0).
			bool bCandHit = true;
			float MissCm = -1.0f;
			FBox CandBox(ForceInit);
			if (CandToken.Entries.Num() > 0 && CandToken.BuildBoundingBox(TargetActor, CandBox))
			{
				const FBox Padded = CandBox.ExpandBy(30.0f);
				bCandHit = Padded.IsInsideOrOn(ImpactPoint);
				MissCm = bCandHit ? 0.0f : FMath::Sqrt(static_cast<float>(Padded.ComputeSquaredDistanceToPoint(ImpactPoint)));
			}
			RestoreWorld(CandToken);

			Line.Appendf(TEXT(" | dt=%.3f miss=%.1f hit=%d"), CandDt, MissCm, bCandHit ? 1 : 0);
		}

		UE_LOG(LogAFLCombat, Log, TEXT("%s"), Line.ToString());
	}

	return bGeometricallyValid;
}
