// Copyright C12 AI Gaming. All Rights Reserved.

#include "Phases/AFLMatchPhaseComponent.h"

#include "AFLCombat.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "GameModes/LyraGameState.h"
#include "HAL/IConsoleManager.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchPhaseComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_ExtractionWindow_Driver, "AFL.GamePhase.Playing.ExtractionWindow");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowOpen_Driver, "Event.Extraction.WindowOpen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowClosed_Driver, "Event.Extraction.WindowClosed");

// Cadence (driver-owned) + duration (driver-owned now -- the window phase is a BP shell). Cvars so
// the harness compresses timings without a rebuild (afl.Phase.Test.Run uses 10/5).
static TAutoConsoleVariable<float> CVarAFLExtractWindowPeriod(
	TEXT("afl.Extract.WindowPeriod"),
	150.0f,
	TEXT("Seconds between extraction-window OPENINGS (read per schedule -- a change takes at the next boundary)."));

static TAutoConsoleVariable<float> CVarAFLExtractWindowDuration(
	TEXT("afl.Extract.WindowDuration"),
	60.0f,
	TEXT("Seconds an extraction window stays OPEN (read by the driver when it opens a window)."));

namespace
{
	// The BP phase shells (children of ULyraGamePhaseAbility -- the C++ subclass boundary). Authored
	// in Stage 2; resolved by soft path so a missing asset degrades to a logged no-op, not a crash.
	const TCHAR* PlayingPhasePath = TEXT("/Game/BagMan/Phases/BP_AFL_Phase_Playing.BP_AFL_Phase_Playing_C");
	const TCHAR* WindowPhasePath  = TEXT("/Game/BagMan/Phases/BP_AFL_Phase_ExtractionWindow.BP_AFL_Phase_ExtractionWindow_C");

	// THE LYRA PHASE WALL: every C++ entry point to drive a phase from outside LyraGame is blocked --
	// StartPhase/WhenPhase* are public but NOT LYRAGAME_API-exported (link error), and the K2_* UFUNCTION
	// equivalents are exported but PROTECTED (access error). The ONLY reachable surface is IsPhaseActive
	// (public + exported). So we invoke the protected K2_ UFUNCTIONs REFLECTIVELY via ProcessEvent --
	// the same mechanism a Blueprint node compiles to; it ignores both C++ access control and the export
	// boundary. The param structs below mirror each UFUNCTION's parameter layout exactly.
	struct FK2StartPhaseParams
	{
		TSubclassOf<ULyraGamePhaseAbility> Phase;
		FLyraGamePhaseDynamicDelegate PhaseEnded;
	};
	struct FK2WhenPhaseParams
	{
		FGameplayTag PhaseTag;
		EPhaseTagMatchType MatchType = EPhaseTagMatchType::ExactMatch;
		FLyraGamePhaseTagDynamicDelegate WhenPhase;
	};

	void ReflectStartPhase(ULyraGamePhaseSubsystem* Sub, TSubclassOf<ULyraGamePhaseAbility> Phase)
	{
		if (!Sub || !Phase) { return; }
		if (UFunction* Fn = Sub->FindFunction(TEXT("K2_StartPhase")))
		{
			FK2StartPhaseParams Params;
			Params.Phase = Phase;
			Sub->ProcessEvent(Fn, &Params);
		}
	}

	// Even IsPhaseActive (public + BlueprintCallable) does not LINK from outside LyraGame -- the
	// subsystem class carries no LYRAGAME_API, so NONE of its member symbols cross the DLL boundary,
	// UFUNCTION or not. So it too goes through reflection. Param struct = (FGameplayTag, bool ReturnValue).
	// (The shared implementation lives in the UAFLMatchPhaseComponent::IsPhaseActiveReflected static so
	// the harness TU can call it too.)
	struct FK2IsPhaseActiveParams
	{
		FGameplayTag PhaseTag;
		bool ReturnValue = false;
	};
}

bool UAFLMatchPhaseComponent::IsPhaseActiveReflected(const UWorld* World, const FGameplayTag& PhaseTag)
{
	const ULyraGamePhaseSubsystem* Sub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(World);
	if (!Sub) { return false; }
	if (UFunction* Fn = Sub->FindFunction(TEXT("IsPhaseActive")))
	{
		FK2IsPhaseActiveParams Params;
		Params.PhaseTag = PhaseTag;
		const_cast<ULyraGamePhaseSubsystem*>(Sub)->ProcessEvent(Fn, &Params);
		return Params.ReturnValue;
	}
	return false;
}


UAFLMatchPhaseComponent::UAFLMatchPhaseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	// Soft-resolve the BP shells (StaticLoadClass at CDO time is fine for a server component). A BP
	// child of this component may override these in the details panel.
	PlayingPhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, PlayingPhasePath);
	WindowPhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, WindowPhasePath);
}

void UAFLMatchPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	AGameStateBase* GS = GetGameStateChecked<AGameStateBase>();
	if (!GS->HasAuthority())
	{
		return; // pure server driver (the experience row is server-flagged too).
	}

	ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld());
	if (!PhaseSub || !PlayingPhaseClass)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_PHASE: driver could not start -- subsystem %s / PlayingPhaseClass %s (BP shell missing?)."),
			PhaseSub ? TEXT("ok") : TEXT("MISSING"), PlayingPhaseClass ? TEXT("ok") : TEXT("MISSING"));
		return;
	}

	// Start the match shell via reflection (the wall is documented at the param structs above).
	ReflectStartPhase(PhaseSub, PlayingPhaseClass);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: match shell started (AFL.GamePhase.Playing); window cadence armed."));
	ScheduleNextWindow();
}

void UAFLMatchPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindowOpenTimer);
		World->GetTimerManager().ClearTimer(WindowDurationTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLMatchPhaseComponent::ScheduleNextWindow()
{
	// Read the period PER SCHEDULE so a mid-match cvar change lands at the next boundary.
	const float Period = FMath::Max(0.5f, CVarAFLExtractWindowPeriod.GetValueOnGameThread());
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(WindowOpenTimer, this, &UAFLMatchPhaseComponent::OpenWindow, Period, /*loop=*/false);
	}
}

void UAFLMatchPhaseComponent::OpenWindow()
{
	ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld());
	if (PhaseSub && WindowPhaseClass && !IsWindowActive())
	{
		ReflectStartPhase(PhaseSub, WindowPhaseClass); // zone WhenPhaseStartsOrIsActive observers fire -> Active
		bWindowOpen = true;
		BroadcastWindowAnnounce(TAG_Event_Extraction_WindowOpen_Driver);

		// The driver owns the DURATION now (the phase is a BP shell). Arm the close timer.
		const float Duration = FMath::Max(0.1f, CVarAFLExtractWindowDuration.GetValueOnGameThread());
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(WindowDurationTimer, this, &UAFLMatchPhaseComponent::CloseWindowNow, Duration, /*loop=*/false);
		}
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: extraction window OPEN (%.0fs)."), Duration);
	}
	ScheduleNextWindow(); // chain the next opening regardless (cadence continues).
}

void UAFLMatchPhaseComponent::CloseWindowNow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindowDurationTimer);
	}
	if (!bWindowOpen)
	{
		return;
	}

	// End the active window phase ability on the GameState ASC -> OnEndPhase fires -> zone
	// WhenPhaseEnds observers run (-> SetActive(false) -> handle sweep). Phase abilities carry no
	// AbilityTags (GamePhaseTag is a separate property), so cancel by CLASS via the exported
	// CancelAbilitiesByFunc.
	AGameStateBase* GS = GetGameStateChecked<AGameStateBase>();
	ULyraAbilitySystemComponent* GameState_ASC = GS->FindComponentByClass<ULyraAbilitySystemComponent>();
	if (GameState_ASC && WindowPhaseClass)
	{
		UClass* TargetClass = WindowPhaseClass.Get();
		GameState_ASC->CancelAbilitiesByFunc(
			[TargetClass](const ULyraGameplayAbility* Ability, FGameplayAbilitySpecHandle /*Handle*/)
			{
				return Ability && TargetClass && Ability->IsA(TargetClass);
			},
			/*bReplicateCancelAbility=*/true);
	}
	bWindowOpen = false;
	BroadcastWindowAnnounce(TAG_Event_Extraction_WindowClosed_Driver);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: extraction window CLOSED."));
}

bool UAFLMatchPhaseComponent::IsWindowActive() const
{
	return IsPhaseActiveReflected(GetWorld(), TAG_AFL_GamePhase_ExtractionWindow_Driver);
}

void UAFLMatchPhaseComponent::ForceWindowOpen()
{
	if (GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		OpenWindow();
	}
}

void UAFLMatchPhaseComponent::RescheduleCadence()
{
	if (GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		ScheduleNextWindow(); // reads afl.Extract.WindowPeriod fresh.
	}
}

void UAFLMatchPhaseComponent::ForceWindowClose()
{
	if (GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		CloseWindowNow();
	}
}

void UAFLMatchPhaseComponent::BroadcastWindowAnnounce(const FGameplayTag& EventTag) const
{
	UWorld* World = GetWorld();
	ALyraGameState* GS = World ? World->GetGameState<ALyraGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	FLyraVerbMessage Message;
	Message.Verb = EventTag;
	Message.Instigator = GS;

	// Clients: the GameState multicast rebroadcasts into each CLIENT's local message subsystem
	// (LyraGameState.cpp:103-114). Its NM_Client guard skips the LISTEN-SERVER HOST, so broadcast
	// locally on the server world too (the accolade-relay dual pattern). Dedicated server: the local
	// broadcast is harmless (no HUD).
	GS->MulticastReliableMessageToClients(Message);
	UGameplayMessageSubsystem::Get(World).BroadcastMessage(Message.Verb, Message);
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	UAFLMatchPhaseComponent* FindDriver(UWorld* World)
	{
		AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		return GS ? GS->FindComponentByClass<UAFLMatchPhaseComponent>() : nullptr;
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLExtractForceWindowCmd(
		TEXT("afl.Extract.ForceWindow"),
		TEXT("Match phases cycle 1: afl.Extract.ForceWindow open|close -- drive an extraction window deterministically (HOST/authority)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				UAFLMatchPhaseComponent* Driver = FindDriver(World);
				if (!Driver) { Ar.Log(TEXT("afl.Extract.ForceWindow -- no UAFLMatchPhaseComponent on the GameState (run on the HOST inside PIE).")); return; }
				const bool bOpen = Args.Num() > 0 && Args[0].Equals(TEXT("open"), ESearchCase::IgnoreCase);
				if (bOpen) { Driver->ForceWindowOpen(); Ar.Log(TEXT("afl.Extract.ForceWindow -- opened.")); }
				else       { Driver->ForceWindowClose(); Ar.Log(TEXT("afl.Extract.ForceWindow -- closed.")); }
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
