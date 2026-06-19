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

	/** Damage that spilled PAST the zone-HP into the body chain on the depleting hit. */
	UPROPERTY(BlueprintReadWrite, Category="AFL|Dismember")
	double Overflow = 0.0;

	/** PRESENTATION: the shot/impact direction (world-space, normalized) so the severed gib pops AWAY from the
	 *  shooter. Filled by UAFLDamageExecCalc from the hit's TraceStart->TraceEnd (else -ImpactNormal); ZeroVector
	 *  when unknown -> the gib pop falls to a forward cone. NOT replicated -- the broadcast is server-local; the
	 *  consumer (SeverZone) spawns the replicated gib that carries the resulting impulse via snapshot movement. */
	UPROPERTY(BlueprintReadWrite, Category="AFL|Dismember")
	FVector HitDirection = FVector::ZeroVector;
};
