// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

// FAFLRewindPawnEntry stores TArray<FAFLHitboxBoneSample> by value, so the
// element type must be COMPLETE here (TArray's destructor instantiation needs
// it). A forward declaration is insufficient — it only compiled while every
// include site happened to pull in the component header first. Surfaced by
// BM-0105a when Pulse.cpp became the first consumer to include this header
// without the component header preceding it.
#include "LagComp/AFLPawnHitboxHistoryComponent.h"

#include "AFLLagCompensationWorldSubsystem.generated.h"

class AActor;
class APlayerController;
class UAFLPawnHitboxHistoryComponent;


/**
 * FAFLRewindPawnEntry
 *
 * Per-pawn rewound pose captured by a single RewindWorldFor call. Holds the
 * pawn (as a weak ref so a destroyed pawn does not crash the bounding-box
 * check) and the interpolated bone samples at the requested server time.
 *
 * Not exposed to UFunction reflection; used by the opaque token below.
 */
struct AFLCOMBAT_API FAFLRewindPawnEntry
{
	TWeakObjectPtr<const AActor>     Pawn;
	TWeakObjectPtr<UAFLPawnHitboxHistoryComponent> Component;
	TArray<FAFLHitboxBoneSample>     RewoundBones;
};


/**
 * FAFLLagRewindToken
 *
 * Opaque value returned by RewindWorldFor and consumed by RestoreWorld.
 *
 * AFL-0211 uses a *non-mutating* rewind: the token carries the rewound
 * per-pawn bone poses, and the hit-confirmation pass queries the token
 * instead of the live skeletal mesh. That keeps the rewind trivially
 * re-entrant (no shared mutable world state to unwind) and avoids the
 * USkeletalMeshComponent set-bone-transform problem (the engine routes
 * skeletal pose through the anim graph, so writing a bone directly fights
 * the next animation tick and would corrupt the rest-pose). The
 * bounding-box check builds its colliders from the token's transforms.
 *
 * The "RestoreWorld" call invalidates the token (flips `bValid`) so the
 * scope-guard pattern stays meaningful: any further attempts to query the
 * token's rewound data after restore are a programming error and are
 * caught by IsValid()-style accessors.
 *
 * Re-entrancy: nested RewindWorldFor calls produce independent tokens; each
 * resolves bones from the per-pawn history component directly, so token A
 * is unaffected by token B being created or destroyed.
 *
 * The struct is USTRUCT()-marked only so it can travel through reflected
 * paths if a Blueprint-callable wrapper is ever added. Today the API is
 * pure-C++ — UFUNCTION is unnecessary and would force GENERATED_BODY()
 * boilerplate onto every consumer.
 */
USTRUCT()
struct AFLCOMBAT_API FAFLLagRewindToken
{
	GENERATED_BODY()

	FAFLLagRewindToken() = default;

	/** True until RestoreWorld consumes this token. False also = "default-constructed (empty) token." */
	bool   bValid = false;

	/** Server time (UWorld::GetTimeSeconds) the rewind sampled at. */
	double SampledServerTime = 0.0;

	/** Per-pawn rewound pose. Empty when no components were registered. */
	TArray<FAFLRewindPawnEntry> Entries;

	/**
	 * Look up the rewound world transform of a single bone on a specific
	 * actor. Returns false when the actor is not in the token (not registered,
	 * or excluded as the shooter's own pawn) or the bone was not tracked.
	 * Safe to call after RestoreWorld too — but only returns true if the
	 * caller saved a reference to the token before invalidating it.
	 */
	bool QueryBoneTransform(const AActor* Target, FName BoneName, FTransform& OutWorldXForm) const;

	/**
	 * Build an FBox in world space that encloses every rewound bone on the
	 * given actor. The bounding-box hit confirmation in master doc Sec. 7.4
	 * uses this to AABB-test the claimed hit point. Returns false when the
	 * actor has no rewound bones in the token.
	 */
	bool BuildBoundingBox(const AActor* Target, FBox& OutBox) const;
};


/**
 * UAFLLagCompensationWorldSubsystem
 *
 * AFL-0211 server-side lag compensation per master doc Sec. 7 Layer 3.
 *
 * Owns the registry of UAFLPawnHitboxHistoryComponent instances for the
 * world. Each component pushes a 60Hz ring buffer of its tracked hitbox
 * bone transforms (1.2s history); this subsystem reads those buffers when
 * a hit-confirmation pass needs to rewind world state to the shooter's
 * server-time-of-fire.
 *
 * Server-only. ShouldCreateSubsystem early-outs on non-gameplay worlds
 * (editor / preview / inactive); RewindWorldFor early-outs on client nets.
 * Callers can invoke unconditionally; the result is a valid empty token
 * the scope-guard pattern can still hand to RestoreWorld.
 *
 * Re-entrancy: RewindWorldFor returns a self-contained FAFLLagRewindToken;
 * the subsystem itself holds no per-call mutable state outside the
 * registered-component list. Multiple concurrent rewinds (e.g., AOE +
 * follow-up hitscan within the same server tick) are independent.
 */
