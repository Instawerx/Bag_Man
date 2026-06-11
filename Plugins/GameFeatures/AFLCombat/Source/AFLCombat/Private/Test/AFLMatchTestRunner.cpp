// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLMatchTestRunner.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/AFLAG_Extract.h"
#include "Abilities/AFLAG_Laser_Pulse.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchTestRunner)

TWeakObjectPtr<UAFLMatchTestRunner> UAFLMatchTestRunner::ActiveRun;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Warmup_MatchRun, "AFL.GamePhase.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Playing_MatchRun, "AFL.GamePhase.Playing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_PostGame_MatchRun, "AFL.GamePhase.PostGame");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_ExtractionWindow_MatchRun, "AFL.GamePhase.Playing.ExtractionWindow");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_InExtractionZone_MatchRun, "State.InExtractionZone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Firing_Pulse_MatchRun, "State.Firing.Pulse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Match_Ended_MatchRun, "Event.Match.Ended");

namespace
{
	constexpr float WattsPerEnergyExpected_MatchRun = 10.0f;
}

void UAFLMatchTestRunner::RunInWorld(UWorld* World)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_MATCHRUN: afl.Match.Test.Run belongs in the HOST window (phase ops are authority-only)."));
		return;
	}
	if (ActiveRun.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_MATCHRUN: a run is already live -- wait for COMPLETE."));
		return;
	}
	UAFLMatchTestRunner* Runner = NewObject<UAFLMatchTestRunner>(GetTransientPackage());
	if (!Runner->StartRun(World))
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_MATCHRUN ABORT -- preconditions unmet (see lines above)."));
		return;
	}
	Runner->AddToRoot();
	ActiveRun = Runner;
}

bool UAFLMatchTestRunner::StartRun(UWorld* World)
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
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_MATCHRUN: no pawn / PlayerState ASC / wallet."));
		return false;
	}
	if (!Driver.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_MATCHRUN: no UAFLMatchPhaseComponent on the GameState (experience row -- relaunch the editor if just added)."));
		return false;
	}
	if (!ASC->GetSet<UAFLAttributeSet_Energy>())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_MATCHRUN: UAFLAttributeSet_Energy not granted."));
		return false;
	}

	// The zone the harness reads (observes the driver's window phase, same as the phase runner).
	ZoneCenter = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 1200.0f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Zone = World->SpawnActor<AAFLExtractionZone>(AAFLExtractionZone::StaticClass(), ZoneCenter, FRotator::ZeroRotator, Params);
	if (!Zone.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_MATCHRUN: zone spawn failed."));
		return false;
	}
	Pawn->SetActorLocation(ZoneCenter, false, nullptr, ETeleportType::TeleportPhysics);

	MatchEndedCount = 0;
	MatchEndedWatts = -1.0;
	MatchEndedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		TAG_Event_Match_Ended_MatchRun,
		[this](FGameplayTag, const FLyraVerbMessage& Msg)
		{
			// Filter to OUR player (Target == our PlayerState).
			if (PC.IsValid() && Msg.Target == PC->PlayerState)
			{
				++MatchEndedCount;
				MatchEndedWatts = Msg.Magnitude;
			}
		});

	// Compress the spine + park the window cadence clear of the ActiveDuration boundary (the
	// leg-isolation lesson), then RESTART the spine so it re-reads these durations deterministically
	// (the driver started at its BeginPlay on the defaults; an already-armed timer can't be shortened).
	// Warmup 3 / Active 12 / Period 900 (no auto-window; the harness force-opens the one it needs).
	auto SetCvar = [](const TCHAR* Name, float Value, float& Restore)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { Restore = V->GetFloat(); V->Set(Value, ECVF_SetByConsole); }
	};
	SetCvar(TEXT("afl.Match.WarmupDuration"), 3.0f, WarmupRestore);
	SetCvar(TEXT("afl.Match.ActiveDuration"), 12.0f, ActiveRestore);
	SetCvar(TEXT("afl.Extract.WindowPeriod"), 900.0f, PeriodRestore);   // no auto-window; force the one we test
	SetCvar(TEXT("afl.Extract.WindowDuration"), 30.0f, DurationRestore); // outlasts the extraction channel
	SetCvar(TEXT("afl.Energy.DrainPerSecond"), 0.0f, DrainRestore);
	Driver->RestartMatch(); // re-enters Warmup reading the compressed cvars -> deterministic spine.

	Step = EStep::AssertWarmup;
	StepTimer = 0.0f;
	bRunning = true;
	Marker(TEXT("RUN -- full spine restarted compressed (Warmup 3 / Active 12). warmup(fire-gated) -> active(fire+extract) -> postgame(frozen+ended). ~25s."));
	return true;
}

