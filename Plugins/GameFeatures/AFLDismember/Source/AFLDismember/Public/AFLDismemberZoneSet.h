// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "AFLDismemberTypes.h"

#include "AFLDismemberZoneSet.generated.h"

/**
 * S4-INC1: the data-driven dismember table. One DataAsset authored in PHASE B
 * (DA_AFL_DismemberZones) holds the per-zone rows; UAFLDismemberComponent reads it
 * (soft-ref) and resolves the killing-blow bone -> zone row -> sever.
 *
 * EditDefaultsOnly array so the rows are authored in the asset, never in code.
 */
UCLASS(BlueprintType)
class AFLDISMEMBER_API UAFLDismemberZoneSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The per-zone dismember rows (authored in DA_AFL_DismemberZones, PHASE B). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember")
	TArray<FAFLDismemberZone> Zones;

	/**
	 * Resolve a hit bone to its zone row by linear-scanning each row's BoneMatches.
	 * Returns nullptr if no row claims the bone (-> no dismemberment, the killing
	 * blow just kills). Const + pointer-into-array (no copy); the caller must not
	 * outlive the asset (it is held by a soft-ref the component keeps resolved).
	 */
	const FAFLDismemberZone* FindZoneForBone(FName Bone) const;
};
