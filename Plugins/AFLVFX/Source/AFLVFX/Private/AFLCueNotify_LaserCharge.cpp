// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "AFLCueNotify_LaserCharge.h"

#include "AFLLaserVisualProvider.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AAFLCueNotify_LaserCharge::AAFLCueNotify_LaserCharge()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bAutoDestroyOnRemove = true;
}

bool AAFLCueNotify_LaserCharge::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	FiringPawn = Cast<APawn>(MyTarget);
	APawn* Pawn = FiringPawn.Get();
	if (!Pawn || !ChargeSystem)
	{
		return false;
	}
	ElapsedCharge = 0.0f;

	// Attach the orb to the firing pawn's mesh at the weapon grip socket (matches the WID attach socket).
	USceneComponent* AttachTo = nullptr;
	if (const ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		AttachTo = Char->GetMesh();
	}
	if (!AttachTo)
	{
		AttachTo = Pawn->GetRootComponent();
	}

	ChargeNC = UNiagaraFunctionLibrary::SpawnSystemAttached(
		ChargeSystem, AttachTo, AttachSocket, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, /*bAutoDestroy=*/ false);

	if (ChargeNC)
	{
		// Tint from the provider -- conditional override (A>0 = a real runtime tint; A==0 = keep the NS
		// authored default), the SAME customization seam the beam cue uses.
		UObject* Provider = const_cast<UObject*>(Parameters.SourceObject.Get());
		if (Provider && Provider->GetClass()->ImplementsInterface(UAFLLaserVisualProvider::StaticClass()))
		{
			VisualProvider = Provider;
			const FLinearColor OverrideColor = IAFLLaserVisualProvider::Execute_GetBeamColor(Provider);
			if (OverrideColor.A > 0.0f)
			{
				ChargeNC->SetVariableLinearColor(ColorParam, OverrideColor);
			}
		}
		ChargeNC->SetVariableFloat(ChargeParam, 0.0f);
	}

	// Local player only: ramping camera shake for the charge tension.
	if (ChargeCameraShake && Pawn->IsLocallyControlled())
	{
		if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraShake(ChargeCameraShake, 1.0f);
				bShakeStarted = true;
			}
		}
	}

	SetActorTickEnabled(true);
	return true;
}

void AAFLCueNotify_LaserCharge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!ChargeNC)
	{
		return;
	}
	// Cosmetic self-ramp: build the orb's Charge param 0..1 over ChargeRampTime. Decoupled from the
	// ability's authoritative charge timing on purpose -- this is only the visible wind-up.
	ElapsedCharge += DeltaSeconds;
	const float Norm = FMath::Clamp(ElapsedCharge / FMath::Max(0.05f, ChargeRampTime), 0.0f, 1.0f);
	ChargeNC->SetVariableFloat(ChargeParam, Norm);
}

bool AAFLCueNotify_LaserCharge::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	if (ChargeNC)
	{
		ChargeNC->Deactivate();
		ChargeNC->SetAutoDestroy(true);
		ChargeNC = nullptr;
	}
	if (bShakeStarted)
	{
		if (const APawn* Pawn = FiringPawn.Get())
		{
			if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
			{
				if (PC->PlayerCameraManager && ChargeCameraShake)
				{
					PC->PlayerCameraManager->StopAllInstancesOfCameraShake(ChargeCameraShake, /*bImmediately=*/ false);
				}
			}
		}
		bShakeStarted = false;
	}
	SetActorTickEnabled(false);

	// Pool-safe: the GC manager recycles this actor; null every captured pointer.
	FiringPawn = nullptr;
	VisualProvider = nullptr;
	return true;
}
