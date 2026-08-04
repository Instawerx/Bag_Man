// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Bots/AFLBotAimTypes.h"
#include "Player/LyraPlayerBotController.h"

#include "AFLBotController.generated.h"

/**
 * One set of movement accumulators. TWO instances exist per bot -- a lifetime one (what afl.Bot.MoveProbe
 * asserts on) and a per-round one (what AFL_MOVESNAP emits) -- and SampleMovement feeds both from the same
 * code path so they can never disagree about what a metric means.
 *
 * WHY PER-ROUND ACCUMULATORS RATHER THAN SNAPSHOT DELTAS. Subtracting the previous snapshot from the running
 * lifetime totals is cheaper and works for every scalar here, but it is WRONG for Cells: bots respawn each
 * round and re-walk ground they already covered, so "new distinct cells since the last snapshot" falls off
 * round over round for a bot whose behaviour never changed. That is the same dilution artefact the tier curve
 * is being built to avoid, hiding in a different metric -- so the round accumulator is real, and cleared.
 */
struct FAFLBotMoveAccum
{
	float CombatSeconds     = 0.0f;
	float StationarySeconds = 0.0f;
	float LateralSpeedSum   = 0.0f;
	float ForwardSpeedSum   = 0.0f;
	float RangeSumCm        = 0.0f;
	int32 RangeSamples      = 0;
	int32 LateralReversals  = 0;
	TSet<FIntVector> Cells;

	// -- goal / path watch. Answers "did the query give this bot anywhere to go, and did it try to go?" --
	int32 GoalPolls       = 0;   // polls taken while in combat
	int32 GoalValid       = 0;   // MoveGoal was SET  <=> the query returned >=1 item (see below)
	int32 GoalChanges     = 0;   // a genuinely new goal was issued
	int32 PollsNoProgress = 0;   // goal set + path active, yet the bot had not moved since the last poll
	int32 PollsPathIdle   = 0;   // goal set but path following idle -- the move was never issued
	float FrozenSeconds   = 0.0f;// sampling SKIPPED because move input was ignored (round-edge freeze)
	float SprintSeconds   = 0.0f;// AI-3: combat time holding State.Movement.Sprinting

	float SprintFraction() const { return (CombatSeconds > KINDA_SMALL_NUMBER) ? (SprintSeconds / CombatSeconds) : 0.0f; }

	void Reset() { *this = FAFLBotMoveAccum(); }

	float StationaryFraction() const { return (CombatSeconds  > KINDA_SMALL_NUMBER) ? (StationarySeconds / CombatSeconds) : 0.0f; }
	float LateralRatio()       const { return (ForwardSpeedSum > KINDA_SMALL_NUMBER) ? (LateralSpeedSum / ForwardSpeedSum) : 0.0f; }
	float ReversalsPerSecond() const { return (CombatSeconds  > KINDA_SMALL_NUMBER) ? (LateralReversals / CombatSeconds)  : 0.0f; }
	float MeanRangeCm()        const { return (RangeSamples > 0) ? (RangeSumCm / RangeSamples) : 0.0f; }
};

/**
 * One finished round, frozen. THE UNIT EVERY BEHAVIOURAL ASSERTION IS EVALUATED OVER.
 *
 * A lifetime mean cannot see an intermittent extreme -- it reports a bot that was wedged solid for one round
 * and fine for seven as mildly sluggish. That mistake has now been made twice on this probe (STATIONARY, then
 * SPRINT), so the fix is not another per-metric patch: assertions evaluate over EVERY recorded round and fail
 * on the WORST one, naming it. A single current-round check would not do either, because the probe runs at one
 * instant and would miss a bad round that already ended.
 *
 * PreferredRange/RangeBand are captured PER ROUND on purpose: they move with the tier, so comparing a
 * whole-match mean against the profile the bot happens to hold right now compares against the wrong yardstick.
 */
