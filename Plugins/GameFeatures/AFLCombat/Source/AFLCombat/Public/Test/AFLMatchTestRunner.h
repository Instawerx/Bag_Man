// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLMatchTestRunner.generated.h"

class APlayerController;
class APawn;
class UAbilitySystemComponent;
class UAFLWalletComponent;
class UAFLMatchPhaseComponent;
class AAFLExtractionZone;

/**
 * UAFLMatchTestRunner  (match spine cycle 1 -- afl.Match.Test.Run, HOST window, observe-only)
 *
 * The FULL spine in one PIE session (the phase-wall reflection + the proven extract primitives):
 *  LEG 1 (warmup): Warmup active + fire BLOCKED (Pulse activate returns false) + zone Inactive.
 *  LEG 2 (active):  ~Warmup elapses -> auto-chain to Playing (Warmup inactive, Playing active) +
 *                   fire now WORKS + a window opens on cadence + a full extraction COMPLETES
 *                   (Watts conservation -- the extract-runner shape).
 *  LEG 3 (postgame): ActiveDuration elapses -> PostGame active + Playing & .ExtractionWindow both
 *                   inactive (cancel) + Event.Match.Ended broadcast (dual) with the Watts-earned
 *                   payload matching the leg-2 delta + fire BLOCKED again (Ended freeze).
 *  LEG 4 (terminal): no window reopens under PostGame (cleared cadence).
 *
 * Compressed cvars (Warmup 3 / Active 12 / WindowPeriod 4 / WindowDuration 3) + the window cadence
 * is kept clear of the ActiveDuration boundary (the leg-isolation lesson). Requires the experience
 * UAFLMatchPhaseComponent driver.
 */
UCLASS()
class AFLCOMBAT_API UAFLMatchTestRunner : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static void RunInWorld(UWorld* World);

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return bRunning; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return WorldPtr.Get(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLMatchTestRunner, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EStep : uint8
	{
		AssertWarmup,
		AwaitPlaying, AssertPlaying_Channel, AssertExtraction,
		AwaitPostGame, AssertPostGame,
		AssertTerminal,
		Finish
	};

	bool StartRun(UWorld* World);
	void FinishRun();

	bool TryFire();              // Pulse activate -> true if it ACTIVATED (blocked tags make it false)
	bool ActivateExtract();
	bool IsPhaseActive(const FGameplayTag& PhaseTag) const;
	bool HasTag(const FGameplayTag& Tag) const;
	float ReadCarriedEnergy() const;
	int32 ReadWatts() const;
	void Console(const FString& Cmd);
	FString NetTag() const;
	void Marker(const FString& Msg) const;
	void Check(bool bPass, const FString& What);

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<UAFLWalletComponent> Wallet;
	TWeakObjectPtr<UAFLMatchPhaseComponent> Driver;
	TWeakObjectPtr<AAFLExtractionZone> Zone;
	FVector ZoneCenter = FVector::ZeroVector;

	FGameplayMessageListenerHandle MatchEndedListener;
	int32 MatchEndedCount = 0;
	double MatchEndedWatts = -1.0;

	float WarmupRestore = 30.0f;
	float ActiveRestore = 480.0f;
	float PeriodRestore = 150.0f;
	float DurationRestore = 60.0f;
	float DrainRestore = 5.0f;
	int32 WattsBeforeExtract = 0;
	int32 ExtractDelta = 0;
	bool bExtractDone = false;

	EStep Step = EStep::AssertWarmup;
	float StepTimer = 0.0f;
	int32 ChecksTotal = 0;
	int32 ChecksFailed = 0;
	bool bRunning = false;

	static TWeakObjectPtr<UAFLMatchTestRunner> ActiveRun;
};
