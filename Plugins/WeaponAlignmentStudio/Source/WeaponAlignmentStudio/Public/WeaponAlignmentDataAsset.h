#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponAlignmentDataAsset.generated.h"

/**
 * Serialised output of one weapon-alignment session.
 * Runtime IK systems (Control Rig, AnimBP) read this asset.
 * Nothing in this class is editor-only — it compiles in game modules too.
 */
UCLASS(BlueprintType, meta=(DisplayName="Weapon Alignment Data"))
class WEAPONALIGNMENTSTUDIO_API UWeaponAlignmentDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Left-hand grip transform relative to the weapon's actor root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="IK Targets|Left Hand")
	FTransform LeftHandGripOffset = FTransform::Identity;

	/** Elbow pole-vector in the left upper-arm (shoulder) local space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="IK Targets|Left Hand")
	FVector LeftElbowPoleOffset = FVector::ZeroVector;

	/** Right-hand grip transform relative to the weapon's actor root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="IK Targets|Right Hand")
	FTransform RightHandGripOffset = FTransform::Identity;

	/** Elbow pole-vector in the right upper-arm (shoulder) local space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="IK Targets|Right Hand")
	FVector RightElbowPoleOffset = FVector::ZeroVector;

	/** Human-readable label shown in the Alignment Studio UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meta")
	FString WeaponLabel;

	/** Bone used as the left-hand grip reference during baking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meta")
	FName LeftHandBoneName = NAME_None;

	/** Bone used as the right-hand grip reference during baking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Meta")
	FName RightHandBoneName = NAME_None;
};
