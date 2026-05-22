// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "AFLPawnHitboxHistoryComponent.generated.h"

class USkeletalMeshComponent;
class UAFLLagCompensationWorldSubsystem;


/**
 * FAFLHitboxBoneSample
 *
 * One bone's world transform within a single 60Hz snapshot. Stored as the
 * component's world-space transform (not bone-local) so the lag-comp re-trace
 * can use it directly for a bounding-box test without re-resolving the
 * parent chain — the master doc Sec. 7.4 budget is 200µs per rewind and the
 * bone-local → world conversion would dominate that.
 *
 * Plain runtime struct (no USTRUCT()); never crosses a UFunction boundary.
 */
struct AFLCOMBAT_API FAFLHitboxBoneSample
{
	FName      Bone     = NAME_None;
	FTransform WorldXForm;
};


/**
 * FAFLHitboxFrameSnapshot
 *
 * One 60Hz frame's worth of bone samples plus the server time the sample
 * was taken at (UWorld::GetTimeSeconds at sample time). The lag-comp
 * interpolator brackets a requested ServerTime between two adjacent
 * snapshots to produce smooth rewound poses between ticks.
 */
struct AFLCOMBAT_API FAFLHitboxFrameSnapshot
{
	double                       ServerTime = 0.0;
	TArray<FAFLHitboxBoneSample> Samples;
};


/**
 * UAFLPawnHitboxHistoryComponent
 *
 * AFL-0211 per-pawn 60Hz hitbox ring buffer. The lag-comp world subsystem
 * (UAFLLagCompensationWorldSubsystem) reads from this component when a
 * server-side hit confirmation needs to rewind world geometry to a past
 * server time.
 *
 * Lifetime:
 *   - BeginPlay (authority worlds only) registers with the subsystem and
 *     enables a TG_PostPhysics tick. TG_PostPhysics matches master doc
 *     Sec. 7.4 — we sample AFTER character movement / cloth / IK have
 *     finalised bone transforms, so the snapshot reflects what a hitscan
 *     this frame would actually hit.
 *   - EndPlay unregisters and clears the ring.
 *   - Clients: tick disabled; the component is added uniformly to all pawn
 *     variants for content-authoring simplicity, but does nothing on
 *     clients. The lag-comp re-trace only runs on the server.
 *
 * Sampling:
 *   - TrackedBones lists the bones to capture. Default is the 8-bone Lyra
 *     hit silhouette (head, neck, spine, pelvis, hands, feet). Empty list
 *     falls back to the mesh root.
 *   - HistorySeconds caps the ring; default 1.2s per master doc.
 *   - Ring buffer size = HistorySeconds * SampleHz, capped to 256 slots.
 *
 * SampleAtTime returns bone transforms interpolated to an absolute server
 * time. The rewind path is read-only — the lag-comp subsystem queries
 * SampleAtTime to fill its rewind token; nothing mutates the live mesh.
 */
UCLASS(ClassGroup=(AFL), Meta=(BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLPawnHitboxHistoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UAFLPawnHitboxHistoryComponent();

	//~ Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

	/**
	 * Fills OutSamples with bone transforms interpolated to ServerTime. Returns
	 * false when no snapshot is available (e.g., the ring is empty because
	 * BeginPlay hasn't sampled yet, or the component is on a client).
	 *
	 * ServerTime is absolute (UWorld::GetTimeSeconds), NOT a delta.
	 */
	bool SampleAtTime(double ServerTime, TArray<FAFLHitboxBoneSample>& OutSamples) const;

	/** Diagnostic: number of valid samples currently in the ring. */
	int32 GetRingCount() const { return RingCount; }

protected:

	/**
	 * Bone names to track. Empty list defaults to the mesh's root bone.
	 * Default value set in the ctor to the 8-bone Lyra hit silhouette.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|LagComp")
	TArray<FName> TrackedBones;

	/**
	 * History window in seconds. Default 1.2 per master doc Sec. 7 L3.
	 * Ring buffer size = HistorySeconds * SampleHz, capped to 256 slots.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|LagComp", Meta=(ClampMin="0.05", ClampMax="3.0"))
	float HistorySeconds = 1.2f;

	/**
	 * Sample frequency in Hz. Default 60 per master doc; lower values trade
	 * interpolation accuracy for tick budget.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|LagComp", Meta=(ClampMin="20", ClampMax="120"))
	int32 SampleHz = 60;

private:

	/**
	 * Resolve the skeletal mesh component to sample. Cached lazily on first
	 * tick; cleared on EndPlay. Component-by-class lookup (not an ACharacter
	 * cast) so the component works on any APawn variant.
	 */
	USkeletalMeshComponent* ResolveMesh() const;

	/** Push one snapshot into the ring. ServerTime is absolute world time. */
	void PushSnapshot(double ServerTime);

	/**
	 * Locate the two snapshots bracketing ServerTime. Returns (NewerIdx,
	 * OlderIdx) into the ring and Alpha in [0,1] for Older → Newer lerp.
	 * Returns false when the ring is empty. Clamps to oldest/newest snapshot
	 * for out-of-window times.
	 */
	bool FindBracketingSnapshots(double ServerTime, int32& OutNewerIdx, int32& OutOlderIdx, float& OutAlpha) const;

	/** Ring storage. Sized in BeginPlay; circular write via WriteHead. */
	TArray<FAFLHitboxFrameSnapshot> Ring;

	/** Index of the next slot to write. Increments mod Ring.Num(). */
	int32 WriteHead = 0;

	/** Valid sample count (capped at Ring.Num()). */
	int32 RingCount = 0;

	/** Accumulator for fixed-rate sampling at SampleHz. */
	float TimeSinceLastSample = 0.0f;

	/** Cached at BeginPlay so per-tick checks are a single bool read. */
	bool bIsServer = false;

	/** Cached subsystem ref to avoid per-tick world->GetSubsystem lookups. */
	TWeakObjectPtr<UAFLLagCompensationWorldSubsystem> CachedSubsystem;

	/** Lazily-resolved mesh; cleared on EndPlay. */
	mutable TWeakObjectPtr<USkeletalMeshComponent> CachedMesh;
};
