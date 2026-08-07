// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLDataLayerStreamTestHarness.generated.h"

class UDataLayerAsset;
class UDataLayerInstance;
enum class EDataLayerRuntimeState : uint8;

/**
 * UAFLDataLayerStreamTestHarness  (data layer streaming self-test)
 *
 * Cheat-driven FSM behind `afl.DL.StreamTest` (the afl.Interaction.Test.Run registration pattern).
 * Proves the CONSEQUENCE half of the district architecture: that an activated runtime data layer's
 * ACTORS actually stream into the world.
 *
 * The state half is already proven (Block 229: Unloaded -> Activated -> Unloaded on both target and
 * effective state, with explicit transition lines). What was never shown is that geometry follows.
 * This session has repeatedly found correct state with nothing consuming it, so the two are logged
 * SEPARATELY at every phase -- "state changed but nothing streamed" and "state never changed" are
 * different bugs with different fixes, and a single combined verdict would hide which one you have.
 *
 *   AFL_TEST[Pn] ...            per-phase actor count + effective layer state
 *   AFL_TEST VERDICT ...        PASS only if P0 absent, P3 present, P5 absent
 *   AFL_TEST COMPLETE           end marker (operator stops PIE after this)
 *
 * Honesty contract (the false-green lessons): presence is asserted off a live TActorIterator over
 * the world, never off "the command was issued" and never off the actor's name. An actor is PRESENT
 * only if it is instantiated in the world AND reports membership of the layer under test via
 * AActor::ContainsDataLayer. The location of the first hit is logged so a count cannot be trusted
 * on its own.
 */
UCLASS()
class AFLGAMECORE_API UAFLDataLayerStreamTestHarness : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Entry point for the console command. Refuses to start while a prior run is live. */
	static void RunInWorld(UWorld* World, const FString& LayerAssetPath);

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return bRunning; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return WorldPtr.Get(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLDataLayerStreamTestHarness, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EPhase : uint8
	{
		P0_Baseline,
		P1_Activate,
		P2_Settle,
		P3_Verify,
		P4_Deactivate,
		P4b_SettleClear,
		P5_VerifyClear,
		P6_Verdict,
		Done
	};

	bool StartRun(UWorld* World, const FString& LayerAssetPath);
	void FinishRun();
	void EnterPhase(EPhase Next);

	/** Live world query. Returns the number of instantiated actors reporting membership of the layer
	 *  under test; writes the first hit's world location so presence is never a bare count. */
	int32 CountLayerActorsInWorld(FVector& OutFirstLocation) const;

	EDataLayerRuntimeState EffectiveState() const;
	EDataLayerRuntimeState TargetState() const;

	/** True once World Partition reports streaming quiescent. Never true before MinSettleFrames. */
	bool StreamingSettled() const;

	static const TCHAR* StateName(EDataLayerRuntimeState State);

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<const UDataLayerAsset> LayerAsset;

	bool  bRunning      = false;
	EPhase Phase        = EPhase::P0_Baseline;
	float PhaseElapsed  = 0.0f;
	int32 PhaseFrames   = 0;

	/** Recorded per-phase counts. -1 = phase never ran, which is itself a FAIL the verdict names. */
	int32 P0Count = -1;
	int32 P3Count = -1;
	int32 P5Count = -1;
	bool  bActivateAccepted   = false;
	bool  bDeactivateAccepted = false;
	bool  bSettleTimedOut     = false;
};
