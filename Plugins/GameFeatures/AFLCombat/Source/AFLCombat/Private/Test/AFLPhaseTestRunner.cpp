// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLPhaseTestRunner.h"

#include "AFLCombat.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/AFLAG_Extract.h"
#include "Attributes/AFLAttributeSet_Energy.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "Engine/World.h"
#include "Extraction/AFLExtractionZone.h"
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "Phases/AFLMatchPhaseComponent.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLPhaseTestRunner)

TWeakObjectPtr<UAFLPhaseTestRunner> UAFLPhaseTestRunner::ActiveRun;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Playing_PhaseRun, "AFL.GamePhase.Playing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_ExtractionWindow_PhaseRun, "AFL.GamePhase.Playing.ExtractionWindow");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_InExtractionZone_PhaseRun, "State.InExtractionZone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Extracting_PhaseRun, "State.Extracting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowOpen_PhaseRun, "Event.Extraction.WindowOpen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowClosed_PhaseRun, "Event.Extraction.WindowClosed");

namespace
{
	constexpr float WattsPerEnergyExpected = 10.0f;
}

void UAFLPhaseTestRunner::RunInWorld(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_PHASERUN: afl.Phase.Test.Run belongs in the HOST window (phase ops are authority-only)."));
		return;
	}
	if (ActiveRun.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_PHASERUN: a run is already live -- wait for COMPLETE."));
		return;
	}
	UAFLPhaseTestRunner* Runner = NewObject<UAFLPhaseTestRunner>(GetTransientPackage());
	if (!Runner->StartRun(World))
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_PHASERUN ABORT -- preconditions unmet (see lines above)."));
		return;
	}
	Runner->AddToRoot();
	ActiveRun = Runner;
}

bool UAFLPhaseTestRunner::StartRun(UWorld* World)
{
	WorldPtr = World;
	PC = World->GetFirstPlayerController();
	Pawn = PC.IsValid() ? PC->GetPawn() : nullptr;
	APlayerState* PS = PC.IsValid() ? PC->PlayerState : nullptr;
	ASC = PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;
	Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	AGameStateBase* GS = World->GetGameState();
	Driver = GS ? GS->FindComponentByClass<UAFLMatchPhaseComponent>() : nullptr;

	if (!Pawn.IsValid() || !ASC.IsValid() || !Wallet.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_PHASERUN: no pawn / PlayerState ASC / wallet."));
		return false;
	}
	if (!Driver.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_PHASERUN: no UAFLMatchPhaseComponent on the GameState (experience row 15 -- relaunch the editor if just added)."));
		return false;
	}
	if (!ASC->GetSet<UAFLAttributeSet_Energy>())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_PHASERUN: UAFLAttributeSet_Energy not granted."));
		return false;
	}

	// Spawn the zone the runner observes (it registers its own phase observer in BeginPlay; the
	// driver's window phase drives it -- the integration proof). 1200uu ahead, pawn starts inside-able.
	ZoneCenter = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 1200.0f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Zone = World->SpawnActor<AAFLExtractionZone>(AAFLExtractionZone::StaticClass(), ZoneCenter, FRotator::ZeroRotator, Params);
	if (!Zone.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_PHASERUN: zone spawn failed."));
		return false;
	}

	// Announce listeners (the dual-broadcast lands here on the host world).
	WindowOpenCount = 0;
	WindowClosedCount = 0;
	WindowOpenListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		TAG_Event_Extraction_WindowOpen_PhaseRun,
		[this](FGameplayTag, const FLyraVerbMessage&) { ++WindowOpenCount; });
	WindowClosedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		TAG_Event_Extraction_WindowClosed_PhaseRun,
		[this](FGameplayTag, const FLyraVerbMessage&) { ++WindowClosedCount; });

	// Compressed cadence + 0 drain (seeding crosses the overdrive threshold). Restored at finish.
	auto SetCvar = [](const TCHAR* Name, float Value, float& Restore)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { Restore = V->GetFloat(); V->Set(Value, ECVF_SetByConsole); }
	};
	// Park the auto-cadence FAR out for the deterministic legs (1-3): run-1 lesson -- with Period 10
	// the driver's own timer reopened a window mid-leg and collided with the force-open/close asserts
	// (three false-fails, all races). The ForceWindow cheat drives legs 1-3; the cadence is proven
	// alone in leg 4 by dropping Period to 8 THEN.
	SetCvar(TEXT("afl.Extract.WindowPeriod"), 9000.0f, PeriodRestore);
	SetCvar(TEXT("afl.Extract.WindowDuration"), 5.0f, DurationRestore);
	SetCvar(TEXT("afl.Energy.DrainPerSecond"), 0.0f, DrainRestore);

	// Teleport the pawn into the zone for the whole run (it tests dispensing, not overlap mechanics).
	Pawn->SetActorLocation(ZoneCenter, false, nullptr, ETeleportType::TeleportPhysics);

	// Make sure no window is open at start (the driver may have opened one on its own timer). Force
	// it closed so leg-1's "closed at start" is deterministic.
	Driver->ForceWindowClose();

	Step = EStep::AssertClosedAtStart;
	StepTimer = 0.0f;
	bRunning = true;
	Marker(TEXT("RUN -- zone breathes on the phase clock (force-driven legs 1-3, cadence leg 4). Operator: just watch (~40s)."));
	return true;
}

