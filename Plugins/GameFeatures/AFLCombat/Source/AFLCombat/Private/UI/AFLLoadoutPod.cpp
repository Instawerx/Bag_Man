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

	PodRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PodRoot"));
	SetRootComponent(PodRoot);

	// Meshes: the BRANDED IRONICS kiosk (imported SM_AFL_LoadoutPod, 2-slot neon-glass) is the DEFAULT; the
	// engine cylinder is the placeholder fallback if the branded mesh is absent. PodClass (on the WBP) can
	// still override the whole actor with a BP child for future pod variants.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BrandedPod(TEXT("/Game/AFL/Casino/Meshes/Capsules/SM_AFL_LoadoutPod.SM_AFL_LoadoutPod"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const bool bBranded = BrandedPod.Succeeded();

	// The look-at reference: roughly the posed hero's chest (pod-local).
	const FVector ChestPoint(0.f, 0.f, 95.f);

	// Pod body. BRANDED: the ~2.7m ROOMY neon-glass capsule (BASE-centre pivot). Drop it 32.4cm so the 32.4cm
	// base platform-TOP lands at the PawnAnchor (pod origin = the hero's feet) -> the hero stands ON the
	// platform, head ~180 with 57.6cm headroom above (the halo-ring space); visible through the translucent
	// shell. PLACEHOLDER: a low podium disc (flat cylinder dropped so its top sits at Z=0, the feet).
	PodMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PodMesh"));
	PodMesh->SetupAttachment(PodRoot);
	if (bBranded)
	{
		PodMesh->SetStaticMesh(BrandedPod.Object);
		PodMesh->SetRelativeLocation(FVector(0.f, 0.f, -32.4f));
		PodMesh->SetRelativeScale3D(FVector::OneVector);
	}
	else if (CylinderMesh.Succeeded())
	{
		PodMesh->SetStaticMesh(CylinderMesh.Object);
		PodMesh->SetRelativeLocation(FVector(0.f, 0.f, -7.5f));
		PodMesh->SetRelativeScale3D(FVector(2.4f, 2.4f, 0.15f));
	}
	PodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PodMesh->SetCastShadow(false);

	// Backdrop -- the placeholder "portal" slab behind the hero. The branded kiosk carries its own shell, so
	// this is HIDDEN when branded (kept for the AFL_Avatar_Portal drop later).
	BackdropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackdropMesh"));
	BackdropMesh->SetupAttachment(PodRoot);
	if (CylinderMesh.Succeeded())
	{
		BackdropMesh->SetStaticMesh(CylinderMesh.Object);
	}
	BackdropMesh->SetRelativeLocation(FVector(-95.f, 0.f, 100.f));
	BackdropMesh->SetRelativeScale3D(FVector(2.8f, 0.12f, 2.1f));
	BackdropMesh->SetVisibility(!bBranded); // branded kiosk carries its own shell -> hide the placeholder slab
	BackdropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackdropMesh->SetCastShadow(false);

	// Glowing platform disc under the hero's feet -- the lit halo the hero stands on (Image-2 concept). Unlit
	// emissive #1E5AFF (reads TRUE in the dark theater); a flat cylinder at the capsule base.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlatformMat(TEXT("/Game/AFL/Casino/Materials/MI_AFL_NeonPlatform.MI_AFL_NeonPlatform"));
	PlatformDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformDisc"));
	PlatformDisc->SetupAttachment(PodRoot);
	if (CylinderMesh.Succeeded())
	{
		PlatformDisc->SetStaticMesh(CylinderMesh.Object);
	}
	PlatformDisc->SetRelativeLocation(FVector(0.f, 0.f, 3.f));
	PlatformDisc->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.04f)); // ~150cm halo disc, 4cm thin
	if (PlatformMat.Succeeded())
	{
		PlatformDisc->SetMaterial(0, PlatformMat.Object);
	}
	PlatformDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlatformDisc->SetCastShadow(false);

	// Halo-RING -- the concept's top-of-chamber signature glow, in the roomy pod's 57.6cm headroom above the
	// hero's head (~pod-local Z 210; head ~180). A flat emissive ring reusing the unlit neon platform material.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RingMesh(TEXT("/Game/Effects/Meshes/ring.ring"));
	HaloRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HaloRing"));
	HaloRing->SetupAttachment(PodRoot);
	if (RingMesh.Succeeded())
	{
		HaloRing->SetStaticMesh(RingMesh.Object);
	}
	if (PlatformMat.Succeeded())
	{
		HaloRing->SetMaterial(0, PlatformMat.Object);
	}
	// The ring mesh is a flat 20cm annulus in the YZ plane (normal +X) -> it faces the +X capture camera as-is
	// (no rotation), scaled to a ~120cm halo. Emissive/unlit so it reads front-on.
	HaloRing->SetRelativeLocation(FVector(0.f, 0.f, 210.f));
	HaloRing->SetRelativeScale3D(FVector(6.0f, 6.0f, 6.0f));
	HaloRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HaloRing->SetCastShadow(false);

	// Neon theater light -- electric-blue #1E5AFF, front-above, aimed at the hero's chest.
	NeonLight = CreateDefaultSubobject<URectLightComponent>(TEXT("NeonLight"));
	NeonLight->SetupAttachment(PodRoot);
	const FVector NeonPos(140.f, 0.f, 210.f);
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
