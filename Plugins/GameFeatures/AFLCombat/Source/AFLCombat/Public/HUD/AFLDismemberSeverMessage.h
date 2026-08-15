// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AFLBodyZone.h"   // EAFLBodyZone (AFLCore)

#include "AFLDismemberSeverMessage.generated.h"

/**
 * S4-INC3: broadcast by UAFLDamageExecCalc on Event.Dismember.Sever.AFL when a zone's
 * dedicated zone-HP depletes to <= 0 on a hit (the LIVE hybrid sever trigger -- a limb
 * falls off when ITS HP runs out, decoupled from the body-overkill that AFL-0408 uses).
 * Clones FAFLOverkillMessage's shape (Instigator/Target/BoneName/Magnitude) and adds the
 * resolved Zone, whether the sever was lethal (Head decapitation), and the Overflow damage
 * that spilled past the zone-HP into the body chain.
 *
 * The dismember system (UAFLDismemberComponent, PHASE B) listens to this to drive the live
 * SeverZone (bone-hide + prop + cue + consequence GE), replacing the overkill trigger for
 * limbs. Head decapitation still also contributes its overflow to Health (the lethal zone).
 */
USTRUCT(BlueprintType)
struct AFLCOMBAT_API FAFLDismemberSeverMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="AFL|Dismember")
	TObjectPtr<UObject> Instigator = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="AFL|Dismember")
	TObjectPtr<UObject> Target = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="AFL|Dismember")
	FName BoneName = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category="AFL|Dismember")
	EAFLBodyZone Zone = EAFLBodyZone::None;

	/** True only for Head (decapitation = lethal); limbs are survivable severs. */
	UPROPERTY(BlueprintReadWrite, Category="AFL|Dismember")
	bool bLethal = false;

	/**
	 * EVERYTHING THAT REACHED THE BODY from the depleting hit -- spill AND bleed-through.
	 *
	 * ⚠ THIS SAID "damage that spilled PAST the zone-HP" AND THAT STOPPED BEING TRUE AT e25f6dda. Limbs now
	 * absorb 65% and always pass the rest on, so a depleting hit reaches the body by two routes:
	 *
	 *     SPILL  damage in excess of the zone's HP        (EffectiveDamage - ZoneHP)
	 *     BLEED  the 35% withheld from what it DID absorb (0.35 * Absorbed)
	 *
	 * On a 1.2 hit into a 0.8 limb that is 0.40 + 0.28 = 0.68, and this field carries 0.68.
	 *
	 * THE BROADER MEANING IS THE CORRECT ONE, not merely the one the code happens to produce. After
	 * bleed-through, "damage that spilled past the zone HP" describes neither the damage dealt nor the damage
	 * the body received -- it is a bookkeeping figure for a model that no longer runs. A consumer scaling a
	 * gib impulse, or asking how hard the shot carried through, wants what actually landed.
	 */
	UPROPERTY(BlueprintReadWrite, Category="AFL|Dismember")
	double Overflow = 0.0;

	/** PRESENTATION: the shot/impact direction (world-space, normalized) so the severed gib pops AWAY from the
	 *  shooter. Filled by UAFLDamageExecCalc from the hit's TraceStart->TraceEnd (else -ImpactNormal); ZeroVector
	 *  when unknown -> the gib pop falls to a forward cone. NOT replicated -- the broadcast is server-local; the
	 *  consumer (SeverZone) spawns the replicated gib that carries the resulting impulse via snapshot movement. */
	UPROPERTY(BlueprintReadWrite, Category="AFL|Dismember")
	FVector HitDirection = FVector::ZeroVector;
};
