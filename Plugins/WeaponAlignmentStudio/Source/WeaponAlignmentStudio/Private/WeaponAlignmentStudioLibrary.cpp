#include "WeaponAlignmentStudioLibrary.h"
#include "WeaponAlignmentDataAsset.h"

#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "AnimationRuntime.h"

#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponAlignment, Log, All);

// ---------------------------------------------------------------------------
// Shoulder-space conversion (ref-pose, no spawned component needed)
// ---------------------------------------------------------------------------

FVector UWeaponAlignmentStudioLibrary::WorldElbowToShoulderLocal(
	USkeletalMesh* CharacterMesh, const FVector& WorldElbow, FName LowerArmBone)
{
	if (!CharacterMesh) { return WorldElbow; }

	const FReferenceSkeleton& RefSkel = CharacterMesh->GetRefSkeleton();
	const int32 LowerArmIdx = RefSkel.FindBoneIndex(LowerArmBone);
	if (LowerArmIdx == INDEX_NONE) { return WorldElbow; }

	// Shoulder = parent of the lower-arm (upper-arm). Fall back to the lower-arm
	// itself if it has no parent (shouldn't happen for a real arm chain).
	int32 ShoulderIdx = RefSkel.GetParentIndex(LowerArmIdx);
	if (ShoulderIdx == INDEX_NONE) { ShoulderIdx = LowerArmIdx; }

	const FTransform ShoulderCS =
		FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, ShoulderIdx);

	// WorldElbow is authored in the preview component space (component sits at the
	// origin in the preview scene), which matches ref-pose component space here.
	return ShoulderCS.InverseTransformPosition(WorldElbow);
}

FVector UWeaponAlignmentStudioLibrary::ShoulderLocalToWorldElbow(
	USkeletalMesh* CharacterMesh, const FVector& ShoulderLocal, FName LowerArmBone)
{
	if (!CharacterMesh) { return ShoulderLocal; }

	const FReferenceSkeleton& RefSkel = CharacterMesh->GetRefSkeleton();
	const int32 LowerArmIdx = RefSkel.FindBoneIndex(LowerArmBone);
	if (LowerArmIdx == INDEX_NONE) { return ShoulderLocal; }

	int32 ShoulderIdx = RefSkel.GetParentIndex(LowerArmIdx);
	if (ShoulderIdx == INDEX_NONE) { ShoulderIdx = LowerArmIdx; }

	const FTransform ShoulderCS =
		FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, ShoulderIdx);

	return ShoulderCS.TransformPosition(ShoulderLocal);
}

// ---------------------------------------------------------------------------
// Bake
// ---------------------------------------------------------------------------

