// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "AFLCueNotify_LaserBeam.h"

#include "AFLLaserVisualProvider.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

AAFLCueNotify_LaserBeam::AAFLCueNotify_LaserBeam()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bAutoDestroyOnRemove = true;
}

bool AAFLCueNotify_LaserBeam::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// const_cast: FGameplayCueParameters::SourceObject is TWeakObjectPtr<const UObject>;
	// the interface thunks take non-const (engine's AddSourceObject does the same cast).
	UObject* Provider = const_cast<UObject*>(Parameters.SourceObject.Get());
	if (!Provider || !Provider->GetClass()->ImplementsInterface(UAFLLaserVisualProvider::StaticClass()))
	{
		return false;
	}
	VisualProvider = Provider;

	// Capture the firing pawn for the aim ray. The cue ACTOR's GetInstigator() is null, so
	// resolve from the cue params: the Instigator (the ability's avatar, what we set), else
	// the cue Target (MyTarget = the pawn that owns the ASC). This fixes the (0,0,0) spawn.
	FiringPawn = Cast<APawn>(Parameters.GetInstigator());
	if (!FiringPawn.IsValid())
	{
		FiringPawn = Cast<APawn>(const_cast<AActor*>(Parameters.GetEffectCauser()));
	}
	if (!FiringPawn.IsValid())
	{
		FiringPawn = Cast<APawn>(MyTarget);
	}

	UNiagaraSystem* Sys = IAFLLaserVisualProvider::Execute_GetBeamSystem(Provider);
	if (!Sys)
	{
		return false;
	}

	// Spawn UNATTACHED in world. Imported beam systems (NS_AFL_Laser_*) stretch from the
	// COMPONENT ORIGIN to User.Beam End, so the visible start IS the component's world
	// position -- which Tick drives to the aim-ray start each frame (NOT the muzzle).
	// Seed at the firing pawn's view so frame 0 isn't at world origin; Tick corrects it.
	FVector SeedLoc = FVector::ZeroVector;
	FVector SeedDir = FVector::ForwardVector;
	if (ResolveAimRay(SeedLoc, SeedDir))
	{
		SeedLoc = SeedLoc + SeedDir * BeamVisualOriginDistance;
	}

	// Seed orientation = the aim direction (so frame 0 isn't vertical); Tick refines it.
	BeamNC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), Sys, SeedLoc, SeedDir.Rotation(),
		FVector(1.0f), /*bAutoDestroy*/ false, /*bAutoActivate*/ true);

	if (BeamNC)
	{
		// Color is a CONDITIONAL runtime override. The base look lives on the NS
		// User.Color default (authored in the Niagara editor -- the rich color surface).
		// The provider returns a real color (A > 0) ONLY when a runtime/entitlement tint
		// should override it; the BlueprintNativeEvent default is zero-init (A == 0),
		// which means "leave the editor default alone." This is the customization seam:
		// editor default -> optional per-player override.
		const FLinearColor OverrideColor = IAFLLaserVisualProvider::Execute_GetBeamColor(Provider);
		if (OverrideColor.A > 0.0f)
		{
			BeamNC->SetVariableLinearColor(ColorParam, OverrideColor);
		}
		BeamNC->SetVariableVec3(BeamEndParam, SeedLoc);
	}

	SetActorTickEnabled(true);
	return true;
}

void AAFLCueNotify_LaserBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!BeamNC)
	{
		return;
	}

	// AIM RAY (camera-based) -- the same ray the authoritative trace uses, so the visible
	// beam rides the crosshair. Verbatim port of the PIE-verified UAFLAG_Laser_Beam.
	FVector ViewLocation, AimDirection;
	if (!ResolveAimRay(ViewLocation, AimDirection))
	{
		return;
	}
	const FVector EndTrace = ViewLocation + AimDirection * MaxRange;

	// Cosmetic-only trace (never feeds damage). Ignore the firing pawn.
	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(AFLLaserCosmetic), /*bTraceComplex*/ true);
	if (AActor* Inst = GetInstigator())
	{
		QP.AddIgnoredActor(Inst);
	}
	const bool bHit = GetWorld()
		? GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, EndTrace, CosmeticTraceChannel, QP)
		: false;
	const FVector ImpactPoint = bHit ? Hit.ImpactPoint : EndTrace;

	// BeamStart on the aim ray (NOT the muzzle), with the point-blank clamp -- identical
	// math to the verified ability. The imported system has no "start" param, so the
	// component's world LOCATION is the start; move it there each tick.
	FVector BeamStart = ViewLocation + AimDirection * BeamVisualOriginDistance;
	const float DistToImpact = FVector::Dist(ViewLocation, ImpactPoint);
	if (DistToImpact < BeamVisualOriginDistance)
	{
		BeamStart = ViewLocation + AimDirection * (DistToImpact * 0.5f);
	}
	// Position AND orient the component. The imported NS_AFL_Laser_* has no "Beam Start"
	// param -- it stretches from the component ORIGIN along the component's orientation to
	// User.Beam End. Setting only location left the beam at its default (vertical) rotation,
	// so it rendered as a standing spiral. Orient the component's forward axis from the
	// start toward the impact so the beam lances horizontally at the crosshair.
	const FRotator BeamRot = (ImpactPoint - BeamStart).Rotation();
	BeamNC->SetWorldLocationAndRotation(BeamStart, BeamRot);

	// The marketplace contract: drive User.Beam End to the impact (world-space).
	BeamNC->SetVariableVec3(BeamEndParam, ImpactPoint);
}

bool AAFLCueNotify_LaserBeam::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (BeamNC)
	{
		BeamNC->Deactivate();        // let ribbons fade
		BeamNC->SetAutoDestroy(true);
		BeamNC = nullptr;
	}
	SetActorTickEnabled(false);
	VisualProvider = nullptr;
	return true;
}

bool AAFLCueNotify_LaserBeam::ResolveAimRay(FVector& OutViewLocation, FVector& OutAimDirection) const
{
	// Verbatim the verified UAFLAG_Laser_Beam view source: pawn view, overridden by the
	// PlayerCameraManager surface when available (post-modifier viewpoint). The firing
	// pawn is captured from the cue params in OnActive (NOT GetInstigator(), which is
	// null on the cue actor -- that was the (0,0,0) spawn bug).
	const APawn* Pawn = FiringPawn.Get();
	if (!Pawn)
	{
		return false;
	}

	FVector  ViewLocation = Pawn->GetPawnViewLocation();
	FRotator ViewRotation = Pawn->GetViewRotation();

	if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
		{
			ViewLocation = CamMgr->GetCameraLocation();
			ViewRotation = CamMgr->GetCameraRotation();
		}
	}

	OutViewLocation = ViewLocation;
	OutAimDirection = ViewRotation.Vector().GetSafeNormal();
	return true;
}
