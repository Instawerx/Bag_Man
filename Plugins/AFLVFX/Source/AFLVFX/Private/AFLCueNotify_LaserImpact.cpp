// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "AFLCueNotify_LaserImpact.h"

#include "AFLLaserVisualProvider.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AAFLCueNotify_LaserImpact::AAFLCueNotify_LaserImpact()
{
	// Burst: no tick. Auto-destroy after the one-shot spark finishes so we never
	// leave a stuck component (the spark system's own lifetime governs the visual).
	PrimaryActorTick.bCanEverTick = false;
	bAutoDestroyOnRemove = true;
}

bool AAFLCueNotify_LaserImpact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// const_cast: FGameplayCueParameters::SourceObject is TWeakObjectPtr<const UObject>,
	// and the IAFLLaserVisualProvider thunks take a non-const UObject*. The engine's own
	// AddSourceObject stores via the identical cast, so casting back on read is symmetric.
	UObject* Provider = const_cast<UObject*>(Parameters.SourceObject.Get());

	// Resolve the spark system: weapon's MuzzleSystem slot (the impact/spark look) if the
	// SourceObject is a provider, else the editor-set fallback. No provider + no fallback
	// = nothing to play (fail soft, no crash).
	UNiagaraSystem* Sys = DefaultImpactSystem;
	if (Provider && Provider->GetClass()->ImplementsInterface(UAFLLaserVisualProvider::StaticClass()))
	{
		if (UNiagaraSystem* FromWeapon = IAFLLaserVisualProvider::Execute_GetMuzzleSystem(Provider))
		{
			Sys = FromWeapon;
		}
	}
	if (!Sys)
	{
		return false;
	}

	// Spark at the confirmed hit point, oriented to the surface normal (Params.Normal).
	const FVector Loc = Parameters.Location;
	const FRotator Rot = Parameters.Normal.IsNearlyZero()
		? FRotator::ZeroRotator
		: Parameters.Normal.Rotation();

	UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), Sys, Loc, Rot,
		FVector(1.0f), /*bAutoDestroy*/ true, /*bAutoActivate*/ true);

	if (NC && Provider && Provider->GetClass()->ImplementsInterface(UAFLLaserVisualProvider::StaticClass()))
	{
		NC->SetVariableLinearColor(ColorParam, IAFLLaserVisualProvider::Execute_GetBeamColor(Provider));
	}

	return true;
}
