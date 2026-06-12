// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "AFLStressObjectComponent.generated.h"

/**
 * UAFLStressObjectComponent  (P2 close-out stress-object chaos -- S10 AFL-0804)
 *
 * The instability half of the stress object (the multipliers ride the carrier GE + the extract GA;
 * the carry/throw/drop all reuse the proven grab verb set). Server-authoritative.
 *
 * INSTABILITY: while the owning grabbable is HELD (its replicated bHeld is true), accumulate a held
 * timer; when it exceeds afl.Chaos.InstabilitySeconds (default 45), REPOSITION -- teleport the object
 * a configured distance away (forcing a re-grab) + a brief AFL_CHAOS log/notify. Glow-ramp +
 * squishy physics + audio chirps are NAMED FEEL-DEBT -- this component exposes the instability hook
 * and the reposition; the cosmetics are a later pass.
 *
 * Added to the stress-object BP (BP_AFL_StressObject) alongside its AFLGrabbableComponent.
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLStressObjectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLStressObjectComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** How far the object teleports on a reposition (forces a re-grab). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Chaos", meta = (ClampMin = "0.0"))
	float RepositionDistance = 800.0f;

private:
	/** Is the owning grabbable currently held? Reads the grabbable's replicated bHeld by reflection so
	 *  this component carries no AFLMovement dependency (the stress component lives in AFLCombat). */
	bool IsOwnerHeld() const;

	float HeldSeconds = 0.0f;
	bool bWasHeld = false;
};
