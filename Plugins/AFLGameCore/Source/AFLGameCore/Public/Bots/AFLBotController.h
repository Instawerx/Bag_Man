// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Bots/AFLBotAimTypes.h"
#include "Player/LyraPlayerBotController.h"

#include "AFLBotController.generated.h"

/**
 * AAFLBotController  (AI-1 -- the bot aim model)
 *
 * THE PROBLEM. Stock bot aim is a perfect, instantaneous ray. BTS_SetFocus calls SetFocus(target) and
 * AAIController::UpdateControlRotation then recomputes control rotation to point exactly at the focal
 * point EVERY FRAME -- no smoothing, no error, no delay. The trace reads that rotation straight off the
 * pawn (AFLAG_Hitscan_Base::ClientPredictAndSend takes GetViewRotation() for anything that is not a
 * PlayerController), so a bot is a hitscan turret that never misses and never looks like it is trying.
 *
 * THE SEAM. Humans are untouched BY CONSTRUCTION, not by a flag: this is an AAIController override, and
 * a human has no AAIController. There is no code path here a player can reach.
 *
 * THE STRUCTURAL IDEA -- WOBBLE GOES INTO THE TARGET, NOT THE OUTPUT. The bot chases a slightly-wrong,
 * slowly-drifting point while a spring-damper closes on it. Two things fall out for free:
 *   - motion is CONTINUOUS. Nothing here snaps, so nothing trips the AimAngularVelocity anti-cheat
 *     telemetry on Pulse and fills it with bot noise.
 *   - micro-correction is EMERGENT rather than authored. The bot is always slightly behind a moving
 *     point, which is what tracking actually looks like when a person does it.
 *
 * OVERSHOOT IS REQUIRED, not incidental. Damping ratio is held below 1 at every tier, so aim passes the
 * target and corrects. An asymptotic tracker -- error only ever shrinking -- is the most robotic-looking
 * option available even at a slow rate, and "never overshoots" is a test FAILURE (see afl.Bot.AimProbe).
 *
 * WHAT MUST NOT BE LOST. This override REPLACES AAIController::UpdateControlRotation rather than calling
 * Super (Super would immediately overwrite the model's output with a perfect snap). Three stock
 * behaviours are therefore reproduced by hand and each is a silent regression if dropped:
 *   1. the FAISystem::IsValidLocation(FocalPoint) check, with the bSetControlRotationFromPawnOrientation
 *      fallback and the leave-rotation-alone case beneath it
 *   2. pitch zeroed unless the focus is a Pawn
 *   3. the bUpdatePawn -> FaceRotation call that turns the BODY (distinct from aim, and the thing that
 *      makes the error visible to an observer)
 */
UCLASS(Blueprintable)
class AFLGAMECORE_API AAFLBotController : public ALyraPlayerBotController
{
	GENERATED_BODY()

public:
	AAFLBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The tier table. Null = the model is INERT and stock snap-aim is used, so a missing asset degrades
	 *  to today's behaviour rather than to a bot that cannot aim. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim")
	TObjectPtr<UAFLBotAimTiers> AimTiers;

	/** Resolved values in use right now (recomputed when the round advances). Probe + log read this. */
	const FAFLBotAimProfile& GetAimProfile() const { return Profile; }

	/** The bot's stable personality roll. */
	const FAFLBotAimRoll& GetAimRoll() const { return Roll; }

	/** 0..1 tier currently resolved from match state. */
	float GetAimTier() const { return CachedTier; }

	/** PER-ACQUISITION metrics. DIAGNOSTIC ONLY -- these reset on every acquisition, and bots re-acquire
	 *  every 1-2s, so a probe sampling one instant reads whatever fragment happens to be in flight. That
	 *  is what produced four false OVERSHOOT failures against a model that logged 147 real crossings in
	 *  the same session. Assert on the lifetime values below instead. */
	float GetLastReactionDelay() const { return LastReactionDelay; }
	float GetPeakTrackRate() const { return PeakTrackRateThisAcquire; }
	int32 GetErrorSignCrossings() const { return SignCrossings; }

	// -- LIFETIME metrics. These are what the probe asserts on: monotonic, never reset. --

	/** Most true-error sign crossings seen in any single acquisition. */
	int32 GetLifetimeMaxCrossings() const { return LifetimeMaxCrossings; }

	/** Fastest slew ever achieved, deg/sec. Compared against the resolved cap. */
	float GetLifetimePeakRate() const { return LifetimePeakRate; }