void UAFLPhaseTestRunner::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	StepTimer += DeltaTime;

	switch (Step)
	{
	case EStep::AssertClosedAtStart:
	{
		if (StepTimer >= 0.8f)
		{
			Check(IsPhaseActive(TAG_AFL_GamePhase_Playing_PhaseRun), TEXT("AFL.GamePhase.Playing active (match shell started by the driver)"));
			Check(!IsPhaseActive(TAG_AFL_GamePhase_ExtractionWindow_PhaseRun), TEXT("no window at start (forced closed)"));
			Check(!HasTag(TAG_State_InExtractionZone_PhaseRun), TEXT("standing inside an INACTIVE zone dispenses NO tag"));
			Marker(TEXT("LEG 2 (force open): opening a window"));
			Step = EStep::ForceOpen; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::ForceOpen:
	{
		Driver->ForceWindowOpen();
		Step = EStep::AssertActive_Channel; StepTimer = 0.0f;
		break;
	}
	case EStep::AssertActive_Channel:
	{
		if (StepTimer >= 0.6f)
		{
			Check(IsPhaseActive(TAG_AFL_GamePhase_ExtractionWindow_PhaseRun), TEXT("window phase active after ForceWindow open"));
			Check(WindowOpenCount >= 1, FString::Printf(TEXT("Event.Extraction.WindowOpen broadcast on the host (count %d)"), WindowOpenCount));
			Check(HasTag(TAG_State_InExtractionZone_PhaseRun), TEXT("zone went Active -> State.InExtractionZone dispensed"));
			// A window must outlast a channel for this leg: bump duration just for the completing channel.
			if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Extract.WindowDuration"))) { V->Set(30.0f, ECVF_SetByConsole); }
			Driver->ForceWindowClose();   // close the 5s window...
			Driver->ForceWindowOpen();    // ...and reopen a fresh 30s one so the 6s channel completes
			WattsAtChannelStart = ReadWatts();
			Console(TEXT("AFL.Combat.EnergyGain 50"));
			Step = EStep::AssertChannelComplete; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertChannelComplete:
	{
		if (StepTimer >= 0.6f && EnergyAtChannelStart == 0.0f)
		{
			EnergyAtChannelStart = ReadCarriedEnergy();
			Check(ActivateExtract(), TEXT("channel started inside the window"));
		}
		if (StepTimer >= 7.0f)
		{
			const int32 Expected = FMath::RoundToInt(EnergyAtChannelStart * WattsPerEnergyExpected);
			Check(ReadCarriedEnergy() <= 0.5f, FString::Printf(TEXT("channel completed: energy zeroed (%.1f)"), ReadCarriedEnergy()));
			Check(ReadWatts() - WattsAtChannelStart == Expected,
				FString::Printf(TEXT("Watts conservation inside window: delta %d == %d"), ReadWatts() - WattsAtChannelStart, Expected));
			Marker(TEXT("LEG 3 (force close mid-channel): seeding 50"));
			Step = EStep::Seed3; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::Seed3:
	{
		Console(TEXT("AFL.Combat.EnergyGain 50"));
		EnergyAtChannelStart = 0.0f;
		Step = EStep::Channel3; StepTimer = 0.0f;
		break;
	}
	case EStep::Channel3:
	{
		if (StepTimer >= 0.6f)
		{
			EnergyAtChannelStart = ReadCarriedEnergy();
			WattsAtChannelStart = ReadWatts();
			Check(ActivateExtract(), TEXT("leg-3 channel started"));
			Step = EStep::ForceClose3; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::ForceClose3:
	{
		if (StepTimer >= 2.0f)
		{
			Driver->ForceWindowClose();
			Marker(TEXT("window force-closed at t=2 mid-channel -- expect cancel, energy retained"));
			Step = EStep::AssertCancel3; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertCancel3:
	{
		if (StepTimer >= 1.0f)
		{
			Check(!IsPhaseActive(TAG_AFL_GamePhase_ExtractionWindow_PhaseRun), TEXT("window phase ended on force-close"));
			Check(WindowClosedCount >= 1, FString::Printf(TEXT("Event.Extraction.WindowClosed broadcast on the host (count %d)"), WindowClosedCount));
			Check(!HasTag(TAG_State_InExtractionZone_PhaseRun), TEXT("zone Inactive -> State.InExtractionZone swept"));
			Check(!HasTag(TAG_State_Extracting_PhaseRun), TEXT("channel cancelled by the swept tag (GA self-cancel, zero new code)"));
			Check(FMath::Abs(ReadCarriedEnergy() - EnergyAtChannelStart) <= 0.5f,
				FString::Printf(TEXT("energy RETAINED on window-close cancel (%.1f == %.1f)"), ReadCarriedEnergy(), EnergyAtChannelStart));
			Check(ReadWatts() == WattsAtChannelStart, TEXT("no reward on window-close cancel"));
			// LEG 4: prove the AUTO-cadence alone. Drop Period to 6s and re-arm the driver's timer NOW
			// (it was parked at 9000s for the force-driven legs). WindowOpenCount=0 AFTER the reschedule
			// so we only count the cadence reopen. Force-close any lingering window first so the count
			// is unambiguous.
			Driver->ForceWindowClose();
			if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Extract.WindowPeriod"))) { V->Set(6.0f, ECVF_SetByConsole); }
			Driver->RescheduleCadence();
			WindowOpenCount = 0;
			Marker(TEXT("LEG 4 (cadence): Period dropped to 6s + timer re-armed -- waiting for the driver to reopen on its OWN timer"));
			Step = EStep::AwaitCadence; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AwaitCadence:
	{
		if (WindowOpenCount >= 1)
		{
			Step = EStep::AssertCadence; StepTimer = 0.0f;
		}
		else if (StepTimer >= 10.0f) // 6s period + generous margin
		{
			Step = EStep::AssertCadence; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertCadence:
	{
		Check(WindowOpenCount >= 1, FString::Printf(TEXT("driver reopened a window on its own cadence (opens since close: %d)"), WindowOpenCount));
		Step = EStep::Finish; StepTimer = 0.0f;
		break;
	}
	case EStep::Finish:
	{
		FinishRun();
		break;
	}
	}
}

void UAFLPhaseTestRunner::FinishRun()
{
	bRunning = false;
	if (WindowOpenListener.IsValid()) { WindowOpenListener.Unregister(); }
	if (WindowClosedListener.IsValid()) { WindowClosedListener.Unregister(); }
	if (Driver.IsValid()) { Driver->ForceWindowClose(); }
	if (Zone.IsValid()) { Zone->Destroy(); }
	auto Restore = [](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByConsole); }
	};
	Restore(TEXT("afl.Extract.WindowPeriod"), PeriodRestore);
	Restore(TEXT("afl.Extract.WindowDuration"), DurationRestore);
	Restore(TEXT("afl.Energy.DrainPerSecond"), DrainRestore);

	UE_LOG(LogAFLCombat, Display, TEXT("AFL_PHASERUN[%s] SUMMARY total: %d checks, %d failed."), *NetTag(), ChecksTotal, ChecksFailed);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_PHASERUN[%s] COMPLETE"), *NetTag());
	Marker(FString::Printf(TEXT("COMPLETE -- %d checks, %d failed. Stop PIE for log verification."), ChecksTotal, ChecksFailed));
	if (ActiveRun.Get() == this) { ActiveRun.Reset(); }
	RemoveFromRoot();
}

bool UAFLPhaseTestRunner::ActivateExtract()
{
	return ASC.IsValid() && ASC->TryActivateAbilityByClass(UAFLAG_Extract::StaticClass());
}

bool UAFLPhaseTestRunner::IsPhaseActive(const FGameplayTag& PhaseTag) const
{
	// Reflection-routed (THE LYRA PHASE WALL: no subsystem member links from outside LyraGame). Share
	// the driver's one tested ProcessEvent path.
	return UAFLMatchPhaseComponent::IsPhaseActiveReflected(WorldPtr.Get(), PhaseTag);
}

bool UAFLPhaseTestRunner::HasTag(const FGameplayTag& Tag) const
{
	return ASC.IsValid() && ASC->HasMatchingGameplayTag(Tag);
}

float UAFLPhaseTestRunner::ReadCarriedEnergy() const
{
	return ASC.IsValid() ? ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()) : -1.0f;
}

int32 UAFLPhaseTestRunner::ReadWatts() const
{
	return Wallet.IsValid() ? Wallet->GetWatts() : -1;
}

void UAFLPhaseTestRunner::Console(const FString& Cmd)
{
	if (PC.IsValid()) { PC->ConsoleCommand(Cmd, true); }
}

FString UAFLPhaseTestRunner::NetTag() const
{
	const UWorld* W = WorldPtr.Get();
	int32 PieId = -1;
#if WITH_EDITOR
	if (W && W->GetOutermost()) { PieId = W->GetOutermost()->GetPIEInstanceID(); }
#endif
	const TCHAR* Prefix = (W && W->GetNetMode() == NM_Client) ? TEXT("CL") : TEXT("SV");
	return (PieId >= 0) ? FString::Printf(TEXT("%s%d"), Prefix, PieId) : FString(Prefix);
}

void UAFLPhaseTestRunner::Marker(const FString& Msg) const
{
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_PHASERUN[%s]: %s"), *NetTag(), *Msg);
}

void UAFLPhaseTestRunner::Check(bool bPass, const FString& What)
{
	++ChecksTotal;
	if (!bPass) { ++ChecksFailed; }
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_PHASERUN[%s] %s -- %s"), *NetTag(), bPass ? TEXT("PASS") : TEXT("FAIL"), *What);
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLPhaseTestRunCmd(
		TEXT("afl.Phase.Test.Run"),
		TEXT("Match phases cycle 1: 4-leg scripted proof (closed/open+channel/force-close-mid/cadence) -- HOST window, observe-only, ~30s."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.Phase.Test.Run -- starting (see AFL_PHASERUN lines in LogAFLCombat)."));
				UAFLPhaseTestRunner::RunInWorld(World);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
