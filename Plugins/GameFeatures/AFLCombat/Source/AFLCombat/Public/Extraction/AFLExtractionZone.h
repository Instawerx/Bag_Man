// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"

#include "AFLExtractionZone.generated.h"

class USphereComponent;
class UGameplayEffect;

/**
 * AAFLExtractionZone  (extraction cycle 1 -- S8 AFL-0802 thin cut: ONE always-active zone)
 *
 * A replicated tag dispenser, nothing more: server-side overlap applies/removes
 * AFLGE_InExtractionZone on pawns with an ASC, tracked BY HANDLE per pawn (the grab ability's
 * carrier-GE precedent). The zone knows NOTHING about channeling -- UAFLAG_Extract owns the
 * channel and self-cancels when the tag drops; the reward commits inside the GA.
 *
 * Net posture = the pickup ctor minus motion (AFLEnergyPickup.cpp:32-38 lineage): bReplicates
 * from birth so the BP child's visuals exist on every client, but static -> no replicated
 * movement, no dormancy churn, low NetUpdateFrequency, no cull (a zone must render at range).
 *
 * CONTESTED SEAM (zero-rework, documented for cycle 2+): zone states (Inactive/Active/Contested,
 * AFL-0802) land as a replicated EAFLZoneState + per-team occupancy derived from this same
 * overlap set; Contested applies a SECOND GE granting State.Zone.Contested which the GA lists in
 * ActivationBlockedTags (and optionally strips this GE to cancel in-flight channels). The GA and
 * this class's overlap contract never change.
 *
 * Visuals live on the BP child (B_AFL_ExtractionZone -- brand MI glow volume); placement is
 * map-side. The harness spawns the raw C++ class at a fixed offset (config-on-actor posture:
 * everything functional is class-default).
 */
UCLASS(Blueprintable, BlueprintType)
class AFLCOMBAT_API AAFLExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	AAFLExtractionZone();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** The zone volume (root). Radius is the designer knob on the BP child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Extraction")
	TObjectPtr<USphereComponent> ZoneSphere;

	/** The tag-dispenser GE (default AFLGE_InExtractionZone; BP child may sub a variant). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	TSubclassOf<UGameplayEffect> ZoneEffectClass;

private:
	/** Live per-pawn handles (the carrier-GE handle-tracking precedent). Weak keys: a pawn that
	 *  dies/despawns inside the zone takes its ASC (and the GE) with it -- stale entries are
	 *  swept on EndOverlap/EndPlay, never double-removed. */
	TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> ZoneEffectHandles;
};
