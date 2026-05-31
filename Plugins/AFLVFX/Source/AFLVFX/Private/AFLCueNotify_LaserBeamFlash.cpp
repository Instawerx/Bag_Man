// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "AFLCueNotify_LaserBeamFlash.h"

#include "AFLLaserVisualProvider.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

AAFLCueNotify_LaserBeamFlash::AAFLCueNotify_LaserBeamFlash()
{
	// Burst: no tick. The flash is spawned, lives FlashLifetime via a timer, then
	// deactivates + auto-destroys. bAutoDestroyOnRemove backstops cleanup if the cue
	// itself is removed before the timer fires.
	PrimaryActorTick.bCanEverTick = false;
	bAutoDestroyOnRemove = true;
}

bool AAFLCueNotify_LaserBeamFlash::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// const_cast: FGameplayCueParameters::SourceObject is TWeakObjectPtr<const UObject>;
	// the IAFLLaserVisualProvider thunks take non-const UObject* (engine's own
	// AddSourceObject does the identical cast). Symmetric, not a workaround.
	UObject* Provider = const_cast<UObject*>(Parameters.SourceObject.Get());
	if (!Provider || !Provider->GetClass()->ImplementsInterface(UAFLLaserVisualProvider::StaticClass()))
	{
		return false;
	}

	UNiagaraSystem* Sys = IAFLLaserVisualProvider::Execute_GetBeamSystem(Provider);
	USceneComponent* Mesh = IAFLLaserVisualProvider::Execute_GetMuzzleMeshComponent(Provider);
	const FName Socket = IAFLLaserVisualProvider::Execute_GetMuzzleSocketName(Provider);
	if (!Sys || !Mesh)
	{
		return false;
	}

	// Spawn the beam at the muzzle (attached so the flash origin sits on the weapon),
	// then drive Beam End to the confirmed impact. Same two-param marketplace contract
	// as the looping beam -- just discrete + short-lived.
	UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Sys, Mesh, Socket,
		FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		/*bAutoDestroy*/ false);

	if (!NC)
	{
		return false;
	}

	NC->SetVariableVec3(BeamEndParam, Parameters.Location);
	// Conditional runtime color override (see AFLCueNotify_LaserBeam): A > 0 = a real
	// entitlement tint; A == 0 (BNE default) = use the NS editor default.
	const FLinearColor OverrideColor = IAFLLaserVisualProvider::Execute_GetBeamColor(Provider);
	if (OverrideColor.A > 0.0f)
	{
		NC->SetVariableLinearColor(ColorParam, OverrideColor);
	}

	// Schedule the brief deactivate so the beam reads as a flash, not a sustained line.
	if (UWorld* World = GetWorld())
	{
		FTimerHandle Th;
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AAFLCueNotify_LaserBeamFlash::DeactivateFlash, NC);
		World->GetTimerManager().SetTimer(Th, Del, FlashLifetime, /*bLoop*/ false);
	}
	else
	{
		// No world to time against -- deactivate immediately rather than leak.
		DeactivateFlash(NC);
	}

	return true;
}

void AAFLCueNotify_LaserBeamFlash::DeactivateFlash(UNiagaraComponent* NC)
{
	if (NC)
	{
		NC->Deactivate();        // let ribbons fade out
		NC->SetAutoDestroy(true);
	}
}
