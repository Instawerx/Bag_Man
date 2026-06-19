// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLDismemberedPart.h"

#include "AFLDismemberedHead.generated.h"

class USoundBase;
class UStaticMesh;
class UAFLSkinColorAsset;
class UMaterialInstanceConstant;

/**
 * S4-05b / S4-INC1: the detached HEAD prop -- a subclass of the extracted AAFLDismemberedPart base.
 *
 * S4 REAL-HEAD-GIB (AFL-0408-FU-HEADMESH, PHASE 3): the head prop is the VICTIM'S ACTUAL HEAD -- a head-only
 * STATIC mesh gib (SM_AFL_RobotHead_Gib) extracted from SK_Mannequin by head/neck bone weight in Blender
 * (origin-centered at its volume center, watertight, ~18x23x27cm, ONE convex collision hull). It IS the base
 * PartMesh (UStaticMeshComponent) -- the proven prim-as-root physics body -- so it simulates + rolls + replicates
 * + grabs on the SAME path every limb prop uses. NO skeletal mesh, NO bone-isolate, NO centering offset: the
 * earlier "isolate SKM_Manny to head-only by HideBoneByName" produced a SHARD (the skull verts are skin-weighted
 * across head+neck+spine, so hiding the neck collapses most of the face) -- that whole approach is GONE, replaced
 * by the real extracted gib. ApplyPopImpulse is overridden to a VELOCITY pop (bVelChange=true) so the light
 * head-sized body does not launch off-screen. Physics-driven tumble, NO roll anim.
 *
 * IDENTITY (two replicated layers, mirroring the live robot's head exactly):
 *   1) HeadMaterial -- the VICTIM's per-skin slot-1 base material (a UMaterialInstanceConstant content asset,
 *      e.g. MI_AFL_FaceMask_Pink / MI_IRONICS_Limbs_*; resolved replication-safe at spawn via GetMaterial(1)->Parent,
 *      NEVER the transient runtime MID). Assigned to BOTH gib slots so the head reads as that specific robot.
 *   2) HeadSkinColor -- the victim's UAFLSkinColorAsset, applied ON TOP via the proven AAFLCharacterPartActor
 *      per-slot CreateAndSetMaterialInstanceDynamic + GetColors()/GetScalars() loop (M_Mannequin params
 *      EmissiveColor/2/3, EdgeGlowColor, ...). The MIC gives the material LOOK; the color asset gives the TINT.
 * Both are ReplicatedUsing=OnRep so the appearance reaches every client + late-joiner; the apply re-runs on each
 * OnRep so whichever of the two lands second completes the look (OnRep order is not guaranteed).
 *
 * AFL-0404 (audio): on BeginPlay the head plays its RollSound (Head_Roll_1) attached to the prop so the
 * roll audio follows the tumble. The electrical-POP at detach is a separate replicated GameplayCue.
 */
UCLASS()
class AFLDISMEMBER_API AAFLDismemberedHead : public AAFLDismemberedPart
{
	GENERATED_BODY()

public:
	AAFLDismemberedHead();

	/** Back-compat accessor (callers used GetSphereMesh()); forwards to the base static PartMesh (now the gib). */
	UStaticMeshComponent* GetSphereMesh() const { return GetPartMesh(); }

	/** SERVER-ONLY: hand the victim's skin color to the head (replicates to all clients via OnRep).
	 *  Called by UAFLDismemberComponent at sever/spawn from the victim's UAFLSkinColorComponent. */
	void SetHeadSkinColor(UAFLSkinColorAsset* InColor);

	/** SERVER-ONLY: hand the victim's per-skin head MATERIAL (the replication-safe slot-1 base MIC) to the head
	 *  (replicates via OnRep). Resolved by UAFLDismemberComponent at spawn from the victim robot's GetMaterial(1)
	 *  ->Parent (the MIC behind the runtime MID). Mirrors SetHeadSkinColor. */
	void SetHeadMaterial(UMaterialInstanceConstant* InMaterial);

	/** Pop the gib PartMesh (the head's physics body) with bVelChange=TRUE -- a target VELOCITY, not a force.
	 *  The head gib is real-head-sized and light; the base's force-pop (bVelChange=false) divides by that small
	 *  mass and LAUNCHES the head off-screen. Velocity-pop is mass/scale-independent + predictable. Limbs keep
	 *  the base force-pop. (S4 TUMBLE FIX.) */
	virtual void ApplyPopImpulse(const FVector& Linear, const FVector& Angular = FVector::ZeroVector) override;

protected:
	/** Read-only access to the head's own gib mesh asset for subclasses (e.g. AAFLHeadLootBox passes it as the
	 *  scattered-loot mesh in LootGrant->Configure). The field stays private/EditDefaults-set; subclasses only read it. */
	UStaticMesh* GetHeadGibMesh() const { return HeadGibMesh; }

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Apply the victim appearance to the gib (runs on every client): assign HeadMaterial (the MIC) to BOTH gib
	 *  slots, then drive the color params via a per-slot CreateAndSetMaterialInstanceDynamic + GetColors()/
	 *  GetScalars() loop (the AAFLCharacterPartActor::ApplySkinColor pattern). Idempotent; re-run on each OnRep so
	 *  whichever of {HeadMaterial, HeadSkinColor} replicates second completes the look. */
	void ApplyHeadAppearance();

	UFUNCTION()
	void OnRep_HeadSkinColor();

	UFUNCTION()
	void OnRep_HeadMaterial();

private:
	/** The head gib mesh -- SM_AFL_RobotHead_Gib (origin-centered head-only static mesh + convex collision).
	 *  EditDefaultsOnly so a future purpose-built head prop is a one-asset swap, no code. Set on PartMesh in ctor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> HeadGibMesh;

	/** The victim's skin color, replicated so the head reads as identity on every client + late-join. */
	UPROPERTY(ReplicatedUsing = OnRep_HeadSkinColor)
	TObjectPtr<UAFLSkinColorAsset> HeadSkinColor;

	/** The victim's per-skin head base material (slot-1 MIC), replicated so the gib looks like that robot.
	 *  A UMaterialInstanceConstant content asset = replication-safe by pointer (NEVER the transient MID). */
	UPROPERTY(ReplicatedUsing = OnRep_HeadMaterial)
	TObjectPtr<UMaterialInstanceConstant> HeadMaterial;

	/** AFL-0404: the rolling-head audio (Head_Roll_*), played attached to the prop on BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> RollSound;
};