	/** Deepest the aim ever went PAST the true target after a crossing, degrees. This is the number that
	 *  says how far it overshot -- unlike the old per-crossing figure, which sampled the error AT the
	 *  zero-crossing and was therefore ~0 by construction. */
	float GetLifetimeMaxOvershootDeg() const { return LifetimeMaxOvershootDeg; }

	/** Shortest reaction delay ever rolled. The floor assertion cares about the minimum, not the last. */
	float GetLifetimeMinReactionDelay() const { return LifetimeMinReactionDelay; }

	/** How many targets this bot has acquired. Zero = no runtime data exists yet. */
	int32 GetAcquisitionCount() const { return AcquisitionCount; }

	/** True once the bot has actually tracked (past a reaction window, non-zero slew). VACUOUS-SAMPLE
	 *  GUARD: without this a bot sitting inside its reaction window reports 0 d/s and trivially satisfies
	 *  a 69 d/s ceiling -- a test a dead bot passes. */
	bool HasTrackedEver() const { return bHasTrackedEver; }

	/** False until OnPossess has rolled and resolved. Config assertions are meaningless before this. */
	bool IsProfileResolved() const { return CachedRound >= 0 && Roll.bRolled; }

	// ================= AI-2 MOVEMENT =================

	/** Blackboard key names the controller writes and EQS_AFL_CombatReposition reads as query params.
	 *  These are the contract between C++ and the AFL blackboard asset -- rename one and the query
	 *  silently falls back to its defaults, which is a bot with no personality and no warning.
	 *
	 *  THE KEYS ARE RADII, NOT PREFERRED-RANGE/BAND. FEQSParametrizedQueryExecutionRequest::Execute binds a
	 *  query param straight to a blackboard key -- FEnvQueryRequest::SetDynamicParam reads the float and
	 *  passes it through, with nowhere to evaluate an expression. So the (preferred -/+ band) arithmetic is
	 *  done here and the blackboard carries the FINISHED donut radii. */
	static const FName BBKey_DonutInner;
	static const FName BBKey_DonutOuter;
	static const FName BBKey_LateralBias;

	/** WRITTEN BUT NOT CONSUMED. UBTService::Interval is a plain float with no FAIDataProvider behind it, so
	 *  there is no binding surface for a per-bot reposition cadence -- every bot re-queries on the shared
	 *  1.5s +/- 0.5s node interval. The key is still published so the value is visible to the probe and to
	 *  a future C++ BT service that can honour it; treat a non-zero value here as intent, not behaviour. */
	static const FName BBKey_RepositionInterval;

	const FAFLBotMoveProfile& GetMoveProfile() const { return MoveProfile; }

	/** True once the four movement params reached the blackboard. If this is false the bot is running on
	 *  the query's authored defaults, NOT its own personality -- the probe must not call that a pass. */
	bool AreMoveParamsPushed() const { return bMoveParamsPushed; }

	/** The donut radii this bot actually pushed, cm. Exposed so afl.Bot.MoveProbe can compare them against
	 *  what the live blackboard reads back WITHOUT re-deriving the arithmetic -- a probe that recomputes the
	 *  expected value from the same inputs cannot catch a key-name drift, it just agrees with itself. */
	float GetDonutInnerCm() const { return PushedDonutInnerCm; }
	float GetDonutOuterCm() const { return PushedDonutOuterCm; }

	// -- movement metrics, lifetime, asserted by afl.Bot.MoveProbe --

	/** Fraction of tracked combat time spent under the stationary speed threshold. TWO-SIDED: too high is
	 *  the standing-still bug; ~zero is a bot that never pauses, which is equally inhuman. */
	float GetStationaryFraction() const;

	/** Distinct 200cm world cells occupied while in combat. 1-2 means the reposition cycle is terminating
	 *  and the bot is shuffling in place. */
	int32 GetDistinctCellsVisited() const { return VisitedCells.Num(); }

	/** Mean |lateral| / |forward| speed relative to aim. Near zero = not strafing, which is the exact
	 *  symptom of a query that generates toward the target. */
	float GetLateralRatio() const;

	/** Lateral direction sign-changes per second. TWO-SIDED with StationaryFraction: a single-sided
	 *  "more movement is better" test passes a bot vibrating in place at 10Hz. */
	float GetReversalsPerSecond() const;

	/** Mean distance held from the focus target, cm. Compared against this bot's own PreferredRangeCm. */
	float GetMeanRangeCm() const;

