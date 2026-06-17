// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "AFLEnergyPickup.generated.h"

class UAFLOverlapCollectComponent;
class UStaticMeshComponent;

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
 * COLLECT SUBSTRATE (Loot Phase 3): the root CollectSphere is now a UAFLOverlapCollectComponent --
 * the PROVEN overlap + server-magnet + one-shot guard + viable-collector check that was EXTRACTED
 * VERBATIM from this very pickup, now shared with the INSTANT loot cache. This actor mirrors
 * AAFLLootCacheInstant: the component IS the root + collect volume + magnet (500uu pull); the actor
 * only binds OnCollected and runs the energy-SPECIFIC grant. The magnet (server-auth scan -> VInterpTo
 * the owner toward the nearest living pawn + dormancy wake/sleep) and the dead-pawn skip live in the
 * component now -- no duplicated overlap/magnet code on the actor.
 *
 * COLLECT GRANT: server-only OnCollected -> UGE_AFL_EnergyGain_Small with SetByCaller(Data.Energy.Gain)
 * = EnergyValue applied to the collector's ASC -> Event.Energy.Collected broadcast -> Destroy.
 */
UCLASS(Blueprintable)
class AFLCOMBAT_API AAFLEnergyPickup : public AActor
{
	GENERATED_BODY()

public:
	AAFLEnergyPickup(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	float GetEnergyValue() const { return EnergyValue; }

	/** Deferred-spawn init (the afl.Energy.SpawnBurst cheat spawns the raw C++ class with per-tier
	 *  values). Call between SpawnActorDeferred and FinishSpawning only. */
	void InitEnergyValue(float InValue) { EnergyValue = InValue; }

protected:
	/** The energy-SPECIFIC grant, bound to CollectSphere->OnCollected. The substrate fires this once,
	 *  server-auth, for a viable collector (overlap/magnet/guard/viable-check all live in the component);
	 *  this applies the gain GE + the collected message + Destroy. */
	UFUNCTION()
	void HandleCollected(AActor* Collector);

	/** Energy granted on collect. Tier BPs override (S=10 / M=25 / L=50). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Energy", meta = (ClampMin = "0.0"))
	float EnergyValue = 10.0f;

	/** Collect volume + root: the proven overlap+magnet substrate (UAFLOverlapCollectComponent, extracted
	 *  from this pickup -- Loot Phase 3). It IS the collect sphere AND runs the server magnet; the 500uu pull
	 *  + the radius/collision config are set in the ctor. Mirrors AAFLLootCacheInstant's root composition. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Energy")
	TObjectPtr<UAFLOverlapCollectComponent> CollectSphere;

	/** Default visible body (engine sphere, small) so C++-spawned pickups are watchable; tier BPs restyle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Energy")
	TObjectPtr<UStaticMeshComponent> VisualMesh;
};
