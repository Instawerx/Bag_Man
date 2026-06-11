// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLPhaseTestRunner.generated.h"

class APlayerController;
class APawn;
class UAbilitySystemComponent;
class UAFLWalletComponent;
class UAFLMatchPhaseComponent;
class AAFLExtractionZone;

/**
 * UAFLPhaseTestRunner  (match phases cycle 1 -- afl.Phase.Test.Run, HOST window, observe-only)
 *
 * Server-side FSM proving the zone breathes on the phase clock (reads IsPhaseActive directly).
 * Arms compressed cvars (Period 10 / Duration 5) + drives windows with ForceWindow for determinism.
 *
 *  LEG 1 (closed at start): Playing active + zone Inactive + standing INSIDE dispenses NO tag.
 *  LEG 2 (force open):      window opens -> zone Active + tag dispensed + a full channel COMPLETES
 *                           inside the window (Watts conservation).
 *  LEG 3 (force close mid): second channel -> ForceWindow close at t=2 -> GA cancels (Failed) +
 *                           energy RETAINED + handles swept.
 *  LEG 4 (cadence):         the driver re-opens a window at the next boundary on its own.
 *
 * Requires the experience UAFLMatchPhaseComponent driver (the Playing shell + ForceWindow). The
 * zone is runner-spawned and observes the driver's window phase (the integration proof).
 */
UCLASS()
class AFLCOMBAT_API UAFLPhaseTestRunner : public UObject, public FTickableGameObject
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
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLPhaseTestRunner, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EStep : uint8
	{
		AssertClosedAtStart,
		ForceOpen, AssertActive_Channel, AssertChannelComplete,
		Seed3, Channel3, ForceClose3, AssertCancel3,
		AwaitCadence, AssertCadence,
		Finish
	};

	bool StartRun(UWorld* World);
	void FinishRun();

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

	FGameplayMessageListenerHandle WindowOpenListener;
	FGameplayMessageListenerHandle WindowClosedListener;
	int32 WindowOpenCount = 0;
	int32 WindowClosedCount = 0;

	float PeriodRestore = 150.0f;
	float DurationRestore = 60.0f;
	float DrainRestore = 5.0f;
	int32 WattsAtChannelStart = 0;
	float EnergyAtChannelStart = 0.0f;

	EStep Step = EStep::AssertClosedAtStart;
	float StepTimer = 0.0f;
	int32 ChecksTotal = 0;
	int32 ChecksFailed = 0;
	bool bRunning = false;

	static TWeakObjectPtr<UAFLPhaseTestRunner> ActiveRun;
};
