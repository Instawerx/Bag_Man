// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "AFLCueNotify_LaserBeam.h"

#include "AFLLaserVisualProvider.h"
#include "AFLBeamEndpointProvider.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Pawn.h"

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

	// The beam ENDPOINT comes from the firing pawn's published-value bridge -- the component
	// that implements IAFLBeamEndpointProvider. MyTarget is the cue target = the ASC's avatar
	// pawn (the firing pawn). We find the component by INTERFACE (no concrete-type dependency
	// on AFLCombat's UAFLBeamChannelComponent) and read its replicated impact point each Tick.
	// This is the whole point: the cue traces NOTHING; gameplay publishes, the cue plays it.
	FiringPawn = Cast<APawn>(MyTarget);
	BeamChannel = nullptr;
	if (MyTarget)
	{
		for (UActorComponent* Comp : MyTarget->GetComponents())
		{
			if (Comp && Comp->GetClass()->ImplementsInterface(UAFLBeamEndpointProvider::StaticClass()))
			{
				BeamChannel = Comp;
				break;
			}
		}
	}

	UNiagaraSystem* Sys = IAFLLaserVisualProvider::Execute_GetBeamSystem(Provider);
	if (!Sys)
	{
		return false;
	}

	// Bail rather than spawn at world origin if there's no firing pawn to anchor the start.
	// MyTarget IS the ASC's avatar pawn, so this should always resolve; the guard exists so a
	// degenerate cue (no pawn) NEVER produces a one-frame beam-to-origin flicker on any client
	// -- the exact artifact that would read as a bug in the 2-client gate.
	APawn* Pawn = FiringPawn.Get();
	if (!Pawn)
	{
		return false;
	}

	// Spawn UNATTACHED in world. With Absolute Beam End enabled on the imported systems the
	// endpoint is WORLD-space, so the beam stretches from the component origin to User."Beam
	// End" with NO component rotation needed. The component's world position is the visible
	// START; Tick anchors it to the published MUZZLE each frame. Seed: prefer the published
	// muzzle if it's already there (host -- ability published before the cue add), else the
	// view-anchored point (never origin -- guaranteed by the pawn guard above) so frame 0 is a
	// sane forward beam until the muzzle lands.
	const FVector ViewLoc   = Pawn->GetPawnViewLocation();
	const FVector ViewFwd   = Pawn->GetViewRotation().Vector().GetSafeNormal();
	FVector SeedStart = ViewLoc + ViewFwd * BeamVisualOriginDistance;
	if (UActorComponent* Ch = BeamChannel.Get())
	{
		const FVector M = IAFLBeamEndpointProvider::Execute_GetBeamMuzzleLocation(Ch);
		if (!M.IsNearlyZero())
		{
			SeedStart = M;
		}
	}

	BeamNC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), Sys, SeedStart, FRotator::ZeroRotator,
		FVector(1.0f), /*bAutoDestroy*/ false, /*bAutoActivate*/ true);

	if (BeamNC)
	{
		// Colour is a CONDITIONAL runtime override. The base look lives on the NS User.Color
		// default (the rich authored colour surface). The provider returns a real colour
		// (A > 0) ONLY when a runtime/entitlement tint should override it; the zero-init
		// default (A == 0) means "leave the editor default alone" -- the customization seam.
		const FLinearColor OverrideColor = IAFLLaserVisualProvider::Execute_GetBeamColor(Provider);
		if (OverrideColor.A > 0.0f)
		{
			BeamNC->SetVariableLinearColor(ColorParam, OverrideColor);
		}

		// Seed the endpoint from the published value if the bridge is already present (it is on
		// the host/authority -- the ability opened it before adding this cue). Else seed a sane
		// forward point well down the aim (a full cosmetic-length forward, NOT origin/zero-len)
		// so a remote client shows a real beam until replication delivers the live endpoint.
		FVector SeedEnd = ViewLoc + ViewFwd * 8000.0f;
		if (UActorComponent* Channel = BeamChannel.Get())
		{
			const FVector Published = IAFLBeamEndpointProvider::Execute_GetBeamImpactPoint(Channel);
			// Guard the published value too: a just-created component reads (0,0,0) until the
			// first PublishImpact lands. Treat near-zero as "not yet published" and keep the
			// forward seed so we never draw a beam to world origin.
			if (!Published.IsNearlyZero())
			{
				SeedEnd = Published;
			}
		}
		BeamNC->SetVariableVec3(BeamEndParam, SeedEnd);
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

	// The ONLY endpoint source: the published impact point from the gameplay ability, read
	// through the interface. No trace, no camera-derived endpoint -- the ability already did
	// the authoritative trace and handed us the world-space point. Replicated, so this reads
	// correctly on the firing client, the listen-server host, AND simulated proxies.
	UActorComponent* Channel = BeamChannel.Get();
	if (!Channel)
	{
		// On a remote client the replicated component may not have arrived yet (frame 0 of a
		// remote player's beam). Try to (re)acquire it; until then, leave the seed endpoint.
		if (APawn* Pawn = FiringPawn.Get())
		{
			for (UActorComponent* Comp : Pawn->GetComponents())
			{
				if (Comp && Comp->GetClass()->ImplementsInterface(UAFLBeamEndpointProvider::StaticClass()))
				{
					BeamChannel = Comp;
					Channel = Comp;
					break;
				}
			}
		}
		if (!Channel)
		{
			return;
		}
	}

	FVector ImpactPoint = IAFLBeamEndpointProvider::Execute_GetBeamImpactPoint(Channel);
	// Near-zero = the bridge component just created/replicated and the first PublishImpact
	// hasn't landed yet (frame 0 / pre-replication on a remote client). The forward-seed
	// fallback below keeps the beam off world origin until the real endpoint arrives.
	const bool bEndpointPublished = !ImpactPoint.IsNearlyZero();

	// Anchor the visible START at the WEAPON MUZZLE (operator precision rule: the beam emits
	// from the barrel tip, like real life), read from the published value -- NOT a synthetic
	// eye-point. The muzzle is resolved gameplay-side (Pulse's proven resolve + weapon_r
	// fallback, never origin), so worst case it's the hand, never a vanished/origin beam.
	// With Absolute Beam End the component needs no rotation -- position alone sets the start.
	const FVector PublishedMuzzle = IAFLBeamEndpointProvider::Execute_GetBeamMuzzleLocation(Channel);

	FVector BeamStart = ImpactPoint;   // fallback if the pawn went away mid-beam
	if (APawn* Pawn = FiringPawn.Get())
	{
		const FVector ViewLoc = Pawn->GetPawnViewLocation();
		const FVector ViewDir = Pawn->GetViewRotation().Vector().GetSafeNormal();

		// Endpoint not yet published: aim a full cosmetic length forward instead of drawing to
		// world origin. (The diagnostic above fires if this persists past the grace window.)
		if (!bEndpointPublished)
		{
			ImpactPoint = ViewLoc + ViewDir * 8000.0f;
		}

		// Muzzle published -> emit from the barrel. Not yet (frame-0 / pre-replication) ->
		// keep the view-anchored start so frame 0 isn't at origin; the muzzle takes over once
		// the published value lands. Point-blank clamp guards against a reversed beam.
		if (!PublishedMuzzle.IsNearlyZero())
		{
			BeamStart = PublishedMuzzle;
		}
		else
		{
			BeamStart = ViewLoc + ViewDir * BeamVisualOriginDistance;
		}

		const float DistToImpact = FVector::Dist(BeamStart, ImpactPoint);
		if (DistToImpact < BeamVisualOriginDistance)
		{
			BeamStart = ViewLoc + ViewDir * (DistToImpact * 0.5f);
		}
	}

	BeamNC->SetWorldLocation(BeamStart);
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

	// Pool-safe: the GC manager recycles this actor (Recycle() does NOT reset subclass
	// members), so null every captured pointer so a reused instance never ticks a stale one.
	BeamChannel = nullptr;
	FiringPawn = nullptr;
	VisualProvider = nullptr;
	return true;
}
