// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLDataLayerStreamTestHarness.h"

#include "AFLGameCore.h"
#include "Engine/World.h"
#include "EngineUtils.h"                                   // TActorIterator
#include "GameFramework/Actor.h"
#include "GameFramework/CheatManagerDefines.h"             // UE_WITH_CHEAT_MANAGER -- WITHOUT THIS the macro is
                                                           // undefined, #if evaluates as 0, and the console command
                                                           // below is silently compiled out. Build stays green.
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace
{
	/** Only one run at a time; a second start while live would interleave phases and poison the log. */
	TWeakObjectPtr<UAFLDataLayerStreamTestHarness> GLiveRun;

	/** Streaming is asynchronous and its duration varies with cell size, disk and platform. A fixed
	 *  sleep is a guess that passes on a fast machine and fails on a slow one, so we POLL World
	 *  Partition for quiescence instead and only use these as rails. */
	constexpr float kSettleTimeoutSeconds = 8.0f;   // hard cap so a stuck stream reports rather than hangs
	constexpr int32 kMinSettleFrames      = 3;      // never query on the request frame, nor the one after
}

const TCHAR* UAFLDataLayerStreamTestHarness::StateName(EDataLayerRuntimeState State)
{
	switch (State)
	{
	case EDataLayerRuntimeState::Unloaded:  return TEXT("Unloaded");
	case EDataLayerRuntimeState::Loaded:    return TEXT("Loaded");
	case EDataLayerRuntimeState::Activated: return TEXT("Activated");
	default:                                return TEXT("<unknown>");
	}
}

void UAFLDataLayerStreamTestHarness::RunInWorld(UWorld* World, const FString& LayerAssetPath)
{
	if (GLiveRun.IsValid())
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_TEST ABORT -- a StreamTest run is already live. Stop PIE and retry."));
		return;
	}

	UAFLDataLayerStreamTestHarness* Harness = NewObject<UAFLDataLayerStreamTestHarness>();
	if (!Harness->StartRun(World, LayerAssetPath))
	{
		return;   // StartRun logs its own reason
	}
	GLiveRun = Harness;
}

bool UAFLDataLayerStreamTestHarness::StartRun(UWorld* World, const FString& LayerAssetPath)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- no game world. Run this from inside PIE."));
		return false;
	}

	// SetDataLayerRuntimeState is server-authoritative: AWorldDataLayers::CanChangeDataLayerRuntimeState
	// rejects AuthoritativeFromClient and logs the refusal at Verbose, which is OFF by default. A client
	// run would therefore look like a silent no-op, so refuse loudly up front instead.
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST ABORT -- this is a CLIENT window. SetDataLayerRuntimeState is server-authoritative ")
			TEXT("and would be silently ignored. Run in the HOST/listen-server window."));
		return false;
	}

	const UDataLayerAsset* Asset = LoadObject<UDataLayerAsset>(nullptr, *LayerAssetPath);
	if (!Asset)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- could not load DataLayerAsset '%s'."), *LayerAssetPath);
		return false;
	}
	if (!Asset->IsRuntime())
	{
		// Editor data layers have no runtime existence at all -- the state change would be dropped and
		// logged only at Verbose. Catch it here rather than reporting a mystery FAIL at P3.
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST ABORT -- '%s' is an EDITOR data layer. Only Runtime layers can be streamed; ")
			TEXT("a state change on this would be silently ignored."), *LayerAssetPath);
		return false;
	}

	UDataLayerManager* Manager = UDataLayerManager::GetDataLayerManager(World);
	if (!Manager || !Manager->GetDataLayerInstanceFromAsset(Asset))
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST ABORT -- '%s' has no DataLayerInstance in this world. The asset exists but the ")
			TEXT("map does not reference it."), *LayerAssetPath);
		return false;
	}

	WorldPtr   = World;
	LayerAsset = Asset;
	bRunning   = true;
	Phase      = EPhase::P0_Baseline;
	PhaseElapsed = 0.0f;
	PhaseFrames  = 0;
	AddToRoot();

	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST BEGIN -- DataLayer streaming self-test on '%s' (netmode=%d). ")
		TEXT("Watch for the marker appearing/vanishing; the LOG is the assertion."),
		*Asset->GetName(), (int32)World->GetNetMode());
	return true;
}

void UAFLDataLayerStreamTestHarness::FinishRun()
{
	bRunning = false;
	Phase    = EPhase::Done;
	UE_LOG(LogAFLGameCore, Display, TEXT("AFL_TEST COMPLETE"));
	GLiveRun = nullptr;
	RemoveFromRoot();
}

