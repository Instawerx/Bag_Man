// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLSkinColorComponent.h"

#include "Components/ChildActorComponent.h"
#include "Cosmetics/AFLCharacterPartActor.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSkinColorComponent)

UAFLSkinColorComponent::UAFLSkinColorComponent()
{
	// No replicated base to inherit from (standalone UActorComponent) -> WE must enable replication so
	// SkinColor replicates. Do NOT omit -- without it, SkinColor never reaches clients and convergence
	// is silently zero (the "compiles but doesn't replicate" trap).
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLSkinColorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLSkinColorComponent, SkinColor);
}

void UAFLSkinColorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Reconcile: if a color + parts are BOTH already present when we begin play (late join, or color set
	// before our BeginPlay), apply now. Idempotent + null-guarded -> safe no-op otherwise.
	ReapplyColorToAllParts();
}

void UAFLSkinColorComponent::SetSkinColor(UAFLSkinColorAsset* NewColor)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SkinColor = NewColor;

		// Listen-host: the authority is also a client, but OnRep does NOT fire on the authority --
		// apply locally now so the host's own view updates immediately.
		ReapplyColorToAllParts();
	}
}

void UAFLSkinColorComponent::OnRep_SkinColor()
{
	// PATH 2 of 2 (covers COLOR-ARRIVES-SECOND): the color value replicated in. Push to already-spawned
	// parts that read null/stale at their BeginPlay. LOAD-BEARING -- without this, color-after-part desyncs.
	ReapplyColorToAllParts();
}

void UAFLSkinColorComponent::ReapplyColorToAllParts()
{
	if (SkinColor == nullptr)
	{
		return; // guard: nothing to apply yet
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Owner->GetComponents<UChildActorComponent>(ChildActorComps);
	for (UChildActorComponent* CAC : ChildActorComps)
	{
		// EDIT 1 FILTER (by-construction): only OUR body parts. Non-body child-actors (e.g. a weapon)
		// are not AAFLCharacterPartActor -> Cast returns null -> skipped. Skin never bleeds onto them.
		if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
		{
			Part->ApplySkinColor(SkinColor); // idempotent (part's owned-MID create-once)
		}
	}
}
