// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "AFLEnergyPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

/**
 * FAFLEnergyCollectedMessage -- broadcast on Event.Energy.Collected when a pickup applies.
 * Plain replicated-less message payload (HUD/score/FX listeners via UGameplayMessageSubsystem);
 * NOT net-serialized, so it lives here in the GameFeature per the residency rule.
 */
USTRUCT(BlueprintType)
struct AFLCOMBAT_API FAFLEnergyCollectedMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Energy")
	TObjectPtr<AActor> Collector = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Energy")
	float EnergyValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Energy")
	FVector Location = FVector::ZeroVector;
};

/**
 * AAFLEnergyPickup  (energy drops cycle 1 -- the P2 loop currency on the ground)
 *
 * Runtime-SPAWNED replicated pickup (death bursts, overload later). Spawned-actor posture per the
 * 2-client F5 rule: ALL config lives ON the actor as BP/class defaults (bReplicates +
 * bReplicateMovement set in the ctor so every spawn replicates from birth; EnergyValue per tier on
 * the BP child) -- no policy-reconstruction assumptions.
 *
 * MAGNETIC PULL: server-authoritative (the cycle-1 grounding C recommendation -- correctness
 * first, no mouth mispredict; bandwidth fits AFL-1402 at burst counts). Server tick scans
 * MagnetRadius for the nearest LIVING pawn with an ASC and VInterpTo-moves toward it; replicated
 * movement carries the result to clients. Client-side visual smoothing of the ~view-age stale
 * stream = NAMED FEEL DEBT (the cycle-3 finding). AFL-1403: DORM_DormantAll at rest +
 * FlushNetDormancy on magnetize-wake + tight NetCullDistanceSquared + NetUpdateFrequency 20.
 *
 * COLLECT: server-only overlap -> UGE_AFL_EnergyGain_Small with SetByCaller(Data.Energy.Gain) =
 * EnergyValue applied to the collector's ASC -> Event.Energy.Collected broadcast -> Destroy.
 * Dead pawns (Status.Death.*) are ignored by BOTH the magnet and the collect -- a death burst
 * must never re-collect into its own corpse.
 */
UCLASS(Blueprintable)
class AFLCOMBAT_API AAFLEnergyPickup : public AActor
{
	GENERATED_BODY()

public:
	AAFLEnergyPickup(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	float GetEnergyValue() const { return EnergyValue; }

	/** Deferred-spawn init (the afl.Energy.SpawnBurst cheat spawns the raw C++ class with per-tier
	 *  values). Call between SpawnActorDeferred and FinishSpawning only. */
	void InitEnergyValue(float InValue) { EnergyValue = InValue; }

protected:
	UFUNCTION()
	void OnCollectOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** True when the pawn is a valid magnet/collect target: has an ASC and is not Status.Death.*. */
	bool IsViableCollector(const AActor* Candidate) const;

	/** Energy granted on collect. Tier BPs override (S=10 / M=25 / L=50). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Energy", meta = (ClampMin = "0.0"))
	float EnergyValue = 10.0f;

	/** Magnet acquisition radius (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Energy", meta = (ClampMin = "0.0"))
	float MagnetRadius = 500.0f;

	/** VInterpTo speed for the pull (uu/s-ish interp constant). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Energy", meta = (ClampMin = "0.0"))
	float PullInterpSpeed = 4.0f;

	/** Collision sphere root (prim-as-root per the movable-asset rule; overlap = the collect). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Energy")
	TObjectPtr<USphereComponent> CollectSphere;

	/** Default visible body (engine sphere, small) so C++-spawned pickups are watchable; tier BPs restyle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Energy")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

private:
	/** One-shot collect guard (overlap can fire multiply before Destroy lands). */
	bool bCollected = false;

	/** Dormancy state mirror (avoid redundant FlushNetDormancy spam). */
	bool bMagnetAwake = false;
};