void UAFLDataLayerStreamTestHarness::EnterPhase(EPhase Next)
{
	Phase        = Next;
	PhaseElapsed = 0.0f;
	PhaseFrames  = 0;
}

EDataLayerRuntimeState UAFLDataLayerStreamTestHarness::EffectiveState() const
{
	UWorld* World = WorldPtr.Get();
	UDataLayerManager* Manager = World ? UDataLayerManager::GetDataLayerManager(World) : nullptr;
	const UDataLayerInstance* Instance = (Manager && LayerAsset.IsValid())
		? Manager->GetDataLayerInstanceFromAsset(LayerAsset.Get()) : nullptr;
	return Instance ? Manager->GetDataLayerInstanceEffectiveRuntimeState(Instance)
	                : EDataLayerRuntimeState::Unloaded;
}

EDataLayerRuntimeState UAFLDataLayerStreamTestHarness::TargetState() const
{
	UWorld* World = WorldPtr.Get();
	UDataLayerManager* Manager = World ? UDataLayerManager::GetDataLayerManager(World) : nullptr;
	const UDataLayerInstance* Instance = (Manager && LayerAsset.IsValid())
		? Manager->GetDataLayerInstanceFromAsset(LayerAsset.Get()) : nullptr;
	return Instance ? Instance->GetRuntimeState() : EDataLayerRuntimeState::Unloaded;
}

int32 UAFLDataLayerStreamTestHarness::CountLayerActorsInWorld(FVector& OutFirstLocation) const
{
	OutFirstLocation = FVector::ZeroVector;
	UWorld* World = WorldPtr.Get();
	if (!World || !LayerAsset.IsValid())
	{
		return 0;
	}

	// PRESENCE means "instantiated in the world", which is exactly what TActorIterator walks -- the
	// level's actor arrays. It cannot see an actor that lives only in the map file, so a streamed-out
	// actor is genuinely absent rather than merely hidden. Membership is read from the actor itself
	// (AActor::ContainsDataLayer, ENGINE_API and NOT editor-only) rather than matched on a name.
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ContainsDataLayer(LayerAsset.Get()))
		{
			if (Count == 0)
			{
				OutFirstLocation = Actor->GetActorLocation();
			}
			++Count;
		}
	}
	return Count;
}

bool UAFLDataLayerStreamTestHarness::StreamingSettled() const
{
	if (PhaseFrames < kMinSettleFrames)
	{
		return false;   // never sample on the request frame
	}
	UWorld* World = WorldPtr.Get();
	const UWorldPartitionSubsystem* Subsystem = World ? World->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
	return Subsystem ? Subsystem->IsStreamingCompleted() : true;
}

