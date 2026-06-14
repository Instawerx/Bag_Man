// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLDismemberedPart.h"

#include "AFLDismemberedHead.generated.h"

class USoundBase;
class USkeletalMesh;
class USkeletalMeshComponent;
class UAFLSkinColorAsset;

/**
 * S4-05b / S4-INC1: the detached HEAD prop -- a subclass of the extracted AAFLDismemberedPart base.
 *
 * S4 BLANK-BALL FIX (AFL-0408-FU-HEADMESH): the head no longer renders the engine-sphere placeholder.
 * It carries its OWN USkeletalMeshComponent (HeadMesh) showing SKM_Manny with every bone EXCEPT 'head'
 * hidden (the INVERSE of UAFLDismemberComponent::GatherZoneMeshes -- head is a leaf bone on SK_Mannequin,
 * parent neck_02, zero children, so "head only" = hide all-but-head). Zero new art -- a dedicated
 * lightweight head-prop mesh is a LATER optimization (a full SKM_Manny per head is heavier; pooling =
 * AFL-0410); the data-driven HeadMeshAsset soft-ptr makes that future swap a one-asset change, no code.
 *
 * S4 TUMBLE FIX (Option 1): the head ROLLS via a simple sphere PHYSICS BODY -- the skeletal mesh is a
 * NON-simulating cosmetic visual that RIDES the sphere. The first HEADMESH cut made the skeletal HeadMesh
 * the simulating root, but IsolateHeadBone's HideBoneByName(PBO_Term) x163 TERMINATED the PA_Mannequin
 * ragdoll bodies -> a one-body "ragdoll" has nothing to articulate -> it pinned at origin and never rolled.
 * The fix RESTORES the base AAFLDismemberedPart's proven prim-as-root: the base PartMesh (UStaticMeshComponent,
 * a ~0.15-0.18-scale engine sphere) is the HIDDEN simulating proxy that rolls + replicates (the exact limb-prop
 * physics shape); the skeletal HeadMesh attaches to it as a non-sim child, offset DOWN by the 'head' bone's
 * ref-pose height so the rendered head centers on the sphere. This also un-does two latent bugs the skeletal-root
 * cut introduced: the grab detach-on-sim tripwire (prim-as-root is the shape the grab system wants) and the
 * ReleaseActor Cast<UPrimitiveComponent>(RootComponent) targeting the dead skeletal ragdoll. ApplyPopImpulse is
 * INHERITED from the base (pops the sphere PartMesh) -- no override. Physics-driven tumble, NO roll anim.
 *
 * IDENTITY: the prop carries the VICTIM's replicated skin color (HeadSkinColor), applied to the head
 * mesh's M_HeadLegs slot via the same CreateAndSetMaterialInstanceDynamic + GetColors()/GetScalars() loop
 * AAFLCharacterPartActor uses -- so you recognize WHOSE head it is (pink robot -> pink head). Replicated
 * (ReplicatedUsing=OnRep) so the color reaches all clients + late-joiners, mirroring UAFLSkinColorComponent.
 *
 * AFL-0404 (audio): on BeginPlay the head plays its RollSound (Head_Roll_1) attached to the mesh so the
 * roll audio follows the tumble. The electrical-POP at detach is a separate replicated GameplayCue.
 */
UCLASS()
class AFLDISMEMBER_API AAFLDismemberedHead : public AAFLDismemberedPart
{
	GENERATED_BODY()

public:
	AAFLDismemberedHead();

	/** Back-compat accessor (callers used GetSphereMesh()); forwards to the base static PartMesh. */
	UStaticMeshComponent* GetSphereMesh() const { return GetPartMesh(); }

	/** The skeletal head display (SKM_Manny, head-bone-only) -- a NON-simulating cosmetic visual that rides
	 *  the sphere PartMesh root (S4 TUMBLE FIX). */
	USkeletalMeshComponent* GetHeadMesh() const { return HeadMesh; }

	/** SERVER-ONLY: hand the victim's skin color to the head (replicates to all clients via OnRep).
	 *  Called by UAFLDismemberComponent at sever/spawn from the victim's UAFLSkinColorComponent. */
	void SetHeadSkinColor(UAFLSkinColorAsset* InColor);

	/** Pop the sphere PartMesh (the head's physics body) with bVelChange=TRUE -- a target VELOCITY, not a
	 *  force. The head sphere is head-sized (~0.21) and very light (mass ~ scale^3); the base's force-pop
	 *  (bVelChange=false) divides by that tiny mass and LAUNCHES the head off-screen. Velocity-pop is
	 *  mass/scale-independent + predictable. Limbs keep the base force-pop. (S4 TUMBLE FIX.) */
	virtual void ApplyPopImpulse(const FVector& Impulse) override;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Show only the head: hide every bone except 'head' on HeadMesh (inverse GatherZoneMeshes). Render-only
	 *  now (HeadMesh is non-simulating) -> PBO_None, not PBO_Term: hide the geometry without touching bodies. */
	void IsolateHeadBone();

	/** S4 TUMBLE FIX: center the rendered head on the sphere body. The SK_Manny 'head' bone sits ~165cm up at
	 *  ref pose; left at relative-zero the head would float a metre above the rolling sphere. Query the 'head'
	 *  bone's component-space ref-pose transform and SetRelativeLocation to NEGATE it, so the head-bone region
	 *  lands on the sphere origin. Resolved FROM THE BONE (not a magic constant) -> robust to a future mesh swap.
	 *  Falls back to HeadVisualOffset if the bone can't be resolved. */
	void CenterHeadVisualOnBody();

	/** Apply HeadSkinColor to the head mesh's material (clone of AAFLCharacterPartActor's apply). Idempotent. */
	void ApplyHeadSkinColor();

	UFUNCTION()
	void OnRep_HeadSkinColor();

	/** The head display mesh -- SKM_Manny, head-bone-only. NON-simulating cosmetic visual: a child of the
	 *  sphere PartMesh root that rides its physics tumble (S4 TUMBLE FIX; the sphere is the physics body). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> HeadMesh;

private:
	/** Data-driven head mesh (soft, EditDefaultsOnly -> the BP child sets SKM_Manny; a future purpose-built
	 *  head prop is a one-asset swap here, no code). Replicates implicitly via the archetype default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<USkeletalMesh> HeadMeshAsset;

	/** The victim's skin color, replicated so the head reads as identity on every client + late-join. */
	UPROPERTY(ReplicatedUsing = OnRep_HeadSkinColor)
	TObjectPtr<UAFLSkinColorAsset> HeadSkinColor;

	/** AFL-0404: the rolling-head audio (Head_Roll_*), played attached to the mesh on BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> RollSound;

	/** S4 TUMBLE FIX: fallback head-visual offset used ONLY if the 'head' bone can't be resolved at BeginPlay
	 *  (CenterHeadVisualOnBody prefers the live bone transform). ~-165cm Z matches SK_Manny's head ref height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	FVector HeadVisualOffset = FVector(0.f, 0.f, -165.f);

	/** S4 TUMBLE TIER 1: uniform scale of the sphere physics body. The engine sphere is 100cm diameter at
	 *  scale 1; 0.21 -> ~21cm diameter (~10.5cm radius) = head-sized. EditDefaultsOnly -> PIE-tunable, no rebuild. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Dismember", meta = (AllowPrivateAccess = "true"))
	float SphereBodyScale = 0.30f;
};
