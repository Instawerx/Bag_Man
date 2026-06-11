// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLEnergyTestRunner.generated.h"

class APlayerController;
class APawn;
class UAbilitySystemComponent;

/**
 * UAFLEnergyTestRunner  (energy drops cycle 1 -- afl.Energy.Test.Run, HOST window, observe-only)
 *
 * One command scripts the whole P2-loop proof: burst spawn -> magnetic pull -> collection ->
 * attribute delta -> overdrive threshold crossing -> death burst + victim reduction. The runner
 * is HOST-side (spawns, teleports and the damage cheat are authority ops -- the mirror of the
 * lag-comp runner's client-side guard). Attribute reads are direct C++ (same module as the set --
 * alias-proof by construction; the stringly reader is only needed cross-module). Threshold proof
 * is a REGISTERED GameplayMessage listener, not a log grep. AFL_ENERGYRUN markers + SUMMARY +
 * COMPLETE; ~20s.
 */
UCLASS()
class AFLCOMBAT_API UAFLEnergyTestRunner : public UObject, public FTickableGameObject
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
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLEnergyTestRunner, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EStep : uint8
	{
		Burst1, AssertCountStable, TeleportIn, AssertPull, AssertCollected_Burst2,
		AssertCollected2, DeathLeg, AssertDeathBurst, Finish
	};

	bool StartRun(UWorld* World);
	void FinishRun();

	int32 CountPickups(float& OutMeanDistToPawn) const;
	float ReadCarriedEnergy() const;
	void Console(const FString& Cmd);
	FString NetTag() const;
	void Marker(const FString& Msg) const;
	void Check(bool bPass, const FString& What);

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	FGameplayMessageListenerHandle ThresholdListener;
	bool bThresholdSeen = false;

	/** Run-1 lesson: per-player attribute expectations break the moment the OTHER player collects
	 *  (the client vacuumed 2/5 and every downstream number shifted while total conservation held
	 *  EXACTLY). v2 asserts CONSERVATION via a registered Event.Energy.Collected listener --
	 *  collector-agnostic, operator-proof. */
	FGameplayMessageListenerHandle CollectListener;
	int32 CollectedCount = 0;
	float CollectedSum = 0.0f;
	float CollectedSumAtDeath = 0.0f;
	FVector RingCenter = FVector::ZeroVector;

	EStep Step = EStep::Burst1;
	float StepTimer = 0.0f;
	float Energy0 = 0.0f;
	float EnergyPreDeath = 0.0f;
	float MeanDistAtBurst = 0.0f;
	int32 ChecksTotal = 0;
	int32 ChecksFailed = 0;
	bool bRunning = false;

	static TWeakObjectPtr<UAFLEnergyTestRunner> ActiveRun;
};
