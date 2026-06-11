// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLExtractTestRunner.generated.h"

class APlayerController;
class APawn;
class UAbilitySystemComponent;
class UAFLWalletComponent;
class AAFLExtractionZone;

/**
 * UAFLExtractTestRunner  (extraction cycle 1 -- afl.Extract.Test.Run, HOST window, observe-only)
 *
 * Three scripted legs against the P2 cash-out (the energy-runner FSM skeleton):
 *  LEG 1 COMPLETE: seed 100 energy -> teleport into the runner-spawned zone -> channel 6s
 *    uninterrupted -> CarriedEnergy 0 + Watts delta EXACTLY energy x 10 (conservation form via
 *    GetWatts) + the O1 hard-lock sampled mid-channel (CMC GetMaxSpeed == 0).
 *  LEG 2 DAMAGE: seed 50 -> channel -> self-damage cheat at t=3 -> Failed + Watts unchanged +
 *    burst pickups exist BEFORE the magnet eats them, then the pawn FLEES immediately (the
 *    re-collect wrinkle is reported as INFO, never a false fail).
 *  LEG 3 ZONE-EXIT: seed -> channel -> teleport OUT at t=2 -> Failed + both tags down + energy
 *    RETAINED (== the recorded channel-start value) + no Watts delta.
 *
 * Drain cvar pinned to 0 for the run (seeding 100 crosses the overdrive threshold; an active
 * drain would corrupt the exact-1000-Watts conservation assert -- SetByCaller-at-apply makes 0
 * stick for the buff's lifetime). Restored at FinishRun. Complete/Failed proofs are REGISTERED
 * FLyraVerbMessage listeners (the conservation-law shape), never log greps.
 */
UCLASS()
class AFLCOMBAT_API UAFLExtractTestRunner : public UObject, public FTickableGameObject
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
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLExtractTestRunner, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EStep : uint8
	{
		Seed1, TeleportIn1, Activate1, MidSample1, AssertComplete1,
		Seed2, Activate2, Damage2, AssertBurst_Flee2, AssertFailed2,
		Seed3, TeleportIn3, Activate3, TeleportOut3, AssertCancel3,
		Finish
	};

	bool StartRun(UWorld* World);
	void FinishRun();

	bool ActivateExtract();
	float ReadCarriedEnergy() const;
	int32 ReadWatts() const;
	float ReadPawnMaxSpeed() const;          // GetMaxSpeed() -- the MovementStopped-aware read
	bool HasTag(const FGameplayTag& Tag) const;
	int32 CountPickups() const;
	void Console(const FString& Cmd);
	FString NetTag() const;
	void Marker(const FString& Msg) const;
	void Check(bool bPass, const FString& What);

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<UAFLWalletComponent> Wallet;
	TWeakObjectPtr<AAFLExtractionZone> Zone;
	FVector ZoneCenter = FVector::ZeroVector;
	/** Run-2 lesson: away-teleports must land on PROVEN floor -- 3000uu offsets ran off the greybox
	 *  edge and the pawn fell into the void. The pawn's run-start spot IS proven floor and sits
	 *  1200uu from the zone (well outside the 300uu sphere) -- it is the one safe anchor. */
	FVector StartLocation = FVector::ZeroVector;

	FGameplayMessageListenerHandle CompleteListener;
	FGameplayMessageListenerHandle FailedListener;
	FGameplayMessageListenerHandle CollectListener;   // leg-2 re-collect INFO only
	int32 CompleteCount = 0;
	int32 FailedCount = 0;
	int32 ReCollectedCount = 0;
	float ReCollectedSum = 0.0f;

	float DrainCvarRestore = 5.0f;
	int32 WattsAtChannelStart = 0;
	float EnergyAtChannelStart = 0.0f;

	EStep Step = EStep::Seed1;
	float StepTimer = 0.0f;
	int32 ChecksTotal = 0;
	int32 ChecksFailed = 0;
	bool bRunning = false;

	static TWeakObjectPtr<UAFLExtractTestRunner> ActiveRun;
};
