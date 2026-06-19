// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/SphereComponent.h"

#include "AFLOverlapCollectComponent.generated.h"

class AActor;

/** Fired (server-auth) the instant a viable collector overlaps -- the consumer binds this to its grant
 *  (energy pickup -> the gain GE + Destroy; INSTANT loot cache -> UAFLLootGrantComponent::TryGrant). The
 *  component itself is generic: it does overlap-detect + (optional) magnet + the one-shot guard; what the
 *  collect GRANTS lives in the consumer's handler. One-shot: fires exactly once (bCollected). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAFLOnOverlapCollected, AActor*, Collector);

/**
 * UAFLOverlapCollectComponent -- the generalized INSTANT retrieval substrate (Loot Phase 3). The
 * overlap + magnet + dormancy + one-shot guard + viable-collector check EXTRACTED VERBATIM from the
 * proven AAFLEnergyPickup (committed P2). A USphereComponent: it IS the collect sphere AND runs the
 * authority magnet (VInterpTo-pulls its OWNER actor toward the nearest viable pawn) AND fires
 * OnCollected on overlap. Consumers (the refactored energy pickup; the INSTANT loot cache) wear it as
 * their root/collect volume and bind OnCollected to grant + consume.
 *
 * Server-authoritative (the magnet + the collect run on authority only; clients ride replicated movement
 * of the owner). The magnet is OPTIONAL: MagnetRadius <= 0 disables it (pure walk-over overlap, e.g. a
 * cache that should not fly at you); the energy pickup keeps its 500uu pull.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLOverlapCollectComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UAFLOverlapCollectComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Bound by the consumer to grant + consume on collect. Server-auth, fires once. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Loot")
	FAFLOnOverlapCollected OnCollected;

	/** True once collected (one-shot guard -- overlap can fire multiply before the owner is consumed). */
	UFUNCTION(BlueprintPure, Category = "AFL|Loot")
	bool IsCollected() const { return bCollected; }

	/** Magnet acquisition radius (uu). <= 0 disables the magnet (pure overlap). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot", meta = (ClampMin = "0.0"))
	float MagnetRadius = 0.0f;

	/** VInterpTo speed for the pull. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot", meta = (ClampMin = "0.0"))
	float PullInterpSpeed = 4.0f;

	/** E2 cause-B (PRESENTATION delay): seconds AFTER spawn before the collect arms, so a dismember part appears +
	 *  tumbles + lands before it can be collected (vs instant point-blank absorb). 0 = arm immediately (the placed
	 *  INSTANT cache / energy pickup -- unchanged). The head/limb set ~1.5s. This is a gameplay beat, not the race
	 *  fix (the grant's bConfigured gate is the race fix); it layers ON TOP. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot", meta = (ClampMin = "0.0"))
	float ActivationDelay = 0.0f;

	/** PRESENTATION (dismember pass): when true the collect arms on the attach-parent physics body's
	 *  OnComponentSleep (the gib SETTLED = landed), with ActivationDelay repurposed as the MAX-CAP fallback (a
	 *  gib that never sleeps). False (caches/energy) keeps the pure ActivationDelay/immediate behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Loot")
	bool bArmOnSettle = false;

protected:
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Pawn + ASC + not Status.Death.* (a death burst must never re-collect into its own corpse). */
	bool IsViableCollector(const AActor* Candidate) const;

	/** E2 cause-B: arm the collect (called by the ActivationDelay timer, or immediately when the delay is 0). */
	void Arm();

	/** PRESENTATION (landing-on-settle): arm when the attach-parent physics body sleeps (the gib LANDED). Bound
	 *  to UPrimitiveComponent::OnComponentSleep in BeginPlay when bArmOnSettle is set. */
	UFUNCTION()
	void OnAttachParentSleep(UPrimitiveComponent* SleepingComponent, FName BoneName);

private:
	/** One-shot collect guard. */
	bool bCollected = false;

	/** E2 cause-B (presentation): the collect is inert until armed. Default true (delay 0 -> armed at BeginPlay);
	 *  set false + timer-armed when ActivationDelay > 0, so a just-spawned part presents before it's collectible. */
	bool bArmed = true;

	/** Timer that arms the collect after ActivationDelay (the presentation beat). */
	FTimerHandle ArmTimer;

	/** Dormancy mirror (avoid redundant FlushNetDormancy spam) -- only meaningful when the magnet is on. */
	bool bMagnetAwake = false;
};
