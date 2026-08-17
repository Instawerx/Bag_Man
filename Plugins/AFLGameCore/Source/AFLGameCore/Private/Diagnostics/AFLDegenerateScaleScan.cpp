// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLGameCore.h"                        // LogAFLGameCore
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"                        // TActorIterator
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"                       // the re-scan timer (a one-shot scan reports a false all-clear)

/**
 * afl.Diag.FindDegenerateScale -- name the primitive that is NaN-ing the renderer.
 *
 * WHY THIS EXISTS. A cooked client logs thousands of these on the RENDER thread:
 *
 *   TMatrix<T>::InverseFast(), trying to invert a NIL matrix, this results in NaNs!
 *     FDistanceFieldSceneData::UpdateDistanceFieldObjectBuffers  [DistanceFieldObjectManagement.cpp:625]
 *     PrepareDistanceFieldScene -> FDeferredShadingSceneRenderer::Render -> RenderingThreadMain
 *
 * Line 625 inverts a distance-field primitive's LocalToWorld. A NIL matrix there means SOME PRIMITIVE IN THE
 * SCENE HAS A ZERO-SCALE TRANSFORM and has distance fields generated. The engine reports the maths failure
 * but never names the object, which is why this was invisible: the log says a matrix was bad, not which
 * actor owns it.
 *
 * ⚠ THE COST IS NOT THE NaN, IT IS THE ENSURE. ErrorEnsure (Matrix.h:469) calls ensureMsgf, and a single
 * stack walk was MEASURED at 9.1 s -- on the render thread. That is the multi-second freeze that reads as a
 * frozen/T-posed character. The animation is fine; the renderer is stalled mid-frame.
 *
 * ⚠ SCANS ISM/HISM PER-INSTANCE, and that matters more than the component sweep. A component with sane scale
 * can still hold instances scaled to zero -- "hiding" an instance by zeroing its scale is a common authoring
 * shortcut, and distance fields do not care that it was meant to be invisible. A component-only scan would
 * report nothing and look like a clean bill of health.
 */
static void AFLFindDegenerateScale(UWorld* World)
{
	if (!World)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_DIAG: no world."));
		return;
	}

	// Matches the engine's own threshold at the failure site: InverseFast ensures when the scaled axes are
	// near-zero, so anything below this is what the renderer will choke on.
	constexpr float Threshold = UE_SMALL_NUMBER;

	int32 Scanned = 0;
	int32 Found = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		TArray<UPrimitiveComponent*> Prims;
		Actor->GetComponents<UPrimitiveComponent>(Prims);

		for (UPrimitiveComponent* Prim : Prims)
		{
			if (!Prim)
			{
				continue;
			}
			++Scanned;

			const FVector Scale = Prim->GetComponentScale();
			const bool bDegenerate =
				FMath::Abs(Scale.X) <= Threshold || FMath::Abs(Scale.Y) <= Threshold || FMath::Abs(Scale.Z) <= Threshold;

			if (bDegenerate)
			{
				++Found;
				UE_LOG(LogAFLGameCore, Error,
					TEXT("AFL_DIAG: DEGENERATE COMPONENT  actor='%s'  comp='%s'  scale=(%.6f, %.6f, %.6f)  affectsDF=%d"),
					*Actor->GetName(), *Prim->GetName(), Scale.X, Scale.Y, Scale.Z,
					Prim->bAffectDistanceFieldLighting ? 1 : 0);
			}

			// Per-instance sweep. An ISM's own transform can be perfectly valid while an instance inside it
			// is zeroed -- and each instance is its own distance-field object.
			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Prim))
			{
				const int32 Count = ISM->GetInstanceCount();
				for (int32 i = 0; i < Count; ++i)
				{
					FTransform Inst;
					if (!ISM->GetInstanceTransform(i, Inst, /*bWorldSpace=*/true))
					{
						continue;
					}
					const FVector IS = Inst.GetScale3D();
					if (FMath::Abs(IS.X) <= Threshold || FMath::Abs(IS.Y) <= Threshold || FMath::Abs(IS.Z) <= Threshold)
					{
						++Found;
						UE_LOG(LogAFLGameCore, Error,
							TEXT("AFL_DIAG: DEGENERATE ISM INSTANCE  actor='%s'  comp='%s'  instance=%d/%d  "
							     "scale=(%.6f, %.6f, %.6f)  affectsDF=%d"),
							*Actor->GetName(), *Prim->GetName(), i, Count, IS.X, IS.Y, IS.Z,
							Prim->bAffectDistanceFieldLighting ? 1 : 0);
					}
				}
			}
		}
	}

	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_DIAG: scanned %d primitive(s) -- %d degenerate."), Scanned, Found);
}

/**
 * Re-scan on a timer until something is found or the budget is spent.
 *
 * ⚠ WHY THIS IS NOT OPTIONAL POLISH. The first run of this tool reported "324 primitives, 0 degenerate" and
 * looked like a clean bill of health -- but it executed on FRAME 1, via -ExecCmds, before the level had
 * finished populating. 324 is a fraction of an art-passed arena. A one-shot scan does not merely miss the
 * offender, it actively asserts the opposite of the truth, which is worse than no tool at all.
 *
 * Logging the primitive count every attempt makes the world filling in VISIBLE, so a low count reads as
 * "too early" instead of "nothing there".
 */
static void AFLScanWithRetry(UWorld* World, TSharedPtr<int32> Attempt)
{
	if (!World || !Attempt.IsValid())
	{
		return;
	}

	constexpr int32 MaxAttempts = 12;   // ~24s at 2s spacing -- past level streaming and first spawns
	++(*Attempt);

	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_DIAG: --- attempt %d/%d ---"), *Attempt, MaxAttempts);
	AFLFindDegenerateScale(World);

	if (*Attempt >= MaxAttempts)
	{
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_DIAG: budget spent with nothing degenerate found. The renderer's NaN is real, so the "
			     "offender is either SPAWNED mid-round (weapon, gib, VFX) or lives in a streamed level that "
			     "was never loaded here -- re-run during an actual round."));
		return;
	}

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateLambda([World, Attempt]() { AFLScanWithRetry(World, Attempt); }),
		2.0f, /*bLoop=*/false);
}

static FAutoConsoleCommandWithWorld GAFLFindDegenerateScaleCmd(
	TEXT("afl.Diag.FindDegenerateScale"),
	TEXT("Name every primitive (and ISM instance) with a zero/near-zero scale axis -- the distance-field NaN "
	     "source. Re-scans every 2s for ~24s, because a scan run before the level populates reports a false "
	     "all-clear."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		AFLScanWithRetry(World, MakeShared<int32>(0));
	}));
