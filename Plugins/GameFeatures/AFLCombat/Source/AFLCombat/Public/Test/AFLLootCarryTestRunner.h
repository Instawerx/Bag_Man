// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLLootCarryTestRunner.generated.h"

class APlayerController;
class APawn;
class UAbilitySystemComponent;
class UAFLLootCarryComponent;
class AAFLLootCacheCarry;

/**
 * UAFLLootCarryTestRunner  (Loot-Carry Phase B -- afl.LootCarry.Test.Run, HOST window, observe-only)
 *
 * Scripted proof of the CARRY collect-channel (the extract-runner FSM skeleton). It pins
 * afl.Channel.DurationOverride to a known 2s so it can land its mid-channel interrupts deterministically,
 * spawns a fresh CARRY cache per leg, and drives the channel by the SAME GameplayEvent the grab fork sends
 * (Event.Loot.CollectChannel, Target = the cache). The operator only WATCHES; the harness asserts:
 *  LEG 1 COLLECT: send -> channel completes -> pool += 200 (the cache LootWatts) + the cache DESPAWNS
 *    (Decision B collect->despawn) + Event.Channel.Complete fired once.
 *  LEG 2 MOVE-CANCEL: send -> teleport the pawn >MaxMoveRadius mid-channel -> Event.Channel.Interrupted +
 *    pool UNCHANGED + the cache STILL alive (no grant, no despawn).
 *  LEG 3 DAMAGE-CANCEL: send -> self-damage (AFL.Combat.Damage) mid-channel -> Event.Channel.Interrupted +
 *    pool UNCHANGED + the cache alive. (This is the gate the 1.5s manual window kept missing.)
 *
 * Complete/Interrupted are REGISTERED Event.Channel.* listeners (the conservation shape), never log greps.
 * The cvar is restored at FinishRun. Observe-only: do NOT move the pawn during the run (the move-interrupt
 * is live -- a nudge would false-cancel a non-move leg).
 */
UCLASS()
class AFLCOMBAT_API UAFLLootCarryTestRunner : public UObject, public FTickableGameObject
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
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLLootCarryTestRunner, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EStep : uint8
	{
		L1_Start, L1_Wait, L1_Assert,
		L2_Start, L2_Mid, L2_Assert,
		L3_Start, L3_Mid, L3_Assert,
		Finish
	};

	bool StartRun(UWorld* World);
	void FinishRun();

	AAFLLootCacheCarry* SpawnCache();
	void SendCollectChannel(AActor* CacheActor);
	int32 ReadPool() const;
	void Console(const FString& Cmd);
	FString NetTag() const;
	void Marker(const FString& Msg) const;
	void Check(bool bPass, const FString& What);

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<UAFLLootCarryComponent> Carry;
	TWeakObjectPtr<AAFLLootCacheCarry> Cache;

	/** The pawn's run-start spot -- proven floor + the channel-start anchor for the move-away teleport
	 *  (extract-runner lesson: away-teleports must land on proven floor, not off the greybox edge). */
	FVector StartLocation = FVector::ZeroVector;

	FGameplayMessageListenerHandle CompleteListener;
	FGameplayMessageListenerHandle InterruptedListener;
	int32 CompleteCount = 0;
	int32 InterruptedCount = 0;
	int32 PoolAtLegStart = 0;

	float DurationCvarRestore = -1.0f;

	EStep Step = EStep::L1_Start;
	float StepTimer = 0.0f;
	int32 ChecksTotal = 0;
	int32 ChecksFailed = 0;
	bool bRunning = false;

	static TWeakObjectPtr<UAFLLootCarryTestRunner> ActiveRun;
};
