// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/PhysicsVolume.h"

#include "AFLWaterVolume.generated.h"

/**
 * AAFLWaterVolume
 *
 * The swimmable water body. Phase 1 of Docs/design/ShantyTown_Water_Swim_DESIGN.md -- the CLASS ONLY.
 * No component, no GameplayEffect, no degradation logic; those are phases 2 and 4. The knobs below are
 * DECLARED so phase 4's component reads them off the volume instead of carrying its own copy, but nothing
 * here consumes them yet.
 *
 * WHY A CLASS AND NOT A HAND-PLACED APhysicsVolume. UE engages MOVE_Swimming when a pawn overlaps a physics
 * volume with bWaterVolume set. A hand-placed volume works -- right up until someone forgets one of the two
 * settings that must never be wrong, at which point the failure is silent. Making them properties of the TYPE
 * removes them from the set of things an operator can get wrong:
 *
 *   bWaterVolume       set in the ctor. Without it the volume is inert and the player wades instead of swims.
 *   spatial loading    OFF in the ctor, and LOCKED -- see CanChangeIsSpatiallyLoadedFlag below.
 *
 * ⚠ THE ALWAYS-LOADED REQUIREMENT IS THE LOAD-BEARING ONE. ONE volume, never several.
 *
 *   A volume that STREAMS OUT mid-swim drops the player from MOVE_Swimming into falling -- inside water
 *   geometry, with no volume left to re-enter. Nothing tells them; they simply start drowning in a way the
 *   game never explains.
 *
 *   SEVERAL volumes create SEAMS. A player crossing from one to the next belongs to neither for a frame and
 *   exits swim.
 *
 * This is the same failure class as DOCTRINE C6 -- spawn-critical actors in a World Partition map must be
 * non-spatial or they are absent when needed -- and that law was earned on the very map this volume is for.
 * So residency is not left to placement: the constructor defaults it off, and CanChangeIsSpatiallyLoadedFlag
 * returns false so the editor cannot turn it back on. An operator can move and resize this volume; they
 * cannot make it stream.
 *
 * SIZE IT TO THE SWIMMABLE BAND, NOT THE VISUAL PLANE. The water mesh is 16 km square. The volume is the play
 * space (design doc section 5), and the band is bounded by degradation plus a blocking volume beyond any
 * survivable swim -- C5, containment is a correctness requirement rather than polish.
 *
 * WHY AFLGameCore AND NOT AFLMovement. The design doc names AFLMovement, which is correct for phase 4's
 * component and WRONG for this. AFLMovement is a GameFeature (ExplicitlyLoaded=true) and can unload; this
 * actor is PLACED IN A LEVEL, so its class must resolve whenever the level loads, regardless of which
 * Experience is active. AFLGameCore is a Runtime/Default plugin that is always loaded, and it already hosts
 * AAFLWeaponSpawner -- the existing precedent for a level-placed AFL actor.
 *
 * Shape follows DOCTRINE A10: a lean actor carrying its own tuning knobs, no Pawn anywhere near it.
 */
UCLASS()
class AFLGAMECORE_API AAFLWaterVolume : public APhysicsVolume
{
	GENERATED_BODY()

public:

	AAFLWaterVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor
	/**
	 * Returns false: this volume may NEVER be made spatially loaded.
	 *
	 * The engine routes every change to bIsSpatiallyLoaded through SetIsSpatiallyLoaded, which check()s this
	 * first, and the World Partition editor UI consults it before offering the toggle. Returning false is
	 * therefore the enforcement, not merely a hint -- the flag is not editable on this class.
	 *
	 * Structural on purpose. The alternative -- defaulting it off and trusting nobody flips it -- is exactly
	 * the "correct by placement" posture that produced the C6 spawn failure on this map.
	 */
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override;
	//~End

	// ---- PHASE 4 READ SURFACE. Declared here so the degradation component has ONE source for these values. ----

	/** Seconds a pawn may be swimming before degradation begins. */
	float GetEntryGraceSeconds() const { return EntryGraceSeconds; }

	/** Health lost per second once the grace period has elapsed. */
	float GetDegradationPerSecond() const { return DegradationPerSecond; }

	/** Whether the rate scales with distance from the play area. Phase 4 owns the scaling; this is the switch. */
	bool ShouldScaleRateWithDistance() const { return bScaleRateWithDistance; }

	/** Distance at which the rate is exactly DegradationPerSecond, cm. Only meaningful when scaling is on. */
	float GetDistanceScaleReferenceCm() const { return DistanceScaleReferenceCm; }

	/** Ceiling on the distance multiplier, so a far swim is bounded rather than unbounded. */
	float GetMaxDistanceRateMultiplier() const { return MaxDistanceRateMultiplier; }

protected:

	// ---- DEGRADATION KNOBS -- DECLARED, NOT WIRED (phase 4 consumes them). ----

	/**
	 * Grace period before degradation starts, seconds. Entering water must not punish instantly, or the
	 * shoreline stops being a decision and becomes a wall -- the design doc's containment section wants
	 * water survivable near shore.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Water|Degradation", meta = (ClampMin = "0.0"))
	float EntryGraceSeconds = 3.0f;

	/**
	 * Health lost per second while swimming, after the grace period. Applied by phase 4 through the damage
	 * ExecCalc, never as a direct health write (DOCTRINE A4). Left at 0 so a volume placed before phase 4
	 * exists is traversable and harmless rather than silently lethal.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Water|Degradation", meta = (ClampMin = "0.0"))
	float DegradationPerSecond = 0.0f;

	/**
	 * Whether the rate rises with distance from the play area. The design doc recommends distance-scaled,
	 * because a FLAT rate cannot express "survivable near shore, unsurvivable far out" -- it makes shallow
	 * water either free or lethal with nothing between. THE SHAPE IS EXPOSED HERE; THE SCALING IS PHASE 4's.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Water|Degradation")
	bool bScaleRateWithDistance = false;

	/** Distance from the play area at which the multiplier is 1.0, cm. Beyond it the rate climbs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Water|Degradation",
		meta = (EditCondition = "bScaleRateWithDistance", ClampMin = "0.0"))
	float DistanceScaleReferenceCm = 0.0f;

	/**
	 * Hard ceiling on the distance multiplier. Bounded on purpose: an unbounded ramp makes the far edge
	 * instantly lethal, which is the depth-lethality rule R32 retired, reintroduced through a curve.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Water|Degradation",
		meta = (EditCondition = "bScaleRateWithDistance", ClampMin = "1.0"))
	float MaxDistanceRateMultiplier = 1.0f;
};