bool UWeaponAlignmentStudioLibrary::BakeAlignmentAsset(
	const FString& CharacterMeshPath,
	const FWeaponAlignmentBakeParams& Params,
	const FString& PackagePath,
	const FString& AssetName,
	UWeaponAlignmentDataAsset*& OutCreatedAsset,
	FString& OutError)
{
	OutCreatedAsset = nullptr;
	OutError.Empty();

	// Resolve the character mesh (optional — only needed for elbow conversion).
	USkeletalMesh* CharMesh = nullptr;
	if (!CharacterMeshPath.IsEmpty())
	{
		CharMesh = LoadObject<USkeletalMesh>(nullptr, *CharacterMeshPath);
		if (!CharMesh)
		{
			OutError = FString::Printf(
				TEXT("Could not load character skeletal mesh: %s"), *CharacterMeshPath);
			return false;
		}
	}

	if (AssetName.IsEmpty() || PackagePath.IsEmpty())
	{
		OutError = TEXT("PackagePath and AssetName must both be non-empty.");
		return false;
	}

	// Elbow world -> shoulder-local
	const FVector LeftElbow  = WorldElbowToShoulderLocal(
		CharMesh, Params.LeftElbowPoleWorld,  Params.LeftLowerArmBone);
	const FVector RightElbow = WorldElbowToShoulderLocal(
		CharMesh, Params.RightElbowPoleWorld, Params.RightLowerArmBone);

	// Build package + asset
	const FString FullPath = (PackagePath / AssetName).TrimStartAndEnd();

	UPackage* Pkg = CreatePackage(*FullPath);
	if (!Pkg)
	{
		OutError = FString::Printf(TEXT("Failed to create package: %s"), *FullPath);
		return false;
	}
	Pkg->FullyLoad();

	UWeaponAlignmentDataAsset* Asset = NewObject<UWeaponAlignmentDataAsset>(
		Pkg, *AssetName, RF_Public | RF_Standalone);

	Asset->LeftHandGripOffset   = Params.LeftHandGripWeaponRelative;
	Asset->RightHandGripOffset  = Params.RightHandGripWeaponRelative;
	Asset->LeftElbowPoleOffset  = LeftElbow;
	Asset->RightElbowPoleOffset = RightElbow;
	Asset->LeftHandBoneName     = Params.LeftHandBoneName;
	Asset->RightHandBoneName    = Params.RightHandBoneName;
	Asset->WeaponLabel          = Params.WeaponLabel;

	Pkg->MarkPackageDirty();

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags       = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags           = SAVE_NoError;
	SaveArgs.bForceByteSwapping  = false;
	SaveArgs.bWarnOfLongFilename = false;

	const FString PkgFile = FPackageName::LongPackageNameToFilename(
		FullPath, FPackageName::GetAssetPackageExtension());

	if (!UPackage::SavePackage(Pkg, Asset, *PkgFile, SaveArgs))
	{
		OutError = FString::Printf(TEXT("Failed to save package to disk: %s"), *PkgFile);
		return false;
	}

	FAssetRegistryModule::AssetCreated(Asset);

	OutCreatedAsset = Asset;
	UE_LOG(LogWeaponAlignment, Log, TEXT("Baked alignment asset: %s"), *FullPath);
	return true;
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

bool UWeaponAlignmentStudioLibrary::LoadAlignmentAsset(
	const FString& AssetPath,
	FWeaponAlignmentBakeParams& OutParams,
	FString& OutError)
{
	OutError.Empty();

	UWeaponAlignmentDataAsset* Asset =
		LoadObject<UWeaponAlignmentDataAsset>(nullptr, *AssetPath);
	if (!Asset)
	{
		OutError = FString::Printf(TEXT("Could not load alignment asset: %s"), *AssetPath);
		return false;
	}

	OutParams.LeftHandGripWeaponRelative  = Asset->LeftHandGripOffset;
	OutParams.RightHandGripWeaponRelative = Asset->RightHandGripOffset;
	// NOTE: stored elbow values are shoulder-local; surfaced as-is.
	OutParams.LeftElbowPoleWorld  = Asset->LeftElbowPoleOffset;
	OutParams.RightElbowPoleWorld = Asset->RightElbowPoleOffset;
	OutParams.LeftHandBoneName    = Asset->LeftHandBoneName;
	OutParams.RightHandBoneName   = Asset->RightHandBoneName;
	OutParams.WeaponLabel         = Asset->WeaponLabel;

	return true;
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

static void TransformToJson(const FTransform& T, const TSharedRef<FJsonObject>& Obj)
{
	const FVector L = T.GetLocation();
	const FRotator R = T.GetRotation().Rotator();
	const FVector S = T.GetScale3D();

	TSharedRef<FJsonObject> Loc = MakeShared<FJsonObject>();
	Loc->SetNumberField(TEXT("x"), L.X); Loc->SetNumberField(TEXT("y"), L.Y); Loc->SetNumberField(TEXT("z"), L.Z);
	TSharedRef<FJsonObject> Rot = MakeShared<FJsonObject>();
	Rot->SetNumberField(TEXT("pitch"), R.Pitch); Rot->SetNumberField(TEXT("yaw"), R.Yaw); Rot->SetNumberField(TEXT("roll"), R.Roll);
	TSharedRef<FJsonObject> Scale = MakeShared<FJsonObject>();
	Scale->SetNumberField(TEXT("x"), S.X); Scale->SetNumberField(TEXT("y"), S.Y); Scale->SetNumberField(TEXT("z"), S.Z);

	Obj->SetObjectField(TEXT("location"), Loc);
	Obj->SetObjectField(TEXT("rotation"), Rot);
	Obj->SetObjectField(TEXT("scale"), Scale);
}

static void VectorToJson(const FVector& V, const TSharedRef<FJsonObject>& Obj)
{
	Obj->SetNumberField(TEXT("x"), V.X);
	Obj->SetNumberField(TEXT("y"), V.Y);
	Obj->SetNumberField(TEXT("z"), V.Z);
}

FString UWeaponAlignmentStudioLibrary::AlignmentAssetToJson(const FString& AssetPath)
{
	UWeaponAlignmentDataAsset* Asset =
		LoadObject<UWeaponAlignmentDataAsset>(nullptr, *AssetPath);
	if (!Asset)
	{
		return FString::Printf(
			TEXT("{\"error\":\"could not load %s\"}"), *AssetPath);
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset"), AssetPath);
	Root->SetStringField(TEXT("weaponLabel"), Asset->WeaponLabel);
	Root->SetStringField(TEXT("leftHandBone"), Asset->LeftHandBoneName.ToString());
	Root->SetStringField(TEXT("rightHandBone"), Asset->RightHandBoneName.ToString());

	TSharedRef<FJsonObject> LHand = MakeShared<FJsonObject>();
	TransformToJson(Asset->LeftHandGripOffset, LHand);
	Root->SetObjectField(TEXT("leftHandGripOffset"), LHand);

	TSharedRef<FJsonObject> RHand = MakeShared<FJsonObject>();
	TransformToJson(Asset->RightHandGripOffset, RHand);
	Root->SetObjectField(TEXT("rightHandGripOffset"), RHand);

	TSharedRef<FJsonObject> LElbow = MakeShared<FJsonObject>();
	VectorToJson(Asset->LeftElbowPoleOffset, LElbow);
	Root->SetObjectField(TEXT("leftElbowPoleOffset"), LElbow);

	TSharedRef<FJsonObject> RElbow = MakeShared<FJsonObject>();
	VectorToJson(Asset->RightElbowPoleOffset, RElbow);
	Root->SetObjectField(TEXT("rightElbowPoleOffset"), RElbow);

	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}
