// Copyright C12 AI Gaming. All Rights Reserved.

#include "Beam/AFLBeamVisualComponent.h"

#include "AFLCombat.h"
#include "Beam/AFLBeamChannelComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLBeamVisualComponent)

UAFLBeamVisualComponent::UAFLBeamVisualComponent()
{
	// Tick only to feed the endpoint while the beam is live (gated inside TickComponent).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Replicated so bBeamActive (+ its OnRep) reaches simulated proxies. The OWNING actor is the
	// weapon display actor (bReplicates=true, proven 1c3cb0f9) -- this rides that proven channel.
	SetIsReplicatedByDefault(true);
}

void UAFLBeamVisualComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// To ALL clients (no COND_OwnerOnly): every machine that renders this weapon's beam needs the
	// toggle. The endpoint itself rides the pawn's AFLBeamChannelComponent (read on tick), not here.
	DOREPLIFETIME(UAFLBeamVisualComponent, bBeamActive);
}

void UAFLBeamVisualComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAFLBeamVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Tear the persistent NS down only on actual destruction (weapon unequipped/destroyed) -- NOT
	// per fire. Per-fire is a toggle (Deactivate), never a destroy.
	if (BeamNC)
	{
		BeamNC->Deactivate();
		BeamNC->DestroyComponent();
		BeamNC = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLBeamVisualComponent::SetBeamActive(bool bInActive)
{
	// AUTHORITY-ONLY. Sets the replicated value (-> OnRep on remote clients) AND applies locally
	// here on the server -- because OnRep does NOT fire on the server, and the listen-host is a
	// watched player (Edge 1). Both paths funnel into the SAME ApplyBeamActiveState.
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (bBeamActive == bInActive)
	{
		return; // idempotent -- no redundant re-activate (also avoids the stack/thrash class of bug)
	}

	bBeamActive = bInActive;
	ApplyBeamActiveState(bInActive);   // server-as-player path (OnRep won't fire here)
}

void UAFLBeamVisualComponent::OnRep_bBeamActive()
{
	// Remote-client path. Identical logic to the server path -- the SAME function (Edge 1).
	ApplyBeamActiveState(bBeamActive);
}

void UAFLBeamVisualComponent::ApplyBeamActiveState(bool bActive)
{
	// THE ONE place the visual toggles -- called from BOTH the server (SetBeamActive) and the
	// remote OnRep, so host-as-player / owning client / proxy all run identical logic.
	if (bActive)
	{
		// Spawn the persistent NS ONCE (Auto-Activate OFF), attached to the weapon actor root so it
		// follows the weapon; then Activate. Never spawn-per-fire.
		if (!BeamNC)
		{
			if (!BeamSystem)
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_BEAMVIS: no BeamSystem set on %s -- cannot show beam."),
					*GetNameSafe(GetOwner()));
				return;
			}
			USceneComponent* AttachRoot = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
			BeamNC = UNiagaraFunctionLibrary::SpawnSystemAttached(
				BeamSystem, AttachRoot, NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				/*bAutoDestroy=*/ false,
				/*bAutoActivate=*/ false);
			if (BeamNC && BeamColorOverride.A > 0.0f)
			{
				BeamNC->SetVariableLinearColor(ColorParam, BeamColorOverride);
			}
		}
		if (BeamNC)
		{
			BeamNC->Activate(/*bReset=*/ true);
			SetComponentTickEnabled(true);   // start feeding the endpoint
		}
	}
	else
	{
		if (BeamNC)
		{
			BeamNC->Deactivate();            // toggle OFF -- ribbons fade, component persists
		}
		SetComponentTickEnabled(false);
	}
}

void UAFLBeamVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!BeamNC)
	{
		return;
	}

	// Edge 2: read the endpoint + muzzle from the pawn's published-value bridge (Q1=b doctrine
	// trace). These replicate at NetUpdateFrequency, NOT per-frame -- on a proxy the endpoint may
	// step/lag behind the host's; that is EXPECTED and tunable, NOT a toggle failure.
	UAFLBeamChannelComponent* Channel = CachedChannel.Get();
	if (!Channel)
	{
		Channel = ResolveChannel();
		CachedChannel = Channel;
		if (!Channel)
		{
			return; // bridge not present yet (frame-0 / pre-replication); keep last value
		}
	}

	const FVector ImpactPoint = Channel->GetBeamImpactPoint();
	const FVector Muzzle      = Channel->GetBeamMuzzleLocation();

	// Position the NS start at the muzzle (the persistent component follows the weapon, but we set
	// world location so the beam start sits exactly at the published barrel tip). With Absolute Beam
	// End on the marketplace NS, the endpoint is world-space and needs no component rotation.
	if (!Muzzle.IsNearlyZero())
	{
		BeamNC->SetWorldLocation(Muzzle);
	}
	if (!ImpactPoint.IsNearlyZero())
	{
		BeamNC->SetVariableVec3(BeamEndParam, ImpactPoint);
	}
}

UAFLBeamChannelComponent* UAFLBeamVisualComponent::ResolveChannel() const
{
	// The bridge lives on the FIRING PAWN (the weapon actor's owner/instigator chain). The weapon
	// display actor is spawned with the pawn as owner (Lyra SpawnEquipmentActors), so walk to the
	// pawn and find the channel component.
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// Weapon actor's owner is the pawn (FLyraEquipmentActorToSpawn spawns with OwningPawn as Owner).
	APawn* OwningPawn = Cast<APawn>(Owner->GetOwner());
	if (!OwningPawn)
	{
		// Fallback: instigator.
		OwningPawn = Owner->GetInstigator();
	}
	if (!OwningPawn)
	{
		return nullptr;
	}
	return OwningPawn->FindComponentByClass<UAFLBeamChannelComponent>();
}
