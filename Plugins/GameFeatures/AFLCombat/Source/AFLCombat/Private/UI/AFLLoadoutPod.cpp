// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLLoadoutPod.h"

#include "Components/StaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLoadoutPod)

AAFLLoadoutPod::AAFLLoadoutPod()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // cosmetic-only; renders in the LOCAL player's preview capture, never replicated.
	// Increment B (staged): so the game camera can SetViewTarget(this) and get the tuned FramingCamera's view --
	// AActor::CalcCamera uses the active camera component instead of the actor's default eyes viewpoint.
	bFindCameraComponentWhenViewTarget = true;

	PodRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PodRoot"));
	SetRootComponent(PodRoot);

	// DECAPSULATED (operator ruling 2026-08-28): the hero stands OUT FRONT AND CLEAR -- no kiosk
	// capsule, no halo ring, no backdrop slab, no platform disc. What remains is the open STAGE:
	// the neon key light, the dark gradient dome behind, and the electric-arc energy element.
	// (PodMesh / BackdropMesh / HaloRing / PlatformDisc components deleted with their finders.)

	// The look-at reference: roughly the posed hero's chest (pod-local).
	const FVector ChestPoint(0.f, 0.f, 95.f);

	// Neon theater light -- electric-blue #1E5AFF, front-above, aimed at the hero's chest.
	NeonLight = CreateDefaultSubobject<URectLightComponent>(TEXT("NeonLight"));
	NeonLight->SetupAttachment(PodRoot);
	const FVector NeonPos(50.f, 0.f, 235.f); // at the halo-ring (top of chamber) -> casts light DOWN on the hero
	NeonLight->SetRelativeLocation(NeonPos);
	NeonLight->SetRelativeRotation((ChestPoint - NeonPos).Rotation());
	NeonLight->SetLightColor(FLinearColor(0.013f, 0.102f, 1.0f)); // #1E5AFF (IRONICS primary)
	NeonLight->Intensity = 2500.f;                                 // brighter blue rim for the dark theater; tunable
	NeonLight->AttenuationRadius = 500.f;
	NeonLight->SourceWidth = 150.f;
	NeonLight->SourceHeight = 220.f;
	NeonLight->CastShadows = false;

	// --- NEON ENVIRONMENT (Image-2 concept) -- STRICTLY BEHIND the pod. The capture camera looks from +X
	// toward the pod at X=0, so everything here lives at X<0 (behind the hero); NOTHING renders in the
	// camera-to-pod volume. A dark neon backdrop + a single electric-arc element (no spreading cloud/bubbles). ---
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BackdropMat(TEXT("/Game/AFL/Casino/Materials/M_AFL_NeonBackdrop.M_AFL_NeonBackdrop"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ElectricFX(TEXT("/Game/LaserFX_BP/Niagara/OrbType/NS_AFL_Electric_Orb_02.NS_AFL_Electric_Orb_02"));

	// Backdrop dome -- a large inward two-sided sphere (near-black blue->violet gradient) enclosing the pod =
	// the dark neon sky. It encloses (the pod occludes its far side); it never renders in front of the pod.
	BackdropDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackdropDome"));
	BackdropDome->SetupAttachment(PodRoot);
	if (SphereMesh.Succeeded())
	{
		BackdropDome->SetStaticMesh(SphereMesh.Object);
	}
	if (BackdropMat.Succeeded())
	{
		BackdropDome->SetMaterial(0, BackdropMat.Object);
	}
	BackdropDome->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	BackdropDome->SetRelativeScale3D(FVector(18.f)); // ~9m-radius dome around the pod
	BackdropDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackdropDome->SetCastShadow(false);

	// Electric neon arcs -- the IRONICS electric charge language (AFL laser-FX). Placed FAR behind (X=-800,
	// well past the pod at X=0) so any particle spread stays STRICTLY behind the hero -- never in the
	// camera(+X)-to-pod volume (fixes the "VFX in front" regression). Single element; no cloud/bubbles.
	LightningFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LightningFX"));
	LightningFX->SetupAttachment(PodRoot);
	if (ElectricFX.Succeeded())
	{
		LightningFX->SetAsset(ElectricFX.Object);
	}
	LightningFX->SetRelativeLocation(FVector(-800.f, 0.f, 120.f));
	LightningFX->SetRelativeScale3D(FVector(1.5f));
	LightningFX->SetCastShadow(false);

	// Where the posed hero stands (pod-local origin = base centre).
	PawnAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("PawnAnchor"));
	PawnAnchor->SetupAttachment(PodRoot);

	// Diorama framing camera (Increment B direct-view; front-3/4, chest-height look-at). Inert in C.
	FramingCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FramingCamera"));
	FramingCamera->SetupAttachment(PodRoot);
	FramingCamera->bAutoActivate = false; // never steals the view; it is a marker for Increment B.
	const FVector CamPos(300.f, 120.f, 145.f);
	FramingCamera->SetRelativeLocation(CamPos);
	FramingCamera->SetRelativeRotation((ChestPoint - CamPos).Rotation());
	FramingCamera->SetFieldOfView(38.f);
}

