#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"

class FAdvancedPreviewScene;
class FAssetEditorModeManager;
class FWeaponAlignmentViewportClient;
class USkeletalMesh;
class UStaticMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USceneComponent;
class UTransformProxy;
class UCombinedTransformGizmo;
class AActor;

/**
 * Isolated 3-D preview viewport for the Weapon Alignment Studio panel.
 *
 * Owns its own FAssetEditorModeManager bound to the preview scene, which is
 * what lets the Interactive Tools Framework transform gizmos live and respond
 * to mouse input INSIDE this viewport (instead of the main level viewport).
 * Gizmos drive four "handle" scene components — one per IK target — and the
 * resulting offsets are published via OnOffsetsChanged for the toolkit readout.
 */
class WEAPONALIGNMENTSTUDIO_API SWeaponAlignmentViewport : public SEditorViewport
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnOffsetsChanged);

	SLATE_BEGIN_ARGS(SWeaponAlignmentViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWeaponAlignmentViewport() override;

	// ---- Scene manipulation ---------------------------------------------
	void SetCharacterMesh(USkeletalMesh* NewMesh);
	void SetWeaponMesh(UStaticMesh* NewMesh);
	void SetWeaponTransform(const FTransform& T);

	/** (Re)create the four IK-target gizmos. Call after a mesh swap. */
	void RebuildGizmos();

	/**
	 * Position the four handles to match a previously-baked asset's offsets, then
	 * rebuild gizmos on top. Hand offsets are weapon-relative; elbow offsets are
	 * shoulder-local and converted back to component space via the character ref
	 * skeleton (best-effort — needs a character mesh assigned for elbows).
	 */
	void LoadFromAsset(class UWeaponAlignmentDataAsset* Asset);

	/** Which IK target a numeric edit applies to. */
	enum class EHandle : uint8 { LeftHand, RightHand, LeftElbow, RightElbow };

	/** Set a hand handle's weapon-relative grip transform from numeric input, then rebuild. */
	void SetHandGripOffset(EHandle Which, const FTransform& WeaponRelative);

	/** Set an elbow handle's world location from numeric input, then rebuild. */
	void SetElbowWorldLocation(EHandle Which, const FVector& WorldLoc);

	/**
	 * Snap a hand handle to a named bone/socket on the character skeleton, so the
	 * grip starts exactly on (e.g.) hand_l. Keeps the handle's current rotation.
	 * Returns false if the bone isn't found.
	 */
	bool SnapHandToBone(EHandle Which, FName BoneName);

	/**
	 * Find the closest ref-skeleton bone to a handle's current position.
	 * @return bone name (NAME_None if no skeleton), and fills OutDistance (cm).
	 */
	FName GetClosestBoneToHandle(EHandle Which, float& OutDistance) const;

	// ---- Accessors ------------------------------------------------------
	USkeletalMeshComponent* GetCharacterMeshComponent() const { return CharacterMeshComp; }
	AActor*                 GetWeaponActor()            const { return WeaponActor;        }
	FAdvancedPreviewScene*  GetPreviewScene()           const;

	// Live offsets driven by the gizmos
	FTransform GetLiveLeftHandTransform()  const { return LiveLeftHandTransform;  }
	FVector    GetLiveLeftElbowLocation()  const { return LiveLeftElbowLocation;  }
	FTransform GetLiveRightHandTransform() const { return LiveRightHandTransform; }
	FVector    GetLiveRightElbowLocation() const { return LiveRightElbowLocation; }

	/** Fires whenever any gizmo moves — toolkit subscribes to refresh readouts. */
	FOnOffsetsChanged OnOffsetsChanged;

protected:
	//~ SEditorViewport
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget>              MakeViewportToolbar()       override;

private:
	void SpawnPreviewActors();
	void DestroyGizmos();

	TSharedPtr<FAdvancedPreviewScene>          PreviewScene;
	TSharedPtr<FAssetEditorModeManager>        ModeManager;
	TSharedPtr<FWeaponAlignmentViewportClient> ViewportClient;

	// Scene actors/components — owned by the preview scene's world
	USkeletalMeshComponent* CharacterMeshComp = nullptr;
	AActor*                 WeaponActor        = nullptr;
	UStaticMeshComponent*   WeaponMeshComp     = nullptr;

	// Draggable handle components (one per IK target), parented under the actors
	USceneComponent* LeftHandHandle  = nullptr;
	USceneComponent* RightHandHandle = nullptr;
	USceneComponent* LeftElbowHandle  = nullptr;
	USceneComponent* RightElbowHandle = nullptr;

	// ITF gizmos + proxies (kept alive via the gizmo manager owner key)
	UTransformProxy* LeftHandProxy  = nullptr;
	UTransformProxy* RightHandProxy = nullptr;
	UTransformProxy* LeftElbowProxy  = nullptr;
	UTransformProxy* RightElbowProxy = nullptr;

	// Live offsets (weapon-relative for hands, world for elbows)
	FTransform LiveLeftHandTransform  = FTransform::Identity;
	FVector    LiveLeftElbowLocation  = FVector::ZeroVector;
	FTransform LiveRightHandTransform = FTransform::Identity;
	FVector    LiveRightElbowLocation = FVector::ZeroVector;
};
