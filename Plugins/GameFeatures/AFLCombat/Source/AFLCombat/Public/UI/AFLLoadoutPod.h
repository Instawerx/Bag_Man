// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "AFLLoadoutPod.generated.h"

class UStaticMeshComponent;
class URectLightComponent;
class UCameraComponent;
class UNiagaraComponent;

/**
 * AAFLLoadoutPod -- the IRONICS preview STAGE actor (#7 pod increment; DECAPSULATED 2026-08-28).
 *
 * Operator ruling: the hero stands OUT FRONT AND CLEAR -- no enclosing capsule, halo ring, backdrop
 * slab, or platform disc (all four components deleted). What remains is the open stage: the
 * electric-blue neon key light, the dark blue->violet gradient dome behind, the electric-arc energy
 * element, a PawnAnchor marking where the posed hero stands, and a framing camera (Increment B).
 *   - Increment C (now): spawned attached to the display pawn + rendered inside the loadout's
 *     SceneCapture preview (RefreshPreviewShowList's GetAttachedActors auto-includes it).
 *   - Increment B (next): dropped into a dedicated scene, framed by FramingCamera.
 * Cosmetic-only: no collision, client-side/transient, never replicated.
 */
UCLASS()
class AFLCOMBAT_API AAFLLoadoutPod : public AActor
{
	GENERATED_BODY()

public:
	AAFLLoadoutPod();

	/** Where the posed hero stands (pod-local origin = base centre). The loadout aligns the pawn's feet here. */
	USceneComponent* GetPawnAnchor() const { return PawnAnchor; }

	/** The diorama framing camera (Increment B direct-view; unused in C, which frames via the SceneCapture). */
	UCameraComponent* GetFramingCamera() const { return FramingCamera; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<USceneComponent> PodRoot;

	/** Dark/neon theater light -- electric-blue #1E5AFF (IRONICS palette), aimed at the hero's chest. */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<URectLightComponent> NeonLight;

	/** Neon-atmosphere backdrop dome (large inward sphere, blue->violet gradient) -- the designed IRONICS
	 *  environment behind the pod, replacing flat black. */
	UPROPERTY(VisibleAnywhere, Category = "Pod|Environment")
	TObjectPtr<UStaticMeshComponent> BackdropDome;

	/** Marks the posed-hero stand point (pod-local origin, base centre). */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<USceneComponent> PawnAnchor;

	/** Diorama framing camera (Increment B). */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<UCameraComponent> FramingCamera;
};
