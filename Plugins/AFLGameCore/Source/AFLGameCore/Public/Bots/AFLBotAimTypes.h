// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "AFLBotAimTypes.generated.h"

/**
 * ONE TUNABLE AXIS of the bot aim model.
 *
 * Two independent things live here and they must not be confused:
 *   AtTier0 / AtTier1 -- the COMPETENCE curve. Where the axis sits at the start of a match vs the end.
 *   PerBotSpread      -- the PERSONALITY band. How far an individual bot may sit either side of that
 *                        centre. Without it every bot at a tier is the same bot wearing a different name.
 *
 * Resolution order is always: lerp the tier -> apply the bot's stable roll -> CLAMP to the absolute
 * limits on UAFLBotAimTiers. The clamp is last on purpose, so neither a mis-tuned curve nor an unlucky
 * roll can produce a bot outside the sanctioned envelope.
 */
USTRUCT(BlueprintType)
struct FAFLBotAimAxis
{
	GENERATED_BODY()

	/** Value at tier 0 -- round 1, the weakest a bot is ever asked to be. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim")
	float AtTier0 = 0.0f;

	/** Value at tier 1 -- the final round. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim")
	float AtTier1 = 0.0f;

	/** +/- band a single bot may be rolled within, around the tier centre. 0 = every bot identical. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim", meta = (ClampMin = "0.0"))
	float PerBotSpread = 0.0f;
};

/**
 * A bot's PERSONALITY: one normalised offset per axis, in [-1, +1].
 *
 * Rolled ONCE at possession from a seed derived from the bot's own identity, and never re-rolled.
 * That is deliberate -- competence rises with the match, but WHO the bot is must not change under the
 * player. Re-rolling each round turns character into noise and the roster reads as one AI resampling.
 */
USTRUCT()
struct FAFLBotAimRoll
{
	GENERATED_BODY()

	float Reaction      = 0.0f;
	float TrackRate     = 0.0f;
	float Stiffness     = 0.0f;
	float Damping       = 0.0f;
	float SteadyError   = 0.0f;
	float ErrorFreq     = 0.0f;

	// -- AI-2 movement. Same roll, same seed, same lifetime: a bot's aim and its footwork are one person. --
	float PreferredRange = 0.0f;
	float RangeBand      = 0.0f;
	float RepositionRate = 0.0f;
	float LateralBias    = 0.0f;

	bool bRolled = false;
};

/** The RESOLVED movement values for the current tier. Written to the blackboard at possession, read by
 *  EQS_AFL_CombatReposition as query parameters. */
USTRUCT()
struct FAFLBotMoveProfile
{
	GENERATED_BODY()

	/** Centre of the donut this bot repositions within, cm from its target. AGGRESSION AS DISTANCE:
	 *  low = pusher, high = kiter. This replaces the stock query's only live term -- a prefer-greater
	 *  distance score that, with nothing opposing it on a coverless map, is an unbounded retreat. A held
	 *  range is a number; a retreat bias is a direction. */
	float PreferredRangeCm = 700.0f;

	/** Half-width of that band. Wide = roams, tight = holds a line. */
	float RangeBandCm = 300.0f;

	/** Seconds between repositions. THE CONTINUITY LEVER: the bot must be re-tasked before it arrives,
	 *  or the MoveTo completes, the sequence ends, and it stands still waiting to be re-entered -- which
	 *  is the entire bug AI-2 exists to fix. At 600 uu/s a bot covers ~900cm in 1.5s, so a donut band
	 *  beyond that guarantees re-task-before-arrival and continuity becomes emergent, not authored. */
	float RepositionIntervalSec = 1.5f;

	/** Weight on the angular (Dot) test -- how much this bot prefers moving AROUND its target versus
	 *  toward it. THE STRAFE TERM. bAllowStrafe is already true on the combat MoveTo and produces nothing
	 *  visible, because the stock query generates toward the target so velocity and facing are parallel
	 *  and there is no lateral component to render. Strafe is authored here, not in movement code. */
	float LateralBias = 1.0f;
};

/** The RESOLVED per-bot values for the current tier. Recomputed when the round advances. */
USTRUCT()
struct FAFLBotAimProfile
{
	GENERATED_BODY()

	/** Perception. Seconds between a target becoming the focus and tracking beginning. */
	float ReactionSeconds = 0.25f;

	/** Motor. Hard cap on how fast control rotation may slew, deg/sec. */
	float MaxTrackRateDegPerSec = 120.0f;

	/** Spring constant pulling aim toward the (wobbled) target. Higher = more urgent. */
	float TrackStiffness = 40.0f;

