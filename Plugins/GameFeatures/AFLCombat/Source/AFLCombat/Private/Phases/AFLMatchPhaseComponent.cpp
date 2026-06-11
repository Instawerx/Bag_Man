// Copyright C12 AI Gaming. All Rights Reserved.

#include "Phases/AFLMatchPhaseComponent.h"

#include "AFLCombat.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraGameState.h"
#include "HAL/IConsoleManager.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLMatchPhaseComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Warmup_Driver, "AFL.GamePhase.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_Playing_Driver, "AFL.GamePhase.Playing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_PostGame_Driver, "AFL.GamePhase.PostGame");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_ExtractionWindow_Driver, "AFL.GamePhase.Playing.ExtractionWindow");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowOpen_Driver, "Event.Extraction.WindowOpen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_WindowClosed_Driver, "Event.Extraction.WindowClosed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Match_Ended_Driver, "Event.Match.Ended");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Warmup_Driver, "State.Match.Warmup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Match_Ended_Driver, "State.Match.Ended");

// Match spine durations (S9) + the window cadence/duration (extraction cycle 1). Cvars so the
// harness compresses without a rebuild (afl.Match.Test.Run uses Warmup 3 / Active 10 / window 4/3).
static TAutoConsoleVariable<float> CVarAFLMatchWarmupDuration(
	TEXT("afl.Match.WarmupDuration"),
	30.0f,
	TEXT("Seconds of pre-match Warmup (fire/movement frozen) before chaining to Playing."));

static TAutoConsoleVariable<float> CVarAFLMatchActiveDuration(
	TEXT("afl.Match.ActiveDuration"),
	480.0f,
	TEXT("Seconds the match stays in Playing before PostGame (terminal)."));

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
	const TCHAR* WarmupPhasePath  = TEXT("/Game/BagMan/Phases/BP_AFL_Phase_Warmup.BP_AFL_Phase_Warmup_C");
	const TCHAR* PlayingPhasePath = TEXT("/Game/BagMan/Phases/BP_AFL_Phase_Playing.BP_AFL_Phase_Playing_C");
	const TCHAR* WindowPhasePath  = TEXT("/Game/BagMan/Phases/BP_AFL_Phase_ExtractionWindow.BP_AFL_Phase_ExtractionWindow_C");
	const TCHAR* PostGamePhasePath= TEXT("/Game/BagMan/Phases/BP_AFL_Phase_PostGame.BP_AFL_Phase_PostGame_C");

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
	WarmupPhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, WarmupPhasePath);
	PlayingPhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, PlayingPhasePath);
	WindowPhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, WindowPhasePath);
	PostGamePhaseClass = StaticLoadClass(ULyraGamePhaseAbility::StaticClass(), nullptr, PostGamePhasePath);
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
	if (!PhaseSub || !WarmupPhaseClass || !PlayingPhaseClass)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_PHASE: driver could not start -- subsystem %s / Warmup %s / Playing %s (BP shells missing?)."),
			PhaseSub ? TEXT("ok") : TEXT("MISSING"), WarmupPhaseClass ? TEXT("ok") : TEXT("MISSING"), PlayingPhaseClass ? TEXT("ok") : TEXT("MISSING"));
		return;
	}

	StartSpineFromWarmup();
}

void UAFLMatchPhaseComponent::StartSpineFromWarmup()
{
	// THE SPINE STARTS AT WARMUP. Start the phase, freeze fire/movement on all pawns, arm the
	// warmup->playing chain timer (reads the cvar FRESH so RestartMatch can compress it).
	bMatchEnded = false;
	StartPhaseByClass(WarmupPhaseClass, TAG_AFL_GamePhase_Warmup_Driver);
	GrantMatchTagToAllPawns(TAG_State_Match_Warmup_Driver);
	const float Warmup = FMath::Max(0.1f, CVarAFLMatchWarmupDuration.GetValueOnGameThread());
	GetWorld()->GetTimerManager().SetTimer(WarmupTimer, this, &UAFLMatchPhaseComponent::EnterPlaying, Warmup, /*loop=*/false);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: WARMUP started (%.0fs; fire/movement frozen)."), Warmup);
}

void UAFLMatchPhaseComponent::RestartMatch()
{
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority())
	{
		return;
	}
	// Tear down: clear all timers, remove both match tags from everyone, end any live phase by
	// starting Warmup fresh (which cancels non-ancestor siblings -- but PostGame is a sibling of
	// Warmup too, so starting Warmup cancels PostGame/Playing/.ExtractionWindow cleanly).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarmupTimer);
		World->GetTimerManager().ClearTimer(ActiveTimer);
		World->GetTimerManager().ClearTimer(WindowOpenTimer);
		World->GetTimerManager().ClearTimer(WindowDurationTimer);
	}
	RemoveMatchTagFromAllPawns(TAG_State_Match_Warmup_Driver);
	RemoveMatchTagFromAllPawns(TAG_State_Match_Ended_Driver);
	bWindowOpen = false;
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: RESTART -- spine reset, re-entering Warmup with fresh cvars."));
	StartSpineFromWarmup();
}

void UAFLMatchPhaseComponent::EnterPlaying()
{
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority() || bMatchEnded)
	{
		return;
	}
	// Lift the warmup freeze, go active (auto-cancels Warmup), snapshot Watts, arm the cadence + the
	// match-duration timer.
	RemoveMatchTagFromAllPawns(TAG_State_Match_Warmup_Driver);
	StartPhaseByClass(PlayingPhaseClass, TAG_AFL_GamePhase_Playing_Driver);
	SnapshotMatchStartWatts();
	ScheduleNextWindow();
	const float Active = FMath::Max(0.1f, CVarAFLMatchActiveDuration.GetValueOnGameThread());
	GetWorld()->GetTimerManager().SetTimer(ActiveTimer, this, &UAFLMatchPhaseComponent::EnterPostGame, Active, /*loop=*/false);
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: PLAYING started (%.0fs; window cadence armed, Watts snapshotted)."), Active);
}

