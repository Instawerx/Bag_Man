// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "AFLEnergyDropComponent.generated.h"

class AAFLEnergyPickup;

/**
 * UAFLEnergyDropComponent  (energy drops cycle 1 -- the death burst)
 *
 * Server-only listener that spawns an energy-pickup burst when the owning pawn dies. Binds
 * ULyraHealthComponent::OnDeathStarted (the AFLTargetDummy precedent: a plain pawn-local dynamic
 * delegate, no ASC-resolution race) -- it does NOT touch UAFLDeathComponent, which remains the
 * sole driver of the death sequence; this component only OBSERVES the same replicated signal.
 *
 * On death: read the victim's CarriedEnergy -> drop DropPercent (cvar afl.Energy.DropPercent,
 * default 70) of it as a ring of pickups at the corpse (tier mix favoring smalls: L while
 * remaining >= 50, then M while >= 25, then S = ceil(rest/10)) -> reduce the victim by the
 * dropped amount via a NEGATIVE SetByCaller application of the gain GE (the rail: no direct
 * attribute writes). One burst per death (idempotence flag; respawn = fresh pawn = fresh
 * component). "Overload-instead-of-death" is a later mode re-skin -- this rides the PROVEN
 * death path by design (the cycle-1 grounding D verdict).
 *
 * Granted to the hero as a BP child (B_AFL_EnergyDropComponent sets the tier classes) via the
 * experience AddComponents row, server-only flag.
 */
UCLASS(ClassGroup = (AFL), Blueprintable, meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLEnergyDropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLEnergyDropComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Drop Percent of the owner's CarriedEnergy as a pickup ring NOW (server-only; returns the
	 *  dropped total, 0 when nothing dropped). Extracted from the death path so other consumers
	 *  ride the SAME rail -- first caller: UAFLAG_Extract's damage interrupt (AFL-0808, 100%).
	 *  Does NOT touch the one-per-death flag; spam-guarding belongs to the caller's lifecycle.
	 *  RE-COLLECT WRINKLE (named feel debt): dropped pickups have no instigator magnet-immunity
	 *  window, so a LIVING owner standing in the ring vacuums them back within ~1s (the death
	 *  path never sees this -- corpses are magnet-invisible). Cycle 1 ships it as-is; the harness
	 *  asserts the burst exists before the magnet and reports re-collection explicitly. */
	float BurstNow(float Percent, const TCHAR* Reason);

protected:
	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	/** Tier pickup classes (EnergyValue 50/25/10). Set on the BP child component. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Energy")
	TSubclassOf<AAFLEnergyPickup> LargePickupClass;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Energy")
	TSubclassOf<AAFLEnergyPickup> MediumPickupClass;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Energy")
	TSubclassOf<AAFLEnergyPickup> SmallPickupClass;

	/** Ring radius (uu) the burst scatters to around the corpse. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Energy", meta = (ClampMin = "0.0"))
	float ScatterRadius = 120.0f;

private:
	/** One burst per death. */
	bool bBurstDone = false;
};