	/** <1 overshoots and corrects (human). >=1 approaches asymptotically and reads robotic. */
	float TrackDampingRatio = 0.7f;

	/** Amplitude of the residual wobble, degrees. NEVER zero -- a perfect hold on a still target is the tell. */
	float SteadyErrorDeg = 1.5f;

	/** How fast that wobble wanders, Hz. */
	float ErrorFrequencyHz = 0.6f;
};

/**
 * UAFLBotAimTiers -- the designer-facing tier table for the bot aim model.
 *
 * TIER IS A PURE FUNCTION OF ROUND NUMBER (v1). Score delta is wired but damped to zero: difficulty
 * may RISE with match progress and may only ever be HELD BACK, never raised, by the score line.
 * Raising difficulty when bots are behind is rubber-banding regardless of how symmetric the input
 * looks, and in a mode carrying staking and MMR that is an integrity problem rather than a feel one.
 */
UCLASS(BlueprintType)
class AFLGAMECORE_API UAFLBotAimTiers : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// -- the six axes --

	/** Seconds before tracking begins. FALLS with tier (a better bot reacts sooner). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Axes")
	FAFLBotAimAxis Reaction = { 0.42f, 0.24f, 0.06f };

	/** Deg/sec slew cap. RISES with tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Axes")
	FAFLBotAimAxis TrackRate = { 90.0f, 260.0f, 25.0f };

	/** Spring constant. RISES with tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Axes")
	FAFLBotAimAxis Stiffness = { 26.0f, 55.0f, 6.0f };

	/** Damping ratio. Held in the overshoot band across all tiers -- a bot that stops overshooting stops
	 *  looking human, so this does NOT trend toward 1.0 as competence rises. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Axes")
	FAFLBotAimAxis Damping = { 0.65f, 0.78f, 0.06f };

	/** Residual wobble, degrees. FALLS with tier but never to zero (see MinSteadyErrorDeg). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Axes")
	FAFLBotAimAxis SteadyError = { 3.2f, 0.9f, 0.5f };

	/** Wobble frequency, Hz. Roughly flat -- this is anatomy, not skill. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Axes")
	FAFLBotAimAxis ErrorFrequency = { 0.55f, 0.75f, 0.20f };

	// -- THE ABSOLUTE CLAMPS. "Never overwhelming" is enforced here, not tuned above. --

	/** Reaction can never go below this at ANY tier or roll. ~190ms is around human-pro visual reaction;
	 *  below it a bot is superhuman by definition, not merely good. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Clamps", meta = (ClampMin = "0.0"))
	float MinReactionSeconds = 0.19f;

	/** Slew can never exceed this at ANY tier or roll. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Clamps", meta = (ClampMin = "1.0"))
	float MaxTrackRateCeilingDegPerSec = 300.0f;

	/** Residual wobble can never fall below this. MUST stay > 0: a bot holding a perfect bead on a
	 *  stationary target is the single clearest tell that it is not a person. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Clamps", meta = (ClampMin = "0.01"))
	float MinSteadyErrorDeg = 0.35f;

	/** Damping is clamped into the overshoot band so no roll can produce an asymptotic tracker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Clamps", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MaxDampingRatio = 0.85f;

	/** Per-acquisition jitter on the reaction delay, seconds (+/-). Applied at ACQUISITION, not at roll:
	 *  a human's reaction varies shot to shot, and without this every bot in a squad reacts on the same
	 *  frame and the volley reads as scripted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Clamps", meta = (ClampMin = "0.0"))
	float ReactionJitterSeconds = 0.05f;

	// -- AI-2 MOVEMENT AXES. Same resolution contract: lerp the tier, apply the bot's roll, clamp last. --

	/** Engagement range, cm. Falls slightly with tier -- a better bot presses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotMove|Axes")
	FAFLBotAimAxis PreferredRange = { 850.0f, 650.0f, 250.0f };

	/** Band half-width, cm. The PerBotSpread here is what makes one bot a roamer and another a holder. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotMove|Axes")
	FAFLBotAimAxis RangeBand = { 300.0f, 250.0f, 80.0f };

	/** Seconds between repositions. Falls with tier -- a better bot resets its angle more often. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotMove|Axes")
	FAFLBotAimAxis RepositionInterval = { 1.8f, 1.2f, 0.4f };

	/** Angular-test weight. Rises with tier -- a better bot strafes more and walks straight at you less. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotMove|Axes")
	FAFLBotAimAxis LateralBias = { 0.8f, 1.6f, 0.35f };

	// -- MOVEMENT CLAMPS --

	/** Never reposition faster than this. The fast-cadence extreme: the goal moves before the bot can
	 *  commit to a path, direction reverses every tick, and it thrashes between two points -- which reads
	 *  WORSE than standing still. afl.Bot.MoveProbe asserts REVERSALS two-sided for exactly this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotMove|Clamps", meta = (ClampMin = "0.25"))
	float MinRepositionIntervalSec = 0.8f;

	/** Never reposition slower than this, or the stationary gaps the whole phase exists to remove return. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotMove|Clamps", meta = (ClampMin = "0.5"))
	float MaxRepositionIntervalSec = 3.0f;

	/** Donut inner radius can never collapse below this -- a bot standing on its target is not combat. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotMove|Clamps", meta = (ClampMin = "50.0"))
	float MinEngagementRangeCm = 250.0f;

	/** ...nor exceed this, or bots disengage to the far side of the map and the round stalls. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotMove|Clamps", meta = (ClampMin = "100.0"))
	float MaxEngagementRangeCm = 2500.0f;

	/** v1 = 0. The score-delta brake is wired and inert until the base model is proven -- confounding two
	 *  axes during first tuning tunes neither. When enabled it may only ever REDUCE tier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Tier", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlowoutDamping = 0.0f;

	/** Score delta at which the brake starts, once BlowoutDamping is non-zero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Tier", meta = (ClampMin = "0.0"))
	float BlowoutThreshold = 2.0f;

	/** Score delta beyond the threshold over which the brake reaches full strength. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|BotAim|Tier", meta = (ClampMin = "1.0"))
	float BlowoutRange = 3.0f;

	/** Resolve one axis: lerp the tier, apply the bot's stable roll, then clamp. */
	static float ResolveAxis(const FAFLBotAimAxis& Axis, float Tier, float NormalisedRoll)
	{
		return FMath::Lerp(Axis.AtTier0, Axis.AtTier1, FMath::Clamp(Tier, 0.0f, 1.0f))
			 + Axis.PerBotSpread * FMath::Clamp(NormalisedRoll, -1.0f, 1.0f);
	}

