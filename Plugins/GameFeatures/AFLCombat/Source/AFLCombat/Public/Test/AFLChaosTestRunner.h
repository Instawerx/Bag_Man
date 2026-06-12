// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLChaosTestRunner.generated.h"

class APlayerController;
class APawn;
class UAbilitySystemComponent;
class UAFLWalletComponent;
class UAFLMatchPhaseComponent;
class AAFLExtractionZone;

/**
 * UAFLChaosTestRunner  (P2 close-out -- two HOST-window observe-only harnesses)
 *
 *  afl.Overload.Test.Run (AFL_OVERLOADRUN): carry energy -> lethal self-damage -> assert SURVIVE
 *    (no Status.Death) + burst (conservation) + Health restored to the floor + State.Overloaded up
 *    then clears; then ZERO energy -> lethal self-damage -> assert REAL death fires.
 *
 *  afl.Chaos.Test.Run (AFL_CHAOSRUN): the stress object exists -> grab -> assert State.Carrying.
 *    Vulnerable + State.Carrying.StressObject applied -> damage the carrier, assert x1.3 taken
 *    (proven arithmetic) -> seed energy + ForceWindow + extract-while-carrying, assert Watts ==
 *    energy x 10 x 1.5 (conservation) -> instability (cvar-compressed) repositions.
 *
 * Both share the FSM skeleton + the extract-runner primitives. RunMode picks which legs run.
 */
UCLASS()
class AFLCOMBAT_API UAFLChaosTestRunner : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	enum class ERunMode : uint8 { Overload, Chaos };
	static void RunInWorld(UWorld* World, ERunMode Mode);

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return bRunning; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return WorldPtr.Get(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLChaosTestRunner, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EStep : uint8
	{
		// overload legs
		OV_Seed, OV_Kill, OV_AssertSurvive, OV_AwaitClear, OV_DrainToZero, OV_KillForReal, OV_AssertDeath,
		// chaos legs: grab -> assert carrier+vuln-tag -> extract(x1.5, 1s) -> damage-drops-it -> instability
		CH_Grab, CH_AssertCarrier, CH_SeedExtract, CH_AssertExtractMult, CH_AssertVuln,
		CH_Instability, CH_AssertReposition,
		Finish
	};

	bool StartRun(UWorld* World, ERunMode Mode);
	void FinishRun();

	void PressGrab();
	int32 CountPickups() const;
	float ReadCarriedEnergy() const;
	float ReadHealth() const;
	float ReadMaxHealth() const;
	int32 ReadWatts() const;
	bool HasTag(const FGameplayTag& Tag) const;
	bool IsDead() const;
	AActor* FindStressObject() const;
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
	TWeakObjectPtr<AActor> StressObject;
	FVector StressStartLoc = FVector::ZeroVector;
	FVector OverloadSpawnLoc = FVector::ZeroVector;

	ERunMode Mode = ERunMode::Overload;
	FGameplayMessageListenerHandle CollectListener;   // overload re-collect conservation proof
	float ReCollectedSum = 0.0f;

	float DrainRestore = 5.0f;
	float InstabilityRestore = 45.0f;
	int32 WattsBeforeExtract = 0;
	float EnergyAtKill = 0.0f;
	float HealthFloorExpected = 0.0f;

	EStep Step = EStep::OV_Seed;
	float StepTimer = 0.0f;
	int32 ChecksTotal = 0;
	int32 ChecksFailed = 0;
	bool bRunning = false;
	bool bExtractStarted = false;

	static TWeakObjectPtr<UAFLChaosTestRunner> ActiveRun;
};
