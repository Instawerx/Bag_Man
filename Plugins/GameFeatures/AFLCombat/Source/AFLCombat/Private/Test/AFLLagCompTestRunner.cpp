// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLLagCompTestRunner.h"

#include "AFLCombat.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "HAL/IConsoleManager.h"
#include "Targeting/AFLLagTestDummy.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLagCompTestRunner)

TWeakObjectPtr<UAFLLagCompTestRunner> UAFLLagCompTestRunner::ActiveRun;
constexpr int32 UAFLLagCompTestRunner::CohortPktLagMs[UAFLLagCompTestRunner::NumCohorts];

namespace
{
	const FName NAME_Tag_Input_Fire_LCRun = TEXT("InputTag.Weapon.Fire");
}

void UAFLLagCompTestRunner::RunInWorld(UWorld* World)
{
	// The runner drives the CLIENT side: its PktLag leg applies to the client driver and the pinned
	// volleys must travel real transit (the whole point). Mirror of the interaction harness guards.
	if (!World || World->GetNetMode() != NM_Client)
	{
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_LCRUN: afl.LagComp.Test.Run belongs in the CLIENT window (real transit is the test)."));
		return;
	}
	if (ActiveRun.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LCRUN: a run is already live -- wait for COMPLETE."));
		return;
	}
	UAFLLagCompTestRunner* Runner = NewObject<UAFLLagCompTestRunner>(GetTransientPackage());
	if (!Runner->StartRun(World))
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_LCRUN ABORT -- preconditions unmet (see lines above)."));
		return;
	}
	Runner->AddToRoot();
	ActiveRun = Runner;
}

bool UAFLLagCompTestRunner::StartRun(UWorld* World)
{
	WorldPtr = World;
	PC = World->GetFirstPlayerController();
	APlayerState* PS = PC.IsValid() ? PC->PlayerState : nullptr;
	LyraASC = Cast<ULyraAbilitySystemComponent>(
		PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr);
	if (!LyraASC.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_LCRUN: no Lyra ASC on the local PlayerState."));
		return false;
	}

	bool bDummyVisible = false;
	for (TActorIterator<AAFLLagTestDummy> It(World); It; ++It)
	{
		bDummyVisible = true;
		break;
	}
	if (!bDummyVisible)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_LCRUN: no AAFLLagTestDummy visible in the client world."));
		return false;
	}

	// Arm the instruments. CVars are process-global in PIE-under-one-process, so the SERVER-side sweep
	// arms from here too -- the one-command property the operator asked for.
	SetGlobalCVar(TEXT("afl.LagComp.SweepDiagnostic"), 1);
	SetGlobalCVar(TEXT("afl.LagComp.AimAtDummy"), 1);
	SetGlobalCVar(TEXT("afl.LagComp.PinMirror"), 0);

	CohortIdx = 0;
	Step = EPhaseStep::Apply;
	StepTimer = 0.0f;
	bRunning = true;
	Marker(TEXT("RUN -- 3 latency cohorts (client-out PktLag 15/40/100ms), 10 pinned shots + 1 flick pair each. Operator: just watch. ~40s."));
	return true;
}

void UAFLLagCompTestRunner::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	if (!WorldPtr.IsValid() || !PC.IsValid() || !LyraASC.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_LCRUN ABORT -- world/PC/ASC went away mid-run."));
		FinishRun();
		return;
	}
	StepTimer += DeltaTime;

	switch (Step)
	{
	case EPhaseStep::Apply:
	{
		const int32 LagMs = CohortPktLagMs[CohortIdx];
		PC->ConsoleCommand(FString::Printf(TEXT("Net PktLag=%d"), LagMs), true);
		Marker(FString::Printf(TEXT("cohort %d/3: Net PktLag=%dms applied (client OUTGOING leg only -- the clean one-leg topology); settling ping %.0fs"),
			CohortIdx + 1, LagMs, SettleSeconds));
		Step = EPhaseStep::Settle;
		StepTimer = 0.0f;
		break;
	}
	case EPhaseStep::Settle:
	{
		if (StepTimer >= SettleSeconds)
		{
			MeasuredPingMs[CohortIdx] = ReadOwnPingMs();
			Marker(FString::Printf(TEXT("cohort %d/3: measured ping=%.0fms (PktLag=%dms + loopback base)"),
				CohortIdx + 1, MeasuredPingMs[CohortIdx], CohortPktLagMs[CohortIdx]));
			ShotsFired = 0;
			Step = EPhaseStep::Volley;
			StepTimer = VolleySpacing; // fire immediately
		}
		break;
	}
	case EPhaseStep::Volley:
	{
		if (StepTimer >= VolleySpacing)
		{
			StepTimer = 0.0f;
			PressFire();
			++ShotsFired;
			if (ShotsFired >= ShotsPerVolley)
			{
				Step = EPhaseStep::FlickArm;
				StepTimer = 0.0f;
			}
		}
		break;
	}
	case EPhaseStep::FlickArm:
	{
		if (StepTimer >= VolleySpacing)
		{
			// Flick pair, shot A: normal pinned aim. Then mirror for shot B.
			PressFire();
			SetGlobalCVar(TEXT("afl.LagComp.PinMirror"), 1);
			Step = EPhaseStep::FlickFire;
			StepTimer = 0.0f;
		}
		break;
	}
	case EPhaseStep::FlickFire:
	{
		if (StepTimer >= FlickSpacing)
		{
			// Shot B: 180 degrees from shot A, 0.2s later = 900 deg/s shipped in the payload. If the
			// fire cooldown blocks this press, no second AFL_LAGCOMP_PIN appears -- an honest miss the
			// post-run log read will see.
			PressFire();
			SetGlobalCVar(TEXT("afl.LagComp.PinMirror"), 0);
			Marker(FString::Printf(TEXT("cohort %d/3: flick pair fired (180deg / %.1fs = %.0f deg/s if the second press cleared cooldown)"),
				CohortIdx + 1, FlickSpacing, 180.0f / FlickSpacing));
			Step = EPhaseStep::NextCohort;
			StepTimer = 0.0f;
		}
		break;
	}
	case EPhaseStep::NextCohort:
	{
		if (StepTimer >= 1.0f)
		{
			++CohortIdx;
			Step = (CohortIdx >= NumCohorts) ? EPhaseStep::Finish : EPhaseStep::Apply;
			StepTimer = 0.0f;
		}
		break;
	}
	case EPhaseStep::Finish:
	{
		FinishRun();
		break;
	}
	}
}

