// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLOverkillMessage.generated.h"

/**
 * Broadcast by UAFLDamageExecCalc on Event.Damage.Overkill.AFL, alongside the
 * Lyra FLyraVerbMessage on Event.Damage.Overkill. Carries BoneName so the
 * dismember system (UAFLDismemberComponent, S4-04) knows which body zone the
 * killing blow struck — the bone is available in the exec calc's hit context
 * but FLyraVerbMessage has no field for it, so we broadcast a dedicated AFL
 * struct rather than smuggle a bone into a Lyra type whose schema we don't own
 * (same rationale as FAFLHitConfirmMessage).
 */
USTRUCT(BlueprintType)
struct AFLCOMBAT_API FAFLOverkillMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="AFL|Overkill")
	TObjectPtr<UObject> Instigator = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="AFL|Overkill")
	TObjectPtr<UObject> Target = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="AFL|Overkill")
	FName BoneName = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category="AFL|Overkill")
	double Magnitude = 0.0;
};
