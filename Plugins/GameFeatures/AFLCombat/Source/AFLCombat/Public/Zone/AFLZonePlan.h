// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AFLZonePlan.generated.h"

/**
 * THE ZONE PLAN — the whole shrinking-circle sequence for a match, as PURE DATA.
 *
 * IRONICS_BR_ZONE_SYSTEM.md §4 makes determinism the crux, because the primary users are STAKED battle
 * royales and tournaments: "same MatchId => same zone", and a disputed match must be replayable exactly.
 *
 * ══ WHY THE WHOLE PLAN IS BUILT UP FRONT ══════════════════════════════════════════════════════════════
 *
 * The scope doc says "seeded center per phase from the MatchId RNG". Drawing lazily, one phase at a time,
 * makes determinism a property of CALL ORDER: the sequence is only reproducible so long as nothing else
 * ever draws from that stream, and so long as every phase draws the same number of values in the same
 * order. Both are invisible invariants that a later edit breaks silently — and silently is exactly how a
 * staking dispute becomes unanswerable, because the log looks fine and the replay does not match.
 *
 * So the plan is computed ONCE, as a pure function of (seed, rules), before the first circle is shown:
 *
 *     - determinism is STRUCTURAL rather than disciplined — there is one draw sequence, in one function
 *     - it is unit-testable with NO world, NO actors, NO net: Z2 is provable headlessly
 *     - the entire future is known at match start, so telegraphing the next circle is a lookup rather
 *       than a prediction, and a tournament observer can be handed the full sequence up front
 *
 * Nothing here touches engine state. Keep it that way — the moment this reads a UWorld it stops being
 * provable.
 */

/** One circle in the sequence, and how the zone gets there from the previous one. */
USTRUCT(BlueprintType)
struct FAFLZonePhasePlan
{
	GENERATED_BODY()

	/** 0-based. Phase 0 is the OPENING circle: the whole playable area, no shrink into it. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Zone") int32 Index = 0;

	/** Centre in world XY. The containment test is a cylinder — height never gates the zone, because a
	 *  player on a roof inside the circle is inside it, and ShantyTown is three storeys tall in places. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Zone") FVector2D Centre = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Zone") float Radius = 0.f;

	/** Seconds the zone RESTS at the previous circle before shrinking into this one. This is the window a
	 *  player has to act on the telegraph, so it is the fairness dial, not a pacing one. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Zone") float HoldSeconds = 0.f;

	/** Seconds spent interpolating from the previous circle to this one. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Zone") float ShrinkSeconds = 0.f;

	/** Damage per second applied to anyone OUTSIDE while this circle is the active target. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Zone") float DamagePerSecond = 0.f;
};

/**
 * The tunables. Authored on `UAFLZoneConfig` so every number below is a data change, never a code change —
 * IRONICS_BR_ZONE_SYSTEM.md §7 lists the curve shape as an operator decision, and a decision nobody has
 * made yet must not be welded into C++.
 */
USTRUCT(BlueprintType)
struct FAFLZoneRules
{
	GENERATED_BODY()

	/** Centre of the whole playable area, world XY. Map-specific (ShantyTown's town core). */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone") FVector2D PlayableCentre = FVector2D::ZeroVector;

	/** Radius of the opening circle, cm. ShantyTown's measured core is ~357x302m, so ~17,500 covers it. */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "100.0")) float PlayableRadius = 17500.f;

	/** How many circles AFTER the opening one. 6 gives a readable arc without dragging at 36 players. */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "1", ClampMax = "16")) int32 ShrinkCount = 6;

	/**
	 * Radius of the LAST circle, cm. Deliberately NOT zero.
	 *
	 * §7 decision 3 asks fixed-footprint vs shrink-to-a-point. A point makes the ending a coin flip that no
	 * amount of skill survives, which is the one outcome a WAGERED match cannot ship — the final fight has
	 * to be winnable by playing it. ~30m is a rooftop-and-street-corner arena: small enough to force the
	 * fight, large enough to have one.
	 */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "100.0")) float FinalRadius = 3000.f;

	/** Hold before the FIRST shrink — longer than the rest, so a match opens with room to loot and rotate. */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "0.0")) float FirstHoldSeconds = 60.f;

	/** Hold before every later shrink. */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "0.0")) float HoldSeconds = 35.f;

	/** Time the first shrink takes. */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "1.0")) float FirstShrinkSeconds = 45.f;

	/** Time the last shrink takes. Later circles close faster — the pressure ramps with the damage. */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "1.0")) float FinalShrinkSeconds = 20.f;

	/** DPS outside the first circle. A nudge: survivable long enough to run in from anywhere. */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "0.0")) float FirstDamagePerSecond = 2.f;

	/** DPS outside the last circle. Lethal: being outside late is a decision, not a tactic. */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "0.0")) float FinalDamagePerSecond = 30.f;

	/**
	 * Fraction of the AVAILABLE offset the next centre may actually use, 0..1.
	 *
	 * §7 decision 2. 1.0 is uniform anywhere inside the parent that keeps the child fully contained; lower
	 * values pull the sequence toward the parent centre. Containment itself is NOT optional and is not
	 * controlled by this — see `FAFLZonePlan::Build`.
	 */
	UPROPERTY(EditAnywhere, Category = "AFL|Zone", meta = (ClampMin = "0.0", ClampMax = "1.0")) float CentreDriftFraction = 1.f;
};

/**
 * The built sequence. Phase 0 is the opening circle; 1..N are the shrinks.
 */
USTRUCT(BlueprintType)
struct AFLCOMBAT_API FAFLZonePlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Zone") TArray<FAFLZonePhasePlan> Phases;

	/** The seed this plan was built from — logged and replicated so a dispute can rebuild it exactly. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Zone") int32 Seed = 0;

	bool IsValid() const { return Phases.Num() >= 2; }

	/** Total match seconds the plan covers, from the opening circle to the end of the final shrink. */
	float TotalSeconds() const;

	/**
	 * Build the whole sequence. PURE — same (Seed, Rules) always yields the same plan, on every platform
	 * and in every build configuration.
	 *
	 * ⚠ EVERY CIRCLE IS FULLY CONTAINED IN ITS PARENT, and that is a fairness requirement rather than an
	 * aesthetic one: if a child circle could poke outside its parent, a player standing safely inside the
	 * current zone could be outside the next one through no action of their own, having had no way to
	 * avoid it. In a staked match that is indefensible. The centre is therefore drawn inside a disc of
	 * radius (parentRadius - childRadius), which makes containment true by construction.
	 *
	 * The offset is drawn UNIFORM-IN-DISC (sqrt of the radial roll), not uniform-in-radius — the latter
	 * concentrates circles near the parent centre and would make the sequence predictable, which for a
	 * wagering product is a competitive edge handed to whoever noticed.
	 */
	static FAFLZonePlan Build(int32 Seed, const FAFLZoneRules& Rules);

	/** Stable seed from the match's FGuid. Folding all four words keeps the whole id in play. */
	static int32 SeedFromGuid(const FGuid& MatchId);

	/** Human-readable dump — one line per phase. Logged at match start, so a replay has the sequence. */
	FString ToLogString() const;
};