void UAFLLagCompTestRunner::FinishRun()
{
	bRunning = false;

	// Cleanup: restore the driver + disarm everything this run armed.
	if (PC.IsValid())
	{
		PC->ConsoleCommand(TEXT("Net PktLag=0"), true);
	}
	SetGlobalCVar(TEXT("afl.LagComp.AimAtDummy"), 0);
	SetGlobalCVar(TEXT("afl.LagComp.PinMirror"), 0);
	SetGlobalCVar(TEXT("afl.LagComp.SweepDiagnostic"), 0);

	// Client-side verdicts: ping scaled per cohort (the out-leg adds ~PktLag to RTT; band is generous
	// because ExactPing smooths). The sweep aggregation itself is the post-run log read's job.
	for (int32 i = 0; i < NumCohorts; ++i)
	{
		const float Expected = static_cast<float>(CohortPktLagMs[i]) + 18.0f; // + loopback base
		const bool bInBand = MeasuredPingMs[i] > Expected * 0.5f && MeasuredPingMs[i] < Expected * 1.8f;
		Check(bInBand, FString::Printf(TEXT("cohort %dms ping in band (measured %.0fms, expected ~%.0fms)"),
			CohortPktLagMs[i], MeasuredPingMs[i], Expected));
	}

	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LCRUN[%s] SUMMARY total: %d checks, %d failed. Sweep verdicts = the AFL_LAGCOMP_SWEEP lines (server side)."),
		*NetTag(), ChecksTotal, ChecksFailed);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LCRUN[%s] COMPLETE"), *NetTag());
	Marker(TEXT("COMPLETE -- stop PIE for log verification."));

	if (ActiveRun.Get() == this)
	{
		ActiveRun.Reset();
	}
	RemoveFromRoot();
}

void UAFLLagCompTestRunner::PressFire()
{
	const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(NAME_Tag_Input_Fire_LCRun, false);
	if (FireTag.IsValid() && LyraASC.IsValid())
	{
		// Press+release back-to-back -- the real IA trigger shape, the interaction harness precedent.
		LyraASC->AbilityInputTagPressed(FireTag);
		LyraASC->AbilityInputTagReleased(FireTag);
	}
}

void UAFLLagCompTestRunner::SetGlobalCVar(const TCHAR* Name, int32 Value) const
{
	if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
	{
		Var->Set(Value, ECVF_SetByConsole);
	}
}

float UAFLLagCompTestRunner::ReadOwnPingMs() const
{
	const APlayerState* PS = PC.IsValid() ? PC->PlayerState : nullptr;
	return PS ? PS->GetPingInMilliseconds() : -1.0f;
}

FString UAFLLagCompTestRunner::NetTag() const
{
	const UWorld* W = WorldPtr.Get();
	int32 PieId = -1;
#if WITH_EDITOR
	if (W && W->GetOutermost())
	{
		PieId = W->GetOutermost()->GetPIEInstanceID();
	}
#endif
	return (PieId >= 0) ? FString::Printf(TEXT("CL%d"), PieId) : FString(TEXT("CL"));
}

void UAFLLagCompTestRunner::Marker(const FString& Msg) const
{
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LCRUN[%s]: %s"), *NetTag(), *Msg);
#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Orange, FString::Printf(TEXT("AFL_LCRUN  %s"), *Msg));
	}
#endif
}

void UAFLLagCompTestRunner::Check(bool bPass, const FString& What)
{
	++ChecksTotal;
	if (!bPass)
	{
		++ChecksFailed;
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LCRUN[%s] %s -- %s"), *NetTag(), bPass ? TEXT("PASS") : TEXT("FAIL"), *What);
#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, bPass ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("AFL_LCRUN %s  %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *What));
	}
#endif
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLagCompTestRunCmd(
		TEXT("afl.LagComp.Test.Run"),
		TEXT("Cycle-3b lag-comp run, CLIENT window, one command, operator-observe-only: arms the server sweep + pinned aim (process-global cvars), phases Net PktLag 15/40/100ms cohorts at runtime, fires 10 pinned shots + a synthesized 900deg/s flick pair per cohort, cleans up after itself. ~40s; stop PIE after COMPLETE."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.LagComp.Test.Run -- starting (see AFL_LCRUN lines in LogAFLCombat)."));
				UAFLLagCompTestRunner::RunInWorld(World);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
