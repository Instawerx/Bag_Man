// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLExtractTestRunner.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/AFLAG_Extract.h"
#include "Attributes/AFLAttributeSet_Energy.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "Energy/AFLEnergyPickup.h"
#include "EngineUtils.h"
#include "Extraction/AFLExtractionZone.h"
// UE_WITH_CHEAT_MANAGER lives here -- without this include the #if guard at the bottom reads an
// UNDEFINED macro as 0 and the afl.Extract.Test.Run registration compiles out SILENTLY (run-1
// lesson: the build succeeds, the command just never exists).
#include "GameFramework/CheatManagerDefines.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLExtractTestRunner)

TWeakObjectPtr<UAFLExtractTestRunner> UAFLExtractTestRunner::ActiveRun;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_InExtractionZone_ExtractRun, "State.InExtractionZone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Extracting_ExtractRun, "State.Extracting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Complete_ExtractRun, "Event.Extraction.Complete");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Failed_ExtractRun, "Event.Extraction.Failed");

namespace
{
	constexpr float WattsPerEnergyExpected = 10.0f; // mirrors UAFLAG_Extract::WattsPerEnergy (locked pick)
}

void UAFLExtractTestRunner::RunInWorld(UWorld* World)
{
	// HOST window only: zone spawn, teleports and the cheats are authority ops (the energy-runner guard).
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_EXTRACTRUN: afl.Extract.Test.Run belongs in the HOST window (spawns/cheats are authority ops)."));
		return;
	}
	if (ActiveRun.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_EXTRACTRUN: a run is already live -- wait for COMPLETE."));
		return;
	}
	UAFLExtractTestRunner* Runner = NewObject<UAFLExtractTestRunner>(GetTransientPackage());
	if (!Runner->StartRun(World))
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_EXTRACTRUN ABORT -- preconditions unmet (see lines above)."));
		return;
	}
	Runner->AddToRoot();
	ActiveRun = Runner;
}

bool UAFLExtractTestRunner::StartRun(UWorld* World)
{
	WorldPtr = World;
	PC = World->GetFirstPlayerController();
	Pawn = PC.IsValid() ? PC->GetPawn() : nullptr;
	APlayerState* PS = PC.IsValid() ? PC->PlayerState : nullptr;
	ASC = PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;
	Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	if (!Pawn.IsValid() || !ASC.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_EXTRACTRUN: no possessed pawn / PlayerState ASC."));
		return false;
	}
	if (!ASC->GetSet<UAFLAttributeSet_Energy>())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_EXTRACTRUN: UAFLAttributeSet_Energy not granted (AbilitySet GrantedAttributes row)."));
		return false;
	}
	if (!Wallet.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_EXTRACTRUN: no UAFLWalletComponent on the PlayerState (experience row)."));
		return false;
	}

	// The zone, runner-spawned at a fixed offset (config-on-actor posture: the raw C++ class is
	// fully functional; the BP child only adds visuals). 1200uu ahead -- the pawn starts OUTSIDE.
	StartLocation = Pawn->GetActorLocation();
	ZoneCenter = StartLocation + Pawn->GetActorForwardVector() * 1200.0f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Zone = World->SpawnActor<AAFLExtractionZone>(AAFLExtractionZone::StaticClass(), ZoneCenter, FRotator::ZeroRotator, Params);
	if (!Zone.IsValid())
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_EXTRACTRUN: zone spawn failed."));
		return false;
	}

	// Windowing regression fix (match-phases cycle 1): the zone is now Inactive-by-default and
	// dispenses ONLY while a window is open. This runner proves the EXTRACT mechanics, not the
	// window cadence (afl.Phase.Test.Run owns that), and its zone is a BARE runner-spawn with no
	// driver phase to observe -- so activate it directly. Without this, leg-1's "tag UP on entry"
	// regresses to a permanent zone-Inactive fail.
	Zone->SetZoneActiveForTest(true);

	// Registered listeners (the conservation-law shape -- never log greps).
	CompleteCount = 0;
	FailedCount = 0;
	CompleteListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		TAG_Event_Extraction_Complete_ExtractRun,
		[this](FGameplayTag /*Channel*/, const FLyraVerbMessage& /*Msg*/) { ++CompleteCount; });
	FailedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FLyraVerbMessage>(
		TAG_Event_Extraction_Failed_ExtractRun,
		[this](FGameplayTag /*Channel*/, const FLyraVerbMessage& /*Msg*/) { ++FailedCount; });
	ReCollectedCount = 0;
	ReCollectedSum = 0.0f;
	CollectListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FAFLEnergyCollectedMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Event.Energy.Collected"), false),
		[this](FGameplayTag /*Channel*/, const FAFLEnergyCollectedMessage& Msg)
		{
			++ReCollectedCount;
			ReCollectedSum += Msg.EnergyValue;
		});

	// Pin the overdrive drain to 0: seeding 100 crosses the threshold and an active drain would
	// corrupt the exact-Watts conservation assert. SetByCaller-at-apply makes 0 stick. Restored
	// at FinishRun (the energy-runner cvar pattern).
	if (IConsoleVariable* DrainVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Energy.DrainPerSecond")))
	{
		DrainCvarRestore = DrainVar->GetFloat();
		DrainVar->Set(0.0f, ECVF_SetByConsole);
	}

	Step = EStep::Seed1;
	StepTimer = 0.0f;
	bRunning = true;
	Marker(TEXT("RUN -- 3 legs: complete / damage-interrupt / zone-exit. Zone spawned 1200uu ahead. Operator: just watch (~30s)."));
	return true;
}