void UAFLDataLayerStreamTestHarness::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	if (!WorldPtr.IsValid())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- world went away mid-run."));
		FinishRun();
		return;
	}

	PhaseElapsed += DeltaTime;
	++PhaseFrames;

	UDataLayerManager* Manager = UDataLayerManager::GetDataLayerManager(WorldPtr.Get());
	FVector FirstLoc = FVector::ZeroVector;

	switch (Phase)
	{
	case EPhase::P0_Baseline:
	{
		P0Count = CountLayerActorsInWorld(FirstLoc);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[P0] BASELINE actors=%d expected=0 %s -- targetState=%s effectiveState=%s"),
			P0Count, (P0Count == 0 ? TEXT("PASS") : TEXT("FAIL")),
			StateName(TargetState()), StateName(EffectiveState()));
		EnterPhase(EPhase::P1_Activate);
		break;
	}

	case EPhase::P1_Activate:
	{
		bActivateAccepted = Manager && Manager->SetDataLayerRuntimeState(LayerAsset.Get(), EDataLayerRuntimeState::Activated);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[P1] ACTIVATE requested -- accepted=%s targetState=%s effectiveState=%s"),
			bActivateAccepted ? TEXT("true") : TEXT("FALSE"),
			StateName(TargetState()), StateName(EffectiveState()));
		EnterPhase(EPhase::P2_Settle);
		break;
	}

	case EPhase::P2_Settle:
	{
		if (StreamingSettled() || PhaseElapsed >= kSettleTimeoutSeconds)
		{
			bSettleTimedOut = !StreamingSettled();
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST[P2] SETTLE %s after %.2fs / %d frames -- effectiveState=%s"),
				bSettleTimedOut ? TEXT("TIMED OUT") : TEXT("complete"),
				PhaseElapsed, PhaseFrames, StateName(EffectiveState()));
			EnterPhase(EPhase::P3_Verify);
		}
		break;
	}

	case EPhase::P3_Verify:
	{
		P3Count = CountLayerActorsInWorld(FirstLoc);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[P3] VERIFY actors=%d expected>=1 %s firstLoc=(%.0f,%.0f,%.0f) -- targetState=%s effectiveState=%s"),
			P3Count, (P3Count >= 1 ? TEXT("PASS") : TEXT("FAIL")),
			FirstLoc.X, FirstLoc.Y, FirstLoc.Z,
			StateName(TargetState()), StateName(EffectiveState()));
		EnterPhase(EPhase::P4_Deactivate);
		break;
	}

	case EPhase::P4_Deactivate:
	{
		bDeactivateAccepted = Manager && Manager->SetDataLayerRuntimeState(LayerAsset.Get(), EDataLayerRuntimeState::Unloaded);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[P4] DEACTIVATE requested -- accepted=%s targetState=%s effectiveState=%s"),
			bDeactivateAccepted ? TEXT("true") : TEXT("FALSE"),
			StateName(TargetState()), StateName(EffectiveState()));
		EnterPhase(EPhase::P4b_SettleClear);
		break;
	}

	case EPhase::P4b_SettleClear:
	{
		if (StreamingSettled() || PhaseElapsed >= kSettleTimeoutSeconds)
		{
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST[P4b] SETTLE-CLEAR %s after %.2fs / %d frames -- effectiveState=%s"),
				StreamingSettled() ? TEXT("complete") : TEXT("TIMED OUT"),
				PhaseElapsed, PhaseFrames, StateName(EffectiveState()));
			EnterPhase(EPhase::P5_VerifyClear);
		}
		break;
	}

	case EPhase::P5_VerifyClear:
	{
		P5Count = CountLayerActorsInWorld(FirstLoc);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[P5] VERIFY-CLEAR actors=%d expected=0 %s -- targetState=%s effectiveState=%s"),
			P5Count, (P5Count == 0 ? TEXT("PASS") : TEXT("FAIL")),
			StateName(TargetState()), StateName(EffectiveState()));
		EnterPhase(EPhase::P6_Verdict);
		break;
	}

	case EPhase::P6_Verdict:
	{
		const bool bP0 = (P0Count == 0);
		const bool bP3 = (P3Count >= 1);
		const bool bP5 = (P5Count == 0);

		if (bP0 && bP3 && bP5)
		{
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST VERDICT PASS -- streaming follows effective state (P0=%d P3=%d P5=%d)."),
				P0Count, P3Count, P5Count);
		}
		else
		{
			// Name the phase, and separate "state never changed" from "state changed, nothing streamed".
			FString Broke;
			if (!bP0) { Broke += TEXT("P0(baseline not empty) "); }
			if (!bP3) { Broke += TEXT("P3(NOTHING STREAMED IN) "); }
			if (!bP5) { Broke += TEXT("P5(did not stream out) "); }

			const TCHAR* Diagnosis =
				(!bP3 && bActivateAccepted)
					? TEXT("state was accepted but no actor appeared -> STREAMING is not following effective state")
					: (!bP3 ? TEXT("the state change itself was refused -> not a streaming fault") : TEXT("see phases"));

			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_TEST VERDICT FAIL -- broke: %s| P0=%d P3=%d P5=%d activateAccepted=%s settleTimedOut=%s | %s"),
				*Broke, P0Count, P3Count, P5Count,
				bActivateAccepted ? TEXT("true") : TEXT("false"),
				bSettleTimedOut ? TEXT("true") : TEXT("false"),
				Diagnosis);
		}
		FinishRun();
		break;
	}

	default:
		FinishRun();
		break;
	}
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLDataLayerStreamTestCmd(
		TEXT("afl.DL.StreamTest"),
		TEXT("Data layer streaming self-test: asserts that an activated RUNTIME data layer's actors actually ")
		TEXT("stream into the world. Optional arg = DataLayerAsset path (defaults to District_Duel). Run in the ")
		TEXT("HOST window. Watch the marker; stop PIE after AFL_TEST COMPLETE, then read the log."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				const FString LayerPath = (Args.Num() > 0)
					? Args[0]
					: TEXT("/Game/Maps/DataLayers/L_ShantyTown/District_Duel.District_Duel");
				Ar.Logf(TEXT("afl.DL.StreamTest -- starting on %s (see AFL_TEST lines in LogAFLGameCore)."), *LayerPath);
				UAFLDataLayerStreamTestHarness::RunInWorld(World, LayerPath);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
