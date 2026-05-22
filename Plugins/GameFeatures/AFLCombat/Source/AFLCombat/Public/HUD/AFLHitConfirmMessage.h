// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "AFLHitConfirmMessage.generated.h"


/**
 * FAFLHitConfirmMessage
 *
 * Broadcast on Event.Damage.Confirmed by UAFLDamageExecCalc whenever a damage
 * GE resolves with EffectiveDamage > 0. Consumed by UAFLHitConfirmComponent
 * on the firing client to drive crosshair pulse, camera shake, and AFL-0204b
 * floating damage numbers.
 *
 * Mirrors FLyraVerbMessage's Instigator/Target/Magnitude shape, plus the
 * bone-name pulled from the FHitResult on the spec's EffectContext. We keep
 * a dedicated struct (instead of reusing FLyraVerbMessage) because the bone
 * name and is-headshot bit are AFL-0411 inputs and shouldn't be smuggled into
 * a Lyra type whose schema we don't own.
 */
USTRUCT(BlueprintType)
struct AFLCOMBAT_API FAFLHitConfirmMessage
{
	GENERATED_BODY()

	/** Firing pawn / effect causer. Listeners filter for "we instigated this" by comparing against their owned pawn. */
	UPROPERTY(BlueprintReadWrite, Category="AFL|HitConfirm")
	TObjectPtr<UObject> Instigator = nullptr;

	/** Hit pawn / actor. */
	UPROPERTY(BlueprintReadWrite, Category="AFL|HitConfirm")
	TObjectPtr<UObject> Target = nullptr;

	/** Effective damage after mitigation + shield-spillover. Always > 0 at broadcast time. */
	UPROPERTY(BlueprintReadWrite, Category="AFL|HitConfirm")
	float Damage = 0.0f;

	/** Bone name from FHitResult.BoneName. NAME_None if the hit result was missing or boneless. */
	UPROPERTY(BlueprintReadWrite, Category="AFL|HitConfirm")
	FName BoneName = NAME_None;

	/** Distance from EffectContext::Origin (claimed view origin, AFL-0106) to impact. 0 when unavailable. */
	UPROPERTY(BlueprintReadWrite, Category="AFL|HitConfirm")
	float DistanceCm = 0.0f;
};
