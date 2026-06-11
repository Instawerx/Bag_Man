// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLLagCompTestRunner.generated.h"

class APlayerController;
class ULyraAbilitySystemComponent;

/**
 * UAFLLagCompTestRunner  (2-client cycle 3b -- the operator-observe-only lag-comp run)
 *
 * One command in the CLIENT window (`afl.LagComp.Test.Run`) replaces the whole manual protocol:
 * cohort latency, instrument arming, pinned volleys, and the telemetry flick pair are all scripted,
 * so the operator only watches. Why this works in one command: console VARIABLES are process-global
 * in PIE-under-one-process (one IConsoleManager) -- the client-window runner arms the SERVER-side
 * sweep (afl.LagComp.SweepDiagnostic) directly -- and `Net PktLag` is a runtime net-driver exec, so
 * latency cohorts apply mid-session with no Editor Preferences dance and no PIE restarts.
 *
 * TOPOLOGY NOTE (the reason this runner exists): client-side `Net PktLag` delays the CLIENT's
 * OUTGOING packets only -- a clean one-leg topology we fully control. Pre-registered prediction for
 * the sweep under it: the claim ages by ~the full added latency in transit while the client's dummy
 * view stays loopback-fresh, so the best-matching rewind dt should track the MEASURED RTT. The first
 * sweep (editor Network Emulation, both-legs configured) measured winning dt = 0 -- evidence that
 * editor emulation inflated ping without delaying actor replication. If this runner's clean topology
 * confirms its own prediction, the rewind formula story is coherent (rewind = claim transit + view
 * age, both measurable) and the editor-emulation anomaly is quarantined as a tooling artifact.
 *
 * Per cohort {15, 40, 100}ms PktLag: apply -> settle 5s (ping smoothing) -> log measured ping ->
 * 10 pinned shots (real ability through the input seam; afl.LagComp.AimAtDummy) -> one synthesized
 * flick pair (afl.LagComp.PinMirror flip between two presses 0.2s apart = 900 deg/s if the second
 * press clears the fire cooldown). Markers: AFL_LCRUN[...] lines + SUMMARY + COMPLETE. Cleanup
 * restores PktLag=0 and disarms every cvar it set.
 */
UCLASS()
class AFLCOMBAT_API UAFLLagCompTestRunner : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Entry point. CLIENT window only (the firing + the PktLag leg must be client-side). */
	static void RunInWorld(UWorld* World);

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return bRunning; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return WorldPtr.Get(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLLagCompTestRunner, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EPhaseStep : uint8 { Apply, Settle, Volley, FlickArm, FlickFire, NextCohort, Finish };

	bool StartRun(UWorld* World);
	void FinishRun();

	void PressFire();                          // press+release through the Lyra input seam (real pipeline)
	void SetGlobalCVar(const TCHAR* Name, int32 Value) const;
	float ReadOwnPingMs() const;
	FString NetTag() const;
	void Marker(const FString& Msg) const;     // AFL_LCRUN log + on-screen narration
	void Check(bool bPass, const FString& What);

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<ULyraAbilitySystemComponent> LyraASC;

	static constexpr int32 NumCohorts = 3;
	static constexpr int32 CohortPktLagMs[NumCohorts] = { 15, 40, 100 };
	static constexpr int32 ShotsPerVolley = 10;
	static constexpr float SettleSeconds = 5.0f;
	static constexpr float VolleySpacing = 0.6f;
	static constexpr float FlickSpacing = 0.2f;  // 180 deg / 0.2s = 900 deg/s > the 720 budget

	int32 CohortIdx = 0;
	EPhaseStep Step = EPhaseStep::Apply;
	float StepTimer = 0.0f;
	int32 ShotsFired = 0;
	float MeasuredPingMs[NumCohorts] = { 0 };

	int32 ChecksTotal = 0;
	int32 ChecksFailed = 0;
	bool bRunning = false;

	static TWeakObjectPtr<UAFLLagCompTestRunner> ActiveRun;
};