void UAFLMatchTestRunner::Tick(float DeltaTime)
{
	if (!bRunning) { return; }
	StepTimer += DeltaTime;

	switch (Step)
	{
	case EStep::AssertWarmup:
	{
		if (StepTimer >= 0.8f)
		{
			Check(IsPhaseActive(TAG_AFL_GamePhase_Warmup_MatchRun), TEXT("Warmup active at PIE start (driver started the spine)"));
			Check(!TryFire(), TEXT("fire BLOCKED during warmup (Pulse activate refused)"));
			Check(!IsPhaseActive(TAG_AFL_GamePhase_ExtractionWindow_MatchRun), TEXT("no extraction window during warmup (zone Inactive)"));
			Marker(TEXT("LEG 2 (active): waiting for Warmup -> Playing chain"));
			Step = EStep::AwaitPlaying; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AwaitPlaying:
	{
		if (IsPhaseActive(TAG_AFL_GamePhase_Playing_MatchRun))
		{
			Step = EStep::AssertPlaying_Channel; StepTimer = 0.0f;
		}
		else if (StepTimer >= 6.0f) // compressed warmup is 3s; generous
		{
			Check(false, TEXT("Warmup never chained to Playing within 6s (compressed warmup is 3s)"));
			Step = EStep::Finish; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertPlaying_Channel:
	{
		if (StepTimer >= 0.5f)
		{
			Check(IsPhaseActive(TAG_AFL_GamePhase_Playing_MatchRun), TEXT("Playing active after the chain"));
			Check(!IsPhaseActive(TAG_AFL_GamePhase_Warmup_MatchRun), TEXT("Warmup cancelled by Playing"));
			Check(TryFire(), TEXT("fire WORKS in Playing (warmup freeze lifted)"));
			// Force a window open + a long duration so the extraction can complete, then channel.
			if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Extract.WindowDuration"))) { V->Set(30.0f, ECVF_SetByConsole); }
			Driver->ForceWindowOpen();
			Console(TEXT("AFL.Combat.EnergyGain 50"));
			WattsBeforeExtract = ReadWatts();
			bExtractDone = false;
			Step = EStep::AssertExtraction; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertExtraction:
	{
		if (StepTimer >= 0.6f && !bExtractDone)
		{
			bExtractDone = true;
			Check(HasTag(TAG_State_InExtractionZone_MatchRun), TEXT("window open -> State.InExtractionZone dispensed"));
			Check(ActivateExtract(), TEXT("extraction channel started inside the window"));
		}
		if (StepTimer >= 7.0f)
		{
			ExtractDelta = ReadWatts() - WattsBeforeExtract;
			Check(ReadCarriedEnergy() <= 0.5f, FString::Printf(TEXT("extraction completed: energy zeroed (%.1f)"), ReadCarriedEnergy()));
			Check(ExtractDelta == FMath::RoundToInt(50.0f * WattsPerEnergyExpected_MatchRun),
				FString::Printf(TEXT("Watts conservation: delta %d == 500"), ExtractDelta));
			// The driver's ActiveDuration (compressed to 12s, armed at the Playing entry ~3s in) drives
			// PostGame deterministically -- the harness waits for it. The force-opened window is still
			// open (30s), so PostGame will cancel an IN-FLIGHT window: the real cancel-chain proof.
			Marker(TEXT("LEG 3 (postgame): active proof done. Waiting for the ActiveDuration timer to fire PostGame (cancels the in-flight window)."));
			Step = EStep::AwaitPostGame; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AwaitPostGame:
	{
		// Deterministic: the compressed ActiveDuration (12s from Playing) drives PostGame. Bounded wait.
		if (IsPhaseActive(TAG_AFL_GamePhase_PostGame_MatchRun))
		{
			Step = EStep::AssertPostGame; StepTimer = 0.0f;
		}
		else if (StepTimer >= 16.0f) // Active is 12s from Playing entry; this leg starts ~7s into Playing
		{
			Check(false, TEXT("PostGame never fired within the bounded wait (ActiveDuration 12s)"));
			Step = EStep::AssertTerminal; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertPostGame:
	{
		if (StepTimer >= 0.5f)
		{
			Check(IsPhaseActive(TAG_AFL_GamePhase_PostGame_MatchRun), TEXT("PostGame active (match ended)"));
			Check(!IsPhaseActive(TAG_AFL_GamePhase_Playing_MatchRun), TEXT("Playing cancelled by PostGame"));
			Check(!IsPhaseActive(TAG_AFL_GamePhase_ExtractionWindow_MatchRun), TEXT(".ExtractionWindow cancelled by PostGame (no force-close needed)"));
			Check(!HasTag(TAG_State_InExtractionZone_MatchRun), TEXT("zone Inactive at match end (handles swept)"));
			Check(MatchEndedCount >= 1, FString::Printf(TEXT("Event.Match.Ended received for our player (count %d)"), MatchEndedCount));
			Check(MatchEndedWatts >= 0.0, FString::Printf(TEXT("match-end Watts payload present (%.0f)"), MatchEndedWatts));
			Check(!TryFire(), TEXT("fire BLOCKED again in PostGame (Ended freeze)"));
			Step = EStep::AssertTerminal; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertTerminal:
	{
		if (StepTimer >= 1.0f)
		{
			// If we reached PostGame, prove no window reopens; else this is a soft pass.
			if (IsPhaseActive(TAG_AFL_GamePhase_PostGame_MatchRun))
			{
				Check(!IsPhaseActive(TAG_AFL_GamePhase_ExtractionWindow_MatchRun), TEXT("terminal: no window reopens under PostGame (cadence cleared)"));
			}
			Step = EStep::Finish; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::Finish:
	{
		FinishRun();
		break;
	}
	}
}

void UAFLMatchTestRunner::FinishRun()
{
	bRunning = false;
	if (MatchEndedListener.IsValid()) { MatchEndedListener.Unregister(); }
	if (Zone.IsValid()) { Zone->Destroy(); }
	auto Restore = [](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) { V->Set(Value, ECVF_SetByConsole); }
	};
	Restore(TEXT("afl.Match.WarmupDuration"), WarmupRestore);
	Restore(TEXT("afl.Match.ActiveDuration"), ActiveRestore);
	Restore(TEXT("afl.Extract.WindowPeriod"), PeriodRestore);
	Restore(TEXT("afl.Extract.WindowDuration"), DurationRestore);
	Restore(TEXT("afl.Energy.DrainPerSecond"), DrainRestore);

	UE_LOG(LogAFLCombat, Display, TEXT("AFL_MATCHRUN[%s] SUMMARY total: %d checks, %d failed."), *NetTag(), ChecksTotal, ChecksFailed);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_MATCHRUN[%s] COMPLETE"), *NetTag());
	Marker(FString::Printf(TEXT("COMPLETE -- %d checks, %d failed. Stop PIE for log verification."), ChecksTotal, ChecksFailed));
	if (ActiveRun.Get() == this) { ActiveRun.Reset(); }
	RemoveFromRoot();
}

bool UAFLMatchTestRunner::TryFire()
{
	// Pulse activate returns true only if it ACTIVATED -- ActivationBlockedTags (Warmup/Ended) make it
	// refuse, returning false. We immediately cancel so a successful fire doesn't linger.
	if (!ASC.IsValid()) { return false; }
	const bool bActivated = ASC->TryActivateAbilityByClass(UAFLAG_Laser_Pulse::StaticClass());
	if (bActivated)
	{
		// State.Firing.Pulse is the activation-owned tag; cancel to clean up the test fire.
		FGameplayTagContainer Cancel;
		Cancel.AddTag(TAG_State_Firing_Pulse_MatchRun);
		ASC->CancelAbilities(&Cancel);
	}
	return bActivated;
}

bool UAFLMatchTestRunner::ActivateExtract()
{
	return ASC.IsValid() && ASC->TryActivateAbilityByClass(UAFLAG_Extract::StaticClass());
}

bool UAFLMatchTestRunner::IsPhaseActive(const FGameplayTag& PhaseTag) const
{
	return UAFLMatchPhaseComponent::IsPhaseActiveReflected(WorldPtr.Get(), PhaseTag);
}

bool UAFLMatchTestRunner::HasTag(const FGameplayTag& Tag) const
{
	return ASC.IsValid() && ASC->HasMatchingGameplayTag(Tag);
}

float UAFLMatchTestRunner::ReadCarriedEnergy() const
{
	return ASC.IsValid() ? ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()) : -1.0f;
}

int32 UAFLMatchTestRunner::ReadWatts() const
{
	return Wallet.IsValid() ? Wallet->GetWatts() : -1;
}

void UAFLMatchTestRunner::Console(const FString& Cmd)
{
	if (PC.IsValid()) { PC->ConsoleCommand(Cmd, true); }
}

FString UAFLMatchTestRunner::NetTag() const
{
	const UWorld* W = WorldPtr.Get();
	int32 PieId = -1;
#if WITH_EDITOR
	if (W && W->GetOutermost()) { PieId = W->GetOutermost()->GetPIEInstanceID(); }
#endif
	const TCHAR* Prefix = (W && W->GetNetMode() == NM_Client) ? TEXT("CL") : TEXT("SV");
	return (PieId >= 0) ? FString::Printf(TEXT("%s%d"), Prefix, PieId) : FString(Prefix);
}

void UAFLMatchTestRunner::Marker(const FString& Msg) const
{
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_MATCHRUN[%s]: %s"), *NetTag(), *Msg);
}

void UAFLMatchTestRunner::Check(bool bPass, const FString& What)
{
	++ChecksTotal;
	if (!bPass) { ++ChecksFailed; }
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_MATCHRUN[%s] %s -- %s"), *NetTag(), bPass ? TEXT("PASS") : TEXT("FAIL"), *What);
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLMatchTestRunCmd(
		TEXT("afl.Match.Test.Run"),
		TEXT("Match spine cycle 1: full warmup->active->postgame proof -- HOST window, RUN AT PIE START (set afl.Match.WarmupDuration/ActiveDuration low first for the compressed run). Observe-only."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.Match.Test.Run -- starting (see AFL_MATCHRUN lines in LogAFLCombat)."));
				UAFLMatchTestRunner::RunInWorld(World);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
