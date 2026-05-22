// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLHitConfirmInfo.generated.h"


/**
 * FAFLHitConfirmInfo
 *
 * Payload passed to OnHitConfirmed listeners (WBP_AFL_HitMarker, future
 * damage-number popups, telemetry). Filled in by UAFLHitConfirmComponent
 * when it receives an Event.Damage.Confirmed gameplay message broadcast
 * by UAFLDamageExecCalc.
 *
 * Per master doc §AFL-0204: this is the smallest payload that lets the UI
 * differentiate headshot vs body, distance-tune the reticle pulse, and (in
 * AFL-0204b) drive the floating damage number. Headshot detection here is a
 * placeholder bone-name comparison — AFL-0411 replaces it with a body-zone
 * enum lookup once the per-pawn zone map exists.
 */
USTRUCT(BlueprintType)
struct AFLCOMBAT_API FAFLHitConfirmInfo
{
	GENERATED_BODY()

	/** Effective damage applied by the GE (post-mitigation, post-shield spillover). */
	UPROPERTY(BlueprintReadOnly, Category="AFL|HitConfirm")
	float Damage = 0.0f;

	/** Bone name lifted from the FHitResult on the EffectContext. None when no hit result attached. */
	UPROPERTY(BlueprintReadOnly, Category="AFL|HitConfirm")
	FName BoneName = NAME_None;

	/** True if BoneName matches the head-zone heuristic. AFL-0411 will swap this for the zone-enum lookup. */
	UPROPERTY(BlueprintReadOnly, Category="AFL|HitConfirm")
	bool bHeadshot = false;

	/** Distance from the firing client's camera origin to the impact point, centimetres. 0 when unknown. */
	UPROPERTY(BlueprintReadOnly, Category="AFL|HitConfirm")
	float DistanceCm = 0.0f;
};
