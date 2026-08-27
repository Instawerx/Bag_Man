// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Cosmetics/AFLSurfaceProviderInterface.h"
#include "AFLAccessoryIKComponent.generated.h"

class ACharacter;
class USkeletalMeshComponent;

/**
 * Per-slot offset from the authored socket to the VISIBLE surface, in world space.
 *
 * Zero means "already on the surface", NOT "no answer". A probe that fails to find the shell reports
 * bValid=false for that slot and leaves the offset at zero, because a silently-zero offset and a
 * measured-zero offset are the state pair this programme has been burned by repeatedly -- an
 * unpopulated field once read as a rendering defect that did not exist.
 */
USTRUCT(BlueprintType)
struct FAFLAccessoryIKTargets
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") FVector LeftWristOffset  = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") FVector RightWristOffset = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") FVector NeckOffset       = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") FVector PendantOffset    = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") bool bLeftWristValid  = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") bool bRightWristValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") bool bNeckValid       = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK") bool bPendantValid    = false;
};

/**
 * DYNAMIC ACCESSORY FIT, because the body is modular and a constant cannot fit a variable.
 *
 * The sockets live on the shared SK_Mannequin; the silhouette is whatever body part is equipped. A
 * hardcoded offset or scale is therefore correct for exactly one body -- demonstrated: pieces either
 * sat inside the armour shell or floated clear of the arm, with nothing in between.
 *
 * THIS COMPONENT IS ALSO THE SURFACE REGISTRY, and that is a deliberate departure from putting the
 * interface on the pawn. The pawn classes are Blueprints (B_Hero_BagMan_Pro_C and every other body in
 * a roster built to swap them), so implementing a C++ UINTERFACE on "the pawn" means editing the
 * implemented-interfaces list of every shipped character BP and re-doing it for each new one. This
 * component is added by the experience through AddComponents, exactly as AFLAccessoryPartComponent
 * is, so it is present on every pawn in every experience with no content edit at all. The interface
 * contract is unchanged; only the implementer moved.
 *
 * Consumers are two, because the pieces are not one kind of thing (measured, all ten):
 *   SKELETAL  2 chains (5 bones), 2 bracelets (4 bones) -> AnimGraph Transform (Modify) Bone
 *   STATIC    2 watches, 4 pendants (no skeleton at all) -> mesh component SetRelativeLocation
 */
UCLASS(ClassGroup = (AFL), meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLAccessoryIKComponent : public UActorComponent, public IAFLSurfaceProviderInterface
{
	GENERATED_BODY()

public:
	UAFLAccessoryIKComponent();

	/**
	 * How far outside the socket to begin the inward probe.
	 *
	 * MEASURED BURIAL DEPTHS, against CharacterMesh0 with the capsule excluded: neck 11.22 cm,
	 * pendant 10.38 cm, wrists 3.00 / 3.85 cm. The probe must START OUTSIDE the shell or it begins
	 * inside the geometry it is trying to find. Raise this for heavier silhouettes; the failure mode
	 * is a probe that reports INVALID rather than a wrong answer, which is the safe direction.
	 */
	UPROPERTY(EditAnywhere, Category = "AFL|AccessoryIK")
	float TraceStartDistance = 45.0f;

	/**
	 * How many skeleton hops from the socket's OWN bone a physics body may be and still count as
	 * "this limb" for the surface search.
	 *
	 * 2 covers the real cases measured on SK_Mannequin: accessory_wrist_l sits on hand_l and the
	 * forearm body is lowerarm_l (1 hop); accessory_neck sits on neck_01 with the body on spine_05
	 * (2 hops). The thigh -- which the UNCONSTRAINED query was actually returning for the wrist, at
	 * 61cm -- is nowhere near that budget.
	 *
	 * Raising this re-admits distant bodies and reintroduces exactly the defect this bounds. The
	 * failure direction if it is too LOW is an INVALID probe (piece stays at its authored pose), which
	 * is the recoverable one.
	 */
	UPROPERTY(EditAnywhere, Category = "AFL|AccessoryIK")
	int32 MaxBoneHops = 2;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|AccessoryIK")
	FAFLAccessoryIKTargets CurrentOffsets;

	UFUNCTION(BlueprintCallable, Category = "AFL|AccessoryIK")
	void EvaluateAccessoryOffsets(ACharacter* OwnerChar);

	/** Offset for one accessory SOCKET, or zero with bOutValid=false if the shell was not reached. */
	UFUNCTION(BlueprintCallable, Category = "AFL|AccessoryIK")
	FVector GetOffsetForSocket(FName SocketName, bool& bOutValid) const;

	/** Re-probe using the owning pawn; safe to call from equip events. */
	UFUNCTION(BlueprintCallable, Category = "AFL|AccessoryIK")
	void RefreshFromOwner();

	// --- IAFLSurfaceProviderInterface -----------------------------------------------------------
	virtual USkeletalMeshComponent* GetVisibleMeshForSlot_Implementation(FName SlotName) override;
	virtual void RegisterMeshForSlot_Implementation(FName SlotName, USkeletalMeshComponent* VisibleMesh) override;

	/** Convenience for the spawn-side bridge: find this component on a pawn and register in one call. */
	UFUNCTION(BlueprintCallable, Category = "AFL|AccessoryIK")
	static void RegisterSurface(AActor* PawnOwner, FName SlotName, USkeletalMeshComponent* VisibleMesh);

private:
	/** Slot -> visible mesh. Weak, because a re-equip destroys the part that published it. */
	UPROPERTY()
	TMap<FName, TWeakObjectPtr<USkeletalMeshComponent>> SlotMeshMap;

	/**
	 * Probe from outside the shell back to the socket and return the surface delta.
	 *
	 * OCCLUSION-SAFE, and every clause is a defect that was measured rather than imagined:
	 *   - the pawn's own mesh is IGNORED (SKM_Manny_Invis: collision, no draw)
	 *   - the capsule is IGNORED (neck/pendant probes stopped on CollisionCylinder)
	 *   - the hit must be ON SearchSurfaceMesh, or it is the wrong surface entirely
	 *   - complex collision, because a physics-asset approximation is not the silhouette an eye judges
	 */
	FVector ProbeSurface(const USkeletalMeshComponent* SocketSourceMesh,
	                     const USkeletalMeshComponent* SearchSurfaceMesh,
	                     FName SocketName,
	                     bool& bOutValid) const;
};