struct FAFLBotRoundSummary
{
	int32 Round            = INDEX_NONE;
	float Tier             = 0.0f;
	float CombatSeconds    = 0.0f;
	float Stationary       = 0.0f;
	int32 Cells            = 0;
	float Lateral          = 0.0f;
	float Reversals        = 0.0f;
	float Sprint           = 0.0f;
	float MeanRangeCm      = 0.0f;
	float PreferredRangeCm = 0.0f;   // the profile AT THAT ROUND -- tier moves it
	float RangeBandCm      = 0.0f;
};

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

	/** Deepest the aim ever went PAST the true target after a crossing, degrees.
	 *
	 *  DIAGNOSTIC ONLY -- do not assert on this. It was intended as "how far did it overshoot", but it
	 *  reads 140deg+ in practice, because when a target crosses the bot at close range the BEARING swings
	 *  that far on its own; the peak is target geometry, not tracker behaviour. It is also unfalsifiable as
	 *  a test: any crossing leaves a non-zero peak, so "> 0" passes by construction. GetLifetimeMaxCrossings
	 *  is the honest overshoot signal -- crossings reset per acquisition and cannot come from a switch. */
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

	/** READ-ONLY here. The BT's RunEQS service owns this key; the controller only observes it, to learn
	 *  whether the query gave this bot anywhere to go. Never written from C++. */
	static const FName BBKey_MoveGoal;

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
	int32 GetDistinctCellsVisited() const { return Lifetime.Cells.Num(); }

	/** Mean |lateral| / |forward| speed relative to aim. Near zero = not strafing, which is the exact
	 *  symptom of a query that generates toward the target. */
	float GetLateralRatio() const;

	/** Lateral direction sign-changes per second. TWO-SIDED with StationaryFraction: a single-sided
	 *  "more movement is better" test passes a bot vibrating in place at 10Hz. */
	float GetReversalsPerSecond() const;

	/** Mean distance held from the focus target, cm. Compared against this bot's own PreferredRangeCm. */
	float GetMeanRangeCm() const;

	/** Seconds of combat sampled. Zero = NO DATA, never PASS. */
	float GetCombatSampleSeconds() const { return Lifetime.CombatSeconds; }

	/** AI-3. Fraction of combat time spent sprinting. TWO-SIDED: ~0 means the ability is inert (the event
	 *  never reached it, or the threshold is unreachable); very high means the bot never walks, which is one
	 *  gear wearing two names and reads exactly as robotic as never sprinting.
	 *  LIFETIME -- diagnostic only. Assert on GetRoundHistory(); this one read 62% while three bots sat at
	 *  94-95% for the only real combat round. */
	float GetSprintFraction() const { return Lifetime.SprintFraction(); }

	/** Did this bot EVER hold the Sprint ability while it was alive?
	 *
	 *  A LATCH, not a live read, and that is the whole point. The probe runs at EndPlay, where a bot that died
	 *  and did not respawn has no pawn -- so no ASC, so a live scan finds no abilities and reports "not granted
	 *  on this hero" for a bot that spent its last round sprinting. Measured on the 2026-08-04 run: the 7 bots
	 *  reporting n/a had 24.5-34.9s of combat, the 8 reporting a real verdict had 54.8-62.5s, no overlap -- and
	 *  every one of the 7 logged 61-94% sprint in its final snapshot. The grant was never the problem; the
	 *  observation window was.
	 *
	 *  Latched at 1 Hz while alive, so it survives the pawn it was read from. */
	bool HasEverHadSprintAbility() const { return bSprintAbilityEverSeen; }

	/** Every finished round. What the behavioural assertions are evaluated over. */
	const TArray<FAFLBotRoundSummary>& GetRoundHistory() const { return RoundHistory; }

	/** The round in progress, as a summary. Combat seconds may be tiny -- the caller decides if it is enough. */
	FAFLBotRoundSummary GetCurrentRoundSummary() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn) override;

private:
	/** Roll the personality ONCE, from a seed stable for this bot's life. */
	void RollProfile();

	/** One-way latch for HasEverHadSprintAbility(). Cheap: returns immediately once set. */
	void LatchSprintAbilityPresence();
	bool bSprintAbilityEverSeen = false;

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
	 *  wandering is not combat movement and must not dilute the metrics. Feeds BOTH accumulators. */
	void SampleMovement(float DeltaTime, const APawn* MyPawn, const AActor* FocusActor);

	/** 1 Hz poll that detects a round change and emits the snapshot for the round just finished.
	 *
	 *  READ-ONLY BY DESIGN. It reads the round number for snapshot bookkeeping and NOTHING else -- in
	 *  particular it does NOT call RefreshTier. Refreshing the tier here would make difficulty advance on a
	 *  round boundary instead of on re-possession, which is a behaviour change wearing an instrumentation
	 *  hat. See the RefreshTier comment: that may well be the right fix one day, but not on this pass. */
	void TickRoundWatch();

	/** Emit one AFL_MOVESNAP line for the round just finished, then clear ThisRound. */
	void EmitMoveSnapshot(int32 ForRound, const TCHAR* Trigger);

	/** 2 Hz poll of MoveGoal + path following. THE WEDGE DISCRIMINATOR.
	 *
	 *  WHY MoveGoal IS THE EQS ITEM COUNT. UBTService_RunEQS::OnQueryFinished computes
	 *  `bSuccess = Result->IsSuccessful() && (Result->Items.Num() >= 1)`; on success it writes MoveGoal, and
	 *  with bUpdateBBOnFail (set on our node) it CLEARS MoveGoal otherwise. The engine has already reduced
	 *  the item count to the one bit that matters, and parked that bit in the blackboard. Reading it here
	 *  costs nothing and needs no replacement BT service class -- which would have meant re-authoring the
	 *  QueryConfig binding that was just proven, to learn something the blackboard already knows.
	 *
	 *    GoalValid ~ 0 of N polls  -> the query is returning NOTHING. EQS authoring.
	 *    GoalValid ~ N, no progress -> it had somewhere to go and could not get there. Pathing/collision.
	 *    GoalValid ~ N, path idle   -> the move was never issued. Behaviour tree. */
	void TickGoalWatch();

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
	FAFLBotMoveAccum Lifetime;    // never reset; what afl.Bot.MoveProbe asserts on
	FAFLBotMoveAccum ThisRound;   // cleared at every round transition; what AFL_MOVESNAP emits
	int32 LastLateralSign = 0;    // shared direction state -- a reversal counts once, into both

	// -- AI-2 round watch (instrumentation only) --
	/** Round the current ThisRound accumulator belongs to. INDEX_NONE until the first read. */
	int32 SnapRound = INDEX_NONE;

	/** Appended by EmitMoveSnapshot just before ThisRound is cleared. Bounded by rounds-per-match. */
	TArray<FAFLBotRoundSummary> RoundHistory;
	FTimerHandle RoundWatchTimer;
	TWeakObjectPtr<UObject> CachedTierSource;

	// -- AI-2 goal watch (instrumentation only) --
	FTimerHandle GoalWatchTimer;
	FVector LastPolledGoal = FVector::ZeroVector;
	FVector LastPolledPos  = FVector::ZeroVector;
	bool    bHasPolledOnce = false;
};
