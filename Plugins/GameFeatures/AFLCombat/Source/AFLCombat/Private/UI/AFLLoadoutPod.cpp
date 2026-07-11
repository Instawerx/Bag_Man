// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLLoadoutPod.h"

#include "Components/StaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLoadoutPod)

AAFLLoadoutPod::AAFLLoadoutPod()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // cosmetic-only; renders in the LOCAL player's preview capture, never replicated.

	PodRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PodRoot"));
	SetRootComponent(PodRoot);

	// Placeholder engine shapes (always cooked). Swap to the branded SM_AFL_LoadoutPod via a BP child.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// The look-at reference: roughly the posed hero's chest (pod-local).
	const FVector ChestPoint(0.f, 0.f, 95.f);

	// Podium -- a low disc the hero stands on. Engine cylinder is 100cm tall/wide, centred; scale it flat and
	// drop it so its TOP sits at pod-local Z=0 (the PawnAnchor / the hero's feet).
	PodMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PodMesh"));
	PodMesh->SetupAttachment(PodRoot);
	if (CylinderMesh.Succeeded())
	{
		PodMesh->SetStaticMesh(CylinderMesh.Object);
	}
	PodMesh->SetRelativeLocation(FVector(0.f, 0.f, -7.5f));
	PodMesh->SetRelativeScale3D(FVector(2.4f, 2.4f, 0.15f));
	PodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PodMesh->SetCastShadow(false);

	// Backdrop -- a thin vertical slab behind the hero (the placeholder "portal" plate). A radially-thin
	// cylinder reads as a standing panel from the front with no orientation risk.
	BackdropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackdropMesh"));
	BackdropMesh->SetupAttachment(PodRoot);
	if (CylinderMesh.Succeeded())
	{
		BackdropMesh->SetStaticMesh(CylinderMesh.Object);
	}
	BackdropMesh->SetRelativeLocation(FVector(-95.f, 0.f, 100.f));
	BackdropMesh->SetRelativeScale3D(FVector(2.8f, 0.12f, 2.1f));
	BackdropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackdropMesh->SetCastShadow(false);

	// Neon theater light -- electric-blue #1E5AFF, front-above, aimed at the hero's chest.
	NeonLight = CreateDefaultSubobject<URectLightComponent>(TEXT("NeonLight"));
	NeonLight->SetupAttachment(PodRoot);
	const FVector NeonPos(140.f, 0.f, 210.f);
	NeonLight->SetRelativeLocation(NeonPos);
	NeonLight->SetRelativeRotation((ChestPoint - NeonPos).Rotation());
	NeonLight->SetLightColor(FLinearColor(0.013f, 0.102f, 1.0f)); // #1E5AFF (IRONICS primary)
	NeonLight->Intensity = 1500.f;                                 // tunable in PIE / a BP child
	NeonLight->AttenuationRadius = 500.f;
	NeonLight->SourceWidth = 150.f;
	NeonLight->SourceHeight = 220.f;
	NeonLight->CastShadows = false;

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
