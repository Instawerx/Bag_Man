// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "AFLLoadoutPod.generated.h"

class UStaticMeshComponent;
class URectLightComponent;
class UCameraComponent;
class UNiagaraComponent;

/**
 * AAFLLoadoutPod -- the reusable IRONICS loadout/armory KIOSK-POD diorama actor (#7 pod increment).
 *
 * A SELF-CONTAINED, mesh-swappable pod: a podium the hero stands on, a backdrop panel, an electric-blue
 * neon rect-light, a PawnAnchor marking where the posed hero stands, and a framing camera. Built ONCE to
 * serve every context of the loadout-pod plan (operator ruled C -> B -> in-game, one pod actor for all):
 *   - Increment C (now): spawned attached to the local player's pawn + rendered INSIDE the loadout's
 *     SceneCapture preview (the pod attaches to the pawn, so RefreshPreviewShowList's GetAttachedActors
 *     auto-includes it in the isolated capture). Zero front-end / launch-menu risk.
 *   - Increment B (next): dropped into a dedicated B_AFL_LoadoutExperience scene, framed by FramingCamera.
 *   - In-game (later): RT-rendered kiosk.
 *
 * The placeholder engine-shape meshes (/Engine/BasicShapes) swap to the branded SM_AFL_LoadoutPod by
 * overriding PodMesh/BackdropMesh in a BP child -- the actor is built to ACCEPT the swap, not hardcode it.
 * Cosmetic-only: all mesh collision is disabled so the pod never pushes the posed pawn, and it is spawned
 * client-side/transient (never replicated) so it renders only in the local player's preview.
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

	/** Set the glowing platform disc's pod-local Z (its TOP is ~Z+2cm) -- the loadout drives this from the
	 *  grounding (afl.Loadout.PodGroundZ) so the disc stays glued under the hero's risen feet. */
	void SetPlatformZ(float Z);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<USceneComponent> PodRoot;

	/** Podium the hero stands on (placeholder cylinder; swap to the branded SM_AFL_LoadoutPod base). */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<UStaticMeshComponent> PodMesh;

	/** Backdrop slab behind the hero (placeholder; the "portal" plate). */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<UStaticMeshComponent> BackdropMesh;

	/** Glowing platform disc under the hero's feet -- the lit halo the hero stands on (unlit emissive #1E5AFF). */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<UStaticMeshComponent> PlatformDisc;

	/** Glowing halo-RING in the headroom above the hero's head -- the concept's top-of-chamber signature glow
	 *  (unlit emissive #1E5AFF). Lives in the roomy pod's 57.6cm headroom. */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<UStaticMeshComponent> HaloRing;

	/** Dark/neon theater light -- electric-blue #1E5AFF (IRONICS palette), aimed at the hero's chest. */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<URectLightComponent> NeonLight;

	/** Neon-atmosphere backdrop dome (large inward sphere, blue->violet gradient) -- the designed IRONICS
	 *  environment behind the pod, replacing flat black. */
	UPROPERTY(VisibleAnywhere, Category = "Pod|Environment")
	TObjectPtr<UStaticMeshComponent> BackdropDome;

	/** Electric neon arcs (AFL laser-FX Niagara) crackling STRICTLY behind the pod on the dark backdrop. */
	UPROPERTY(VisibleAnywhere, Category = "Pod|Environment")
	TObjectPtr<UNiagaraComponent> LightningFX;

	/** Marks the posed-hero stand point (pod-local origin, base centre). */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<USceneComponent> PawnAnchor;

	/** Diorama framing camera (Increment B). */
	UPROPERTY(VisibleAnywhere, Category = "Pod")
	TObjectPtr<UCameraComponent> FramingCamera;
};