UCLASS()
class AFLCOMBAT_API UAFLLagCompensationWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	UAFLLagCompensationWorldSubsystem();

	//~ Begin USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	/**
	 * Rewind every registered pawn's tracked hitbox bones to ServerTime.
	 *
	 * `RequestingPC` is the shooter's PlayerController. Its pawn is excluded
	 * from the rewind because the shooter's own pawn already had the trace
	 * executed in its own client-local frame — rewinding it would double-
	 * compensate. Pass nullptr when there is no shooter pawn to exclude
	 * (AI hitscan, server-driven proximity damage).
	 *
	 * `ServerTime` is the absolute world time (UWorld::GetTimeSeconds) to
	 * rewind to, NOT a delta. Callers compute it as
	 *   `World->GetTimeSeconds() - ClampedRTT`
	 * where ClampedRTT is min(serverMeasuredRTT/2 + interp, 200ms) per
	 * master doc Sec. 7.4.
	 *
	 * Returns a token whose `bValid` is true even when no components are
	 * registered yet, so callers can scope-guard the matching RestoreWorld
	 * call without branching.
	 *
	 * Server-only. On client worlds returns an empty token immediately.
	 */
	FAFLLagRewindToken RewindWorldFor(APlayerController* RequestingPC, float ServerTime);

	/**
	 * Invalidate the token. The AFL-0211 rewind is non-mutating, so this
	 * method does not touch the live world — its purpose is to make the
	 * scope-guard pattern meaningful and to catch double-restores. Future
	 * mutating-rewind implementations can drop into this entry point
	 * without changing call sites.
	 *
	 * Idempotent on already-consumed tokens; safe to call twice from a
	 * defensive ON_SCOPE_EXIT plus an explicit early restore.
	 */
	void RestoreWorld(FAFLLagRewindToken& Token);

	/**
	 * The full server-side hit-confirmation pass: rewind the world by
	 * RewindDeltaSeconds, build TargetActor's rewound bounding box, pad it,
	 * and test ImpactPoint against it. Returns the accept/reject verdict and
	 * emits the canonical "AFL_LAGCOMP: rewind dt=... entries=... verdict=..."
	 * log line. The token is created and restored internally (the rewind is
	 * non-mutating, so this is self-contained).
	 *
	 * This is the SINGLE confirm path. UAFLAG_Laser_Pulse::ServerApplyTargetData
	 * calls it for live hitscan; the afl.LagComp.TestFire debug command calls it
	 * with a fixed coordinate. Sharing the code (vs duplicating it) is the point:
	 * the BM-0105c isolated RTT-flip then exercises the REAL shipping path, not a
	 * reimplementation that could drift from it.
	 *
	 * `RewindDeltaSeconds` is the DELTA (e.g. ClampedRTT), NOT an absolute time —
	 * the absolute RewindTime = GetTimeSeconds() - RewindDeltaSeconds is computed
	 * inside. Empty-token default-accept is preserved: a degenerate rewind (client
	 * world, no registered components, no history yet) returns true so the live
	 * path isn't gated by missing snapshots.
	 *
	 * S4 AFL-0408-FU-GUNFIRE: `OutResolvedBone` is an ADDITIVE out-param (defaults NAME_None) that, ON ACCEPT,
	 * returns the rewound TRACKED bone NEAREST to ImpactPoint -- the authoritative hit bone for the dismember
	 * zone system. The client's Visibility hitscan hits the pawn CAPSULE (BoneName=None), so the live damage
	 * carried no zone bone and gun-fire never severed; the server now overwrites HitResult.BoneName with this
	 * resolved bone before building the damage spec (spoof-proof: server-side authority). The rewound bones live
	 * only inside this call (the token is restored before return), so the resolution MUST happen here. Every
	 * tracked bone is an AFLCore::BoneToZone key, so the result always maps to a zone (never None). On reject /
	 * off-server / empty-token, OutResolvedBone stays NAME_None and the accept/reject bool is UNCHANGED -- the
	 * resolution is purely additive.
	 */
	bool ConfirmHit(APlayerController* RequestingPC, float RewindDeltaSeconds,
		const AActor* TargetActor, const FVector& ImpactPoint, FName& OutResolvedBone);

	/** Called by UAFLPawnHitboxHistoryComponent on BeginPlay. Idempotent. */
	void RegisterComponent(UAFLPawnHitboxHistoryComponent* Component);

	/** Called by UAFLPawnHitboxHistoryComponent on EndPlay. Tolerates unregistered components. */
	void UnregisterComponent(UAFLPawnHitboxHistoryComponent* Component);

	/** Diagnostic: number of live snapshot-publishing components. */
	int32 GetNumRegisteredComponents() const { return RegisteredComponents.Num(); }

private:

	/**
	 * Live registry. Weak refs because component EndPlay is not guaranteed
	 * to fire before world teardown in some PIE shutdown paths; dangling
	 * weak ptrs are swept on the next RewindWorldFor pass.
	 */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UAFLPawnHitboxHistoryComponent>> RegisteredComponents;
};