	/** Full resolution for one bot at one tier. THE CLAMPS ARE APPLIED LAST, after both the curve and the
	 *  roll, so neither can produce a bot outside the envelope. */
	FAFLBotAimProfile Resolve(float Tier, const FAFLBotAimRoll& Roll) const
	{
		FAFLBotAimProfile P;
		P.ReactionSeconds        = FMath::Max(MinReactionSeconds, ResolveAxis(Reaction,       Tier, Roll.Reaction));
		P.MaxTrackRateDegPerSec  = FMath::Clamp(ResolveAxis(TrackRate,      Tier, Roll.TrackRate),   1.0f, MaxTrackRateCeilingDegPerSec);
		P.TrackStiffness         = FMath::Max(1.0f,              ResolveAxis(Stiffness,       Tier, Roll.Stiffness));
		P.TrackDampingRatio      = FMath::Clamp(ResolveAxis(Damping,        Tier, Roll.Damping),     0.05f, MaxDampingRatio);
		P.SteadyErrorDeg         = FMath::Max(MinSteadyErrorDeg, ResolveAxis(SteadyError,     Tier, Roll.SteadyError));
		P.ErrorFrequencyHz       = FMath::Max(0.05f,             ResolveAxis(ErrorFrequency,  Tier, Roll.ErrorFreq));
		return P;
	}

	/** Movement resolution. Identical contract, clamps last. */
	FAFLBotMoveProfile ResolveMove(float Tier, const FAFLBotAimRoll& Roll) const
	{
		FAFLBotMoveProfile M;
		M.PreferredRangeCm = FMath::Clamp(ResolveAxis(PreferredRange, Tier, Roll.PreferredRange),
			MinEngagementRangeCm, MaxEngagementRangeCm);
		M.RangeBandCm      = FMath::Max(50.0f, ResolveAxis(RangeBand, Tier, Roll.RangeBand));
		M.RepositionIntervalSec = FMath::Clamp(ResolveAxis(RepositionInterval, Tier, Roll.RepositionRate),
			MinRepositionIntervalSec, MaxRepositionIntervalSec);
		M.LateralBias      = FMath::Max(0.0f, ResolveAxis(LateralBias, Tier, Roll.LateralBias));

		// The donut's inner edge must stay positive after the band is subtracted, or the generator
		// degenerates to a disc centred on the target and the bot walks into its face.
		M.RangeBandCm = FMath::Min(M.RangeBandCm, M.PreferredRangeCm - MinEngagementRangeCm * 0.5f);
		M.RangeBandCm = FMath::Max(50.0f, M.RangeBandCm);
		return M;
	}
};