void UAFLMatchPhaseComponent::EnterPostGame()
{
	if (!GetGameStateChecked<AGameStateBase>()->HasAuthority() || bMatchEnded)
	{
		return;
	}
	bMatchEnded = true; // the cadence (ScheduleNextWindow/OpenWindow) no-ops from here -- terminal.

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindowOpenTimer);   // C subtlety: no window reopens under PostGame
		World->GetTimerManager().ClearTimer(WindowDurationTimer);
	}

	// Starting PostGame auto-cancels Playing AND .ExtractionWindow (non-ancestor siblings) -> the
	// zone's WhenPhaseEnds observer fires -> SetZoneActive(false) -> handle sweep -> any channeler's
	// GA self-cancels. NO explicit window force-close needed (the cancel chain, proven in cycle 1).
	StartPhaseByClass(PostGamePhaseClass, TAG_AFL_GamePhase_PostGame_Driver);
	bWindowOpen = false;
	GrantMatchTagToAllPawns(TAG_State_Match_Ended_Driver);     // fire/movement frozen, terminal
	BroadcastMatchEnded();                                     // per-player dual-broadcast w/ Watts
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: POSTGAME (terminal -- Playing+Window cancelled, match frozen, ended-broadcast sent)."));
}

void UAFLMatchPhaseComponent::StartPhaseByClass(TSubclassOf<ULyraGamePhaseAbility> PhaseClass, const FGameplayTag& PhaseTag)
{
	ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld());
	if (PhaseSub && PhaseClass && !IsPhaseActiveReflected(GetWorld(), PhaseTag)) // re-entrancy guard
	{
		ReflectStartPhase(PhaseSub, PhaseClass);
	}
}

void UAFLMatchPhaseComponent::GrantMatchTagToAllPawns(const FGameplayTag& Tag)
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(*It))
		{
			ASC->AddLooseGameplayTag(Tag);
		}
	}
}

void UAFLMatchPhaseComponent::RemoveMatchTagFromAllPawns(const FGameplayTag& Tag)
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(*It))
		{
			ASC->RemoveLooseGameplayTag(Tag);
		}
	}
}

void UAFLMatchPhaseComponent::SnapshotMatchStartWatts()
{
	MatchStartWatts.Reset();
	if (const AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (const UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr)
			{
				MatchStartWatts.Add(PS, Wallet->GetWatts());
			}
		}
	}
}

void UAFLMatchPhaseComponent::BroadcastMatchEnded()
{
	// One dual-broadcast PER PLAYER: Target = their PlayerState, Magnitude = this-match Watts delta
	// (GetWatts() - snapshot). Each client's announce widget filters Target == its own PlayerState.
	if (const AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			const UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
			const int32* StartWatts = PS ? MatchStartWatts.Find(PS) : nullptr;
			const int32 Earned = (Wallet && StartWatts) ? FMath::Max(0, Wallet->GetWatts() - *StartWatts) : 0;
			BroadcastAnnounce(TAG_Event_Match_Ended_Driver, PS, static_cast<double>(Earned));
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_PHASE: match-end for %s -- %d Watts this match."), *GetNameSafe(PS), Earned);
		}
	}
}

void UAFLMatchPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarmupTimer);
		World->GetTimerManager().ClearTimer(ActiveTimer);
		World->GetTimerManager().ClearTimer(WindowOpenTimer);
		World->GetTimerManager().ClearTimer(WindowDurationTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLMatchPhaseComponent::ScheduleNextWindow()
{
	if (bMatchEnded) { return; } // terminal: no windows reopen under PostGame.
	// Read the period PER SCHEDULE so a mid-match cvar change lands at the next boundary.
	const float Period = FMath::Max(0.5f, CVarAFLExtractWindowPeriod.GetValueOnGameThread());
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(WindowOpenTimer, this, &UAFLMatchPhaseComponent::OpenWindow, Period, /*loop=*/false);
	}
}

void UAFLMatchPhaseComponent::OpenWindow()
{
	if (bMatchEnded) { return; } // terminal guard (a timer that slipped through).
	ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld());
	if (PhaseSub && WindowPhaseClass && !IsWindowActive())
	{
		ReflectStartPhase(PhaseSub, WindowPhaseClass); // zone WhenPhaseStartsOrIsActive observers fire -> Active
		bWindowOpen = true;
		BroadcastAnnounce(TAG_Event_Extraction_WindowOpen_Driver);

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
	BroadcastAnnounce(TAG_Event_Extraction_WindowClosed_Driver);
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

void UAFLMatchPhaseComponent::BroadcastAnnounce(const FGameplayTag& EventTag, UObject* Target, double Magnitude) const
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
	Message.Target = Target;       // match-end: the player this payload is for (window announces: null)
	Message.Magnitude = Magnitude; // match-end: this-match Watts

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

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLMatchRestartCmd(
		TEXT("afl.Match.Restart"),
		TEXT("Match spine cycle 1: restart the spine from Warmup NOW, reading afl.Match.WarmupDuration/ActiveDuration fresh (HOST/authority). Used by afl.Match.Test.Run for a deterministic compressed run."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				UAFLMatchPhaseComponent* Driver = FindDriver(World);
				if (!Driver) { Ar.Log(TEXT("afl.Match.Restart -- no UAFLMatchPhaseComponent on the GameState (HOST inside PIE).")); return; }
				Driver->RestartMatch();
				Ar.Log(TEXT("afl.Match.Restart -- spine restarted from Warmup."));
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