	/** Seconds of combat sampled. Zero = NO DATA, never PASS. */
	float GetCombatSampleSeconds() const { return CombatSampleSeconds; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn) override;

private:
	/** Roll the personality ONCE, from a seed stable for this bot's life. */
	void RollProfile();

	/** Re-resolve Profile and MoveProfile from the current round.
	 *
	 *  WHEN THIS ACTUALLY RUNS: OnPossess, plus a first-frame guard in UpdateControlRotation. NOTHING WATCHES
	 *  THE ROUND NUMBER. The tier advances because bots re-possess on respawn and rounds end with everyone
	 *  dead, not because a change is detected -- so a bot that somehow held one pawn across a round boundary
	 *  would keep round-1 difficulty. Tying this to an actual round-changed event is the correct fix if that
	 *  ever happens; polling the tier source per frame per bot is not (RefreshTier walks components to find
	 *  IAFLMatchTierSource). PushMoveParamsToBlackboard rides the same schedule, so aim and movement can
	 *  never disagree about which tier they are on. */
	void RefreshTier();

	/** Continuous two-frequency drift, degrees. Incommensurate rates so it never visibly repeats. */
	float Wobble(double TimeSeconds, float PhaseA, float PhaseB) const;

	/** Write the four movement params to the blackboard. The blackboard does not exist at OnPossess (the
	 *  BT starts after), so this retries on a bounded timer -- the same not-ready-yet poll shape used by
	 *  UAFLW_RoundHeader::TryArm and the population reconcile. Silent failure here means every bot runs
	 *  the query's authored defaults, so it logs and it is probe-visible. */
	void PushMoveParamsToBlackboard();

	/** Per-frame movement sampling for the probe. Only accumulates while a focus target exists -- idle
	 *  wandering is not combat movement and must not dilute the metrics. */
	void SampleMovement(float DeltaTime, const APawn* MyPawn, const AActor* FocusActor);

	FAFLBotAimRoll    Roll;
	FAFLBotAimProfile Profile;

	// -- runtime aim state --
	float  AimVelYawDegPerSec   = 0.0f;
	float  AimVelPitchDegPerSec = 0.0f;
	double ReactionEndsAtSeconds = 0.0;
	TWeakObjectPtr<AActor> LastFocusActor;

	/** Per-bot wobble phases, so two bots never drift in sympathy. */
	float WobblePhaseA = 0.0f;
	float WobblePhaseB = 0.0f;

	int32 CachedRound = -1;
	float CachedTier  = 0.0f;

	// -- probe metrics, per acquisition (diagnostic) --
	double AcquireTimeSeconds      = 0.0;
	float  LastReactionDelay       = 0.0f;
	float  PeakTrackRateThisAcquire = 0.0f;
	int32  SignCrossings           = 0;
	/** Sign of the error to the TRUE (un-wobbled) focal point. Crossings are counted on THIS, not on the
	 *  wobbled error: the wobble reverses direction roughly twice a second, so a wobbled-error crossing is
	 *  indistinguishable from the target drifting through a perfectly still aim. Measuring the true error
	 *  is what makes the overshoot assertion falsifiable. */
	int32  LastTrueYawErrorSign    = 0;
	bool   bTrackingStarted        = false;

	/** Running peak of |true error| since the last crossing -- how far past the target this excursion went. */
	float  OvershootPeakThisExcursion = 0.0f;
	bool   bInOvershootWindow         = false;

	// -- probe metrics, LIFETIME (asserted) --
	int32  LifetimeMaxCrossings    = 0;
	float  LifetimePeakRate        = 0.0f;
	float  LifetimeMaxOvershootDeg = 0.0f;
	float  LifetimeMinReactionDelay = TNumericLimits<float>::Max();
	int32  AcquisitionCount        = 0;
	bool   bHasTrackedEver         = false;

	// -- AI-2 movement state --
	FAFLBotMoveProfile MoveProfile;
	bool      bMoveParamsPushed = false;
	int32     MovePushAttempts  = 0;
	float     PushedDonutInnerCm = 0.0f;
	float     PushedDonutOuterCm = 0.0f;
	FTimerHandle MovePushRetryTimer;

	// -- AI-2 movement metrics (combat time only) --
	float CombatSampleSeconds   = 0.0f;
	float StationarySeconds     = 0.0f;
	float LateralSpeedSum       = 0.0f;
	float ForwardSpeedSum       = 0.0f;
	float RangeSumCm            = 0.0f;
	int32 RangeSamples          = 0;
	int32 LateralReversals      = 0;
	int32 LastLateralSign       = 0;
	TSet<FIntVector> VisitedCells;
};
