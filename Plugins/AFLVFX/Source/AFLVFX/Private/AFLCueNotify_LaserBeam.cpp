// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "AFLCueNotify_LaserBeam.h"

#include "AFLLaserVisualProvider.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Pawn.h"

AAFLCueNotify_LaserBeam::AAFLCueNotify_LaserBeam()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bAutoDestroyOnRemove = true;
}

bool AAFLCueNotify_LaserBeam::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	UObject* Provider = const_cast<UObject*>(Parameters.SourceObject.Get());
	if (!Provider || !Provider->GetClass()->ImplementsInterface(UAFLLaserVisualProvider::StaticClass()))
	{
		return false;
	}
	VisualProvider = Provider;

	UNiagaraSystem* Sys = IAFLLaserVisualProvider::Execute_GetBeamSystem(Provider);
	USceneComponent* Mesh = IAFLLaserVisualProvider::Execute_GetMuzzleMeshComponent(Provider);
	const FName Socket = IAFLLaserVisualProvider::Execute_GetMuzzleSocketName(Provider);
	if (!Sys || !Mesh)
	{
		return false;
	}

	BeamNC = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Sys, Mesh, Socket,
		FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		/*bAutoDestroy*/ false);

	if (BeamNC)
	{
		BeamNC->SetVariableLinearColor(ColorParam, IAFLLaserVisualProvider::Execute_GetBeamColor(Provider));
	}

	SetActorTickEnabled(true);
	return true;
}

void AAFLCueNotify_LaserBeam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UObject* Provider = VisualProvider.Get();
	if (!BeamNC || !Provider)
	{
		return;
	}

	USceneComponent* Mesh = BeamNC->GetAttachParent();
	if (!Mesh)
	{
		return;
	}

	const FName Socket = IAFLLaserVisualProvider::Execute_GetMuzzleSocketName(Provider);
	const float Range = IAFLLaserVisualProvider::Execute_GetCosmeticRange(Provider);

	const FVector Start = Mesh->GetSocketLocation(Socket);
	const FVector Dir = Mesh->GetSocketRotation(Socket).Vector();
	const FVector End = Start + Dir * Range;

	FHitResult Hit;
	FCollisionQueryParams QP(SCENE_QUERY_STAT(AFLLaserCosmetic), /*bTraceComplex*/ true);
	if (AActor* Inst = GetInstigator())
	{
		QP.AddIgnoredActor(Inst);
	}

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, CosmeticTraceChannel, QP);

	// The whole marketplace integration contract: drive User.Beam End.
	BeamNC->SetVariableVec3(BeamEndParam, bHit ? Hit.ImpactPoint : End);
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
