#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WeaponAlignmentStudioLibrary.generated.h"

class USkeletalMesh;
class UWeaponAlignmentDataAsset;

/**
 * Input bundle for one bake. Hand grips are weapon-relative transforms;
 * elbow poles are WORLD locations that the bake converts to shoulder-local
 * space using the character skeleton. Mirrors the GUI's live offsets so the
 * editor and scripted paths share one code path.
 */
USTRUCT(BlueprintType)
struct WEAPONALIGNMENTSTUDIO_API FWeaponAlignmentBakeParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Weapon Alignment")
	FTransform LeftHandGripWeaponRelative = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Weapon Alignment")
	FTransform RightHandGripWeaponRelative = FTransform::Identity;

	/** Elbow pole-vector positions in WORLD space (converted to shoulder-local at bake). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Weapon Alignment")
	FVector LeftElbowPoleWorld = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Weapon Alignment")
	FVector RightElbowPoleWorld = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Weapon Alignment")
	FName LeftHandBoneName = FName("hand_l");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Weapon Alignment")
	FName RightHandBoneName = FName("hand_r");

	/** Lower-arm bones used to find each shoulder (parent) for elbow-space conversion. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Weapon Alignment")
	FName LeftLowerArmBone = FName("lowerarm_l");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Weapon Alignment")
	FName RightLowerArmBone = FName("lowerarm_r");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Weapon Alignment")
	FString WeaponLabel;
};

/**
 * Headless, scriptable entry points for the Weapon Alignment Studio.
 *
 * Designed for AIK / Claude automation via the UE Python plugin:
 *   import unreal
 *   lib = unreal.WeaponAlignmentStudioLibrary
 *   params = unreal.WeaponAlignmentBakeParams()
 *   params.set_editor_property('left_hand_grip_weapon_relative', unreal.Transform(...))
 *   ok = lib.bake_alignment_asset('/Game/.../SKM_Quinn', params,
 *                                 '/Game/Weapons/AlignmentData', 'DA_Carbine')
 *
 * All functions are editor-only (the module is Editor type).
 */
UCLASS()
class WEAPONALIGNMENTSTUDIO_API UWeaponAlignmentStudioLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Compute IK offsets from the params and write a UWeaponAlignmentDataAsset to disk.
	 *
	 * @param CharacterMeshPath  Content path to the character USkeletalMesh (for shoulder-space math).
	 * @param Params             Grip transforms + elbow world positions + bone names.
	 * @param PackagePath        Destination content folder, e.g. "/Game/Weapons/AlignmentData".
	 * @param AssetName          Asset name without extension, e.g. "DA_Carbine".
	 * @param OutCreatedAsset    The created/overwritten asset (null on failure).
	 * @return true on success. On failure, OutError describes why.
	 */
	UFUNCTION(BlueprintCallable, Category="Weapon Alignment",
		meta=(DisplayName="Bake Weapon Alignment Asset"))
	static bool BakeAlignmentAsset(
		const FString& CharacterMeshPath,
		const FWeaponAlignmentBakeParams& Params,
		const FString& PackagePath,
		const FString& AssetName,
		UWeaponAlignmentDataAsset*& OutCreatedAsset,
		FString& OutError);

	/**
	 * Read an existing alignment asset back into a params struct (for inspection / re-bake).
	 * Note: elbow values come back in shoulder-local space (as stored), not world.
	 */
	UFUNCTION(BlueprintCallable, Category="Weapon Alignment",
		meta=(DisplayName="Load Weapon Alignment Asset"))
	static bool LoadAlignmentAsset(
		const FString& AssetPath,
		FWeaponAlignmentBakeParams& OutParams,
		FString& OutError);

	/**
	 * Serialise an alignment asset's offsets to a JSON string (for agent/chat round-trips).
	 */
	UFUNCTION(BlueprintCallable, Category="Weapon Alignment",
		meta=(DisplayName="Alignment Asset To JSON"))
	static FString AlignmentAssetToJson(const FString& AssetPath);

	/**
	 * Core math shared by the GUI bake button and the scripted path.
	 * Converts a world elbow location to the lower-arm's parent (shoulder) local space.
	 */
	static FVector WorldElbowToShoulderLocal(
		USkeletalMesh* CharacterMesh,
		const FVector& WorldElbow,
		FName LowerArmBone);

	/** Inverse of WorldElbowToShoulderLocal — shoulder-local back to component space. */
	static FVector ShoulderLocalToWorldElbow(
		USkeletalMesh* CharacterMesh,
		const FVector& ShoulderLocal,
		FName LowerArmBone);
};