void UAFLExtractTestRunner::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	StepTimer += DeltaTime;

	switch (Step)
	{
	case EStep::Seed1:
	{
		Console(TEXT("AFL.Combat.EnergyGain 100"));
		Marker(TEXT("LEG 1 (complete): seeded 100 energy"));
		Step = EStep::TeleportIn1; StepTimer = 0.0f;
		break;
	}
	case EStep::TeleportIn1:
	{
		if (StepTimer >= 0.6f)
		{
			Check(ReadCarriedEnergy() >= 99.0f,
				FString::Printf(TEXT("energy seeded (%.1f, expect 100)"), ReadCarriedEnergy()));
			Pawn->SetActorLocation(ZoneCenter, false, nullptr, ETeleportType::TeleportPhysics);
			Marker(TEXT("teleported INTO the zone"));
			Step = EStep::Activate1; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::Activate1:
	{
		if (StepTimer >= 0.6f)
		{
			Check(HasTag(TAG_State_InExtractionZone_ExtractRun), TEXT("State.InExtractionZone UP inside the volume (zone GE dispensed)"));
			WattsAtChannelStart = ReadWatts();
			EnergyAtChannelStart = ReadCarriedEnergy();
			Check(ActivateExtract(), TEXT("UAFLAG_Extract activated by class (AbilitySet grant + tag gate)"));
			Step = EStep::MidSample1; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::MidSample1:
	{
		if (StepTimer >= 3.0f)
		{
			Check(HasTag(TAG_State_Extracting_ExtractRun), TEXT("State.Extracting held mid-channel"));
			Check(ReadPawnMaxSpeed() <= 0.5f,
				FString::Printf(TEXT("O1 hard-lock live mid-channel (CMC GetMaxSpeed %.0f, expect 0)"), ReadPawnMaxSpeed()));
			Step = EStep::AssertComplete1; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertComplete1:
	{
		if (StepTimer >= 3.8f) // 6.8s total channel window
		{
			const int32 ExpectedWatts = FMath::RoundToInt(EnergyAtChannelStart * WattsPerEnergyExpected);
			Check(CompleteCount == 1, FString::Printf(TEXT("Event.Extraction.Complete received (count %d)"), CompleteCount));
			Check(ReadCarriedEnergy() <= 0.5f,
				FString::Printf(TEXT("CarriedEnergy zeroed through the rail (%.1f)"), ReadCarriedEnergy()));
			Check(ReadWatts() - WattsAtChannelStart == ExpectedWatts,
				FString::Printf(TEXT("Watts conservation: delta %d == energy %.1f x 10 (= %d)"),
					ReadWatts() - WattsAtChannelStart, EnergyAtChannelStart, ExpectedWatts));
			Check(!HasTag(TAG_State_Extracting_ExtractRun), TEXT("State.Extracting cleared after commit"));
			Check(ReadPawnMaxSpeed() > 0.5f, TEXT("movement restored after commit"));
			Marker(TEXT("LEG 2 (damage interrupt): seeding 50"));
			Step = EStep::Seed2; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::Seed2:
	{
		Console(TEXT("AFL.Combat.EnergyGain 50"));
		Step = EStep::Activate2; StepTimer = 0.0f;
		break;
	}
	case EStep::Activate2:
	{
		if (StepTimer >= 0.6f)
		{
			WattsAtChannelStart = ReadWatts();
			EnergyAtChannelStart = ReadCarriedEnergy();
			ReCollectedCount = 0; ReCollectedSum = 0.0f;
			Check(ActivateExtract(), TEXT("leg-2 channel started"));
			Step = EStep::Damage2; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::Damage2:
	{
		if (StepTimer >= 3.0f)
		{
			Console(TEXT("AFL.Combat.Damage 10"));
			Marker(TEXT("self-damage at t=3 -- expect interrupt + AFL-0808 burst"));
			Step = EStep::AssertBurst_Flee2; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertBurst_Flee2:
	{
		if (StepTimer >= 0.3f)
		{
			// Run-2 lesson: the magnet vacuums a 120uu ring in UNDER 0.3s with the owner standing in
			// it -- a world count alone false-fails while conservation holds exactly (1L/50.0 re-
			// collected). The burst proof is the DEATH-LEG shape: on-ground OR re-collected.
			Check(CountPickups() > 0 || ReCollectedSum > 0.0f,
				FString::Printf(TEXT("AFL-0808 interrupt burst happened (on-ground %d, re-collected %.1f)"),
					CountPickups(), ReCollectedSum));
			Pawn->SetActorLocation(StartLocation, false, nullptr, ETeleportType::TeleportPhysics);
			Step = EStep::AssertFailed2; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertFailed2:
	{
		if (StepTimer >= 1.2f)
		{
			Check(FailedCount == 1, FString::Printf(TEXT("Event.Extraction.Failed received (count %d)"), FailedCount));
			Check(ReadWatts() == WattsAtChannelStart,
				FString::Printf(TEXT("no reward on interrupt (Watts %d unchanged)"), ReadWatts()));
			Check(!HasTag(TAG_State_Extracting_ExtractRun), TEXT("State.Extracting cleared after interrupt"));
			// INFO, never a fail: the magnet may have re-collected part of the burst pre-flee.
			Marker(FString::Printf(TEXT("INFO leg-2 re-collection before the flee: %d pickups / %.1f energy (energy now %.1f of %.1f)"),
				ReCollectedCount, ReCollectedSum, ReadCarriedEnergy(), EnergyAtChannelStart));
			Marker(TEXT("LEG 3 (zone-exit): seeding 50"));
			Step = EStep::Seed3; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::Seed3:
	{
		Console(TEXT("AFL.Combat.EnergyGain 50"));
		Step = EStep::TeleportIn3; StepTimer = 0.0f;
		break;
	}
	case EStep::TeleportIn3:
	{
		if (StepTimer >= 0.6f)
		{
			Pawn->SetActorLocation(ZoneCenter, false, nullptr, ETeleportType::TeleportPhysics);
			Step = EStep::Activate3; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::Activate3:
	{
		if (StepTimer >= 0.6f)
		{
			Check(HasTag(TAG_State_InExtractionZone_ExtractRun), TEXT("re-entered the zone (tag re-dispensed)"));
			WattsAtChannelStart = ReadWatts();
			EnergyAtChannelStart = ReadCarriedEnergy();
			Check(ActivateExtract(), TEXT("leg-3 channel started"));
			Step = EStep::TeleportOut3; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::TeleportOut3:
	{
		if (StepTimer >= 2.0f)
		{
			Pawn->SetActorLocation(StartLocation, false, nullptr, ETeleportType::TeleportPhysics);
			Marker(TEXT("teleported OUT at t=2 (back to the start spot) -- expect zone-exit cancel, energy retained"));
			Step = EStep::AssertCancel3; StepTimer = 0.0f;
		}
		break;
	}
	case EStep::AssertCancel3:
	{
		if (StepTimer >= 0.9f)
		{
			Check(FailedCount == 2, FString::Printf(TEXT("zone-exit broadcast Failed (count %d, expect 2)"), FailedCount));
			Check(!HasTag(TAG_State_InExtractionZone_ExtractRun), TEXT("State.InExtractionZone removed on exit"));
			Check(!HasTag(TAG_State_Extracting_ExtractRun), TEXT("State.Extracting cancelled on exit"));
			Check(FMath::Abs(ReadCarriedEnergy() - EnergyAtChannelStart) <= 0.5f,
				FString::Printf(TEXT("energy RETAINED on zone-exit (%.1f == %.1f at channel start)"),
					ReadCarriedEnergy(), EnergyAtChannelStart));
			Check(ReadWatts() == WattsAtChannelStart,
				FString::Printf(TEXT("no reward on zone-exit (Watts %d unchanged)"), ReadWatts()));
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

void UAFLExtractTestRunner::FinishRun()
{
	bRunning = false;
	if (CompleteListener.IsValid()) { CompleteListener.Unregister(); }
	if (FailedListener.IsValid()) { FailedListener.Unregister(); }
	if (CollectListener.IsValid()) { CollectListener.Unregister(); }
	if (Zone.IsValid())
	{
		Zone->Destroy(); // EndPlay sweeps any live zone-GE handles.
	}
	if (IConsoleVariable* DrainVar = IConsoleManager::Get().FindConsoleVariable(TEXT("afl.Energy.DrainPerSecond")))
	{
		DrainVar->Set(DrainCvarRestore, ECVF_SetByConsole);
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_EXTRACTRUN[%s] SUMMARY total: %d checks, %d failed."), *NetTag(), ChecksTotal, ChecksFailed);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_EXTRACTRUN[%s] COMPLETE"), *NetTag());
	Marker(FString::Printf(TEXT("COMPLETE -- %d checks, %d failed. Stop PIE for log verification."), ChecksTotal, ChecksFailed));
	if (ActiveRun.Get() == this)
	{
		ActiveRun.Reset();
	}
	RemoveFromRoot();
}

bool UAFLExtractTestRunner::ActivateExtract()
{
	return ASC.IsValid() && ASC->TryActivateAbilityByClass(UAFLAG_Extract::StaticClass());
}

float UAFLExtractTestRunner::ReadCarriedEnergy() const
{
	return ASC.IsValid() ? ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()) : -1.0f;
}

int32 UAFLExtractTestRunner::ReadWatts() const
{
	return Wallet.IsValid() ? Wallet->GetWatts() : -1;
}

float UAFLExtractTestRunner::ReadPawnMaxSpeed() const
{
	const ACharacter* Character = Cast<ACharacter>(Pawn.Get());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	// GetMaxSpeed (not MaxWalkSpeed): the Lyra CMC override returns 0 under Gameplay.MovementStopped.
	return Movement ? Movement->GetMaxSpeed() : -1.0f;
}

bool UAFLExtractTestRunner::HasTag(const FGameplayTag& Tag) const
{
	return ASC.IsValid() && ASC->HasMatchingGameplayTag(Tag);
}

int32 UAFLExtractTestRunner::CountPickups() const
{
	int32 Count = 0;
	for (TActorIterator<AAFLEnergyPickup> It(WorldPtr.Get()); It; ++It)
	{
		++Count;
	}
	return Count;
}

void UAFLExtractTestRunner::Console(const FString& Cmd)
{
	if (PC.IsValid())
	{
		PC->ConsoleCommand(Cmd, true);
	}
}

FString UAFLExtractTestRunner::NetTag() const
{
	// The energy-runner shape: PIE instance id from the world package (GPlayInEditorID is
	// deprecated in 5.6); this runner is host-only so the SV prefix is the normal case.
	const UWorld* W = WorldPtr.Get();
	int32 PieId = -1;
#if WITH_EDITOR
	if (W && W->GetOutermost())
	{
		PieId = W->GetOutermost()->GetPIEInstanceID();
	}
#endif
	const TCHAR* Prefix = (W && W->GetNetMode() == NM_Client) ? TEXT("CL") : TEXT("SV");
	return (PieId >= 0) ? FString::Printf(TEXT("%s%d"), Prefix, PieId) : FString(Prefix);
}

void UAFLExtractTestRunner::Marker(const FString& Msg) const
{
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_EXTRACTRUN[%s]: %s"), *NetTag(), *Msg);
}

void UAFLExtractTestRunner::Check(bool bPass, const FString& What)
{
	++ChecksTotal;
	if (!bPass)
	{
		++ChecksFailed;
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_EXTRACTRUN[%s] %s -- %s"), *NetTag(), bPass ? TEXT("PASS") : TEXT("FAIL"), *What);
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLExtractTestRunCmd(
		TEXT("afl.Extract.Test.Run"),
		TEXT("Extraction cycle 1: 3-leg scripted proof (complete / damage-interrupt / zone-exit) -- HOST window, observe-only, ~30s."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Log(TEXT("afl.Extract.Test.Run -- starting (see AFL_EXTRACTRUN lines in LogAFLCombat)."));
				UAFLExtractTestRunner::RunInWorld(World);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
