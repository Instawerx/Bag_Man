// Copyright C12 AI Gaming. All Rights Reserved.

#include "Beam/AFLBeamVisualComponent.h"

#include "AFLCombat.h"
#include "AFLLaserVisualProvider.h"
#include "AFLLaserVisualStatics.h"
#include "Beam/AFLBeamChannelComponent.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
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
		}
		if (BeamNC)
		{
			// UNIFIED FX tint -- re-read on EVERY activation, not just the first spawn. The NS persists
			// (spawned once, toggled), so a once-on-spawn read would FREEZE the colour at its first value;
			// a runtime tint change (the cosmetic resolver writing LaserTintColor) must be picked up on the
			// next fire. ResolveProviderTint reflection-reads LaserTintColor (the unified input the pulse
			// cues also drive); BeamColorOverride is the DEPRECATED fallback when LaserTintColor is unset.
			FLinearColor Tint = ResolveProviderTint();
			if (Tint.A <= 0.0f)
			{
				Tint = BeamColorOverride;
			}
			// Beam 0 takes ConvergeFanColors[0] when authored, else the unified tint (unchanged default).
			const FLinearColor PrimaryTint = ResolveFanBeamColor(0, Tint);
			if (PrimaryTint.A > 0.0f)
			{
				BeamNC->SetVariableLinearColor(ColorParam, PrimaryTint);
			}

			BeamNC->Activate(/*bReset=*/ true);

			// CONVERGING FAN (cosmetic): bring up ConvergeFanCount-1 EXTRA persistent beams alongside the
			// primary. Same spawn/toggle contract -- never spawned per fire, never destroyed per fire. At the
			// default count of 1 this loop does nothing, so the pre-fan path is untouched.
			const int32 ExtraWanted = FMath::Max(0, ConvergeFanCount - 1);
			while (ExtraBeamNCs.Num() < ExtraWanted)
			{
				USceneComponent* FanRoot = GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
				UNiagaraComponent* Extra = UNiagaraFunctionLibrary::SpawnSystemAttached(
					BeamSystem, FanRoot, NAME_None,
					FVector::ZeroVector, FRotator::ZeroRotator,
					EAttachLocation::KeepRelativeOffset,
					/*bAutoDestroy=*/ false,
					/*bAutoActivate=*/ false);
				if (!Extra)
				{
					break;
				}
				ExtraBeamNCs.Add(Extra);
			}
			for (int32 i = 0; i < ExtraBeamNCs.Num(); ++i)
			{
				UNiagaraComponent* Extra = ExtraBeamNCs[i];
				if (!Extra) continue;
				if (i < ExtraWanted)
				{
					// Extra i is fan beam i+1 (beam 0 is the primary BeamNC).
					const FLinearColor ExtraTint = ResolveFanBeamColor(i + 1, Tint);
					if (ExtraTint.A > 0.0f) { Extra->SetVariableLinearColor(ColorParam, ExtraTint); }
					Extra->Activate(/*bReset=*/ true);
				}
				else
				{
					Extra->Deactivate();     // count was lowered at runtime -- park the surplus
				}
			}

			SetComponentTickEnabled(true);   // start feeding the endpoint
		}
	}
	else
	{
		if (BeamNC)
		{
			BeamNC->Deactivate();            // toggle OFF -- ribbons fade, component persists
		}
		for (UNiagaraComponent* Extra : ExtraBeamNCs)
		{
			if (Extra) { Extra->Deactivate(); }
		}
		SetComponentTickEnabled(false);
	}
}

void UAFLBeamVisualComponent::ComputeFanOffsets(const FVector& Axis, TArray<FVector>& OutOffsets) const
{
	// Index 0 is ALWAYS the zero offset, so the primary beam sits exactly where it did pre-fan.
	OutOffsets.Reset();
	const int32 N = FMath::Clamp(ConvergeFanCount, 1, 12);
	OutOffsets.Add(FVector::ZeroVector);
	if (N <= 1 || ConvergeFanRadius <= KINDA_SMALL_NUMBER || Axis.IsNearlyZero())
	{
		return;
	}

	// Build a stable basis perpendicular to the beam. UpVector degenerates when firing straight up or
	// down, so fall back to the forward axis for the cross product in that case.
	FVector Right = FVector::CrossProduct(Axis, FVector::UpVector);
	if (Right.IsNearlyZero())
	{
		Right = FVector::CrossProduct(Axis, FVector::ForwardVector);
	}
	Right = Right.GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Right, Axis).GetSafeNormal();

	// TWIST: a time-based phase spins the ORIGINS around the beam axis while every beam keeps the same
	// endpoint -- so the beams sweep around one another and read as a helix converging on the target.
	// 0 = static fan (default). Uses world time, so every machine derives the same phase from the same
	// clock and no extra replication is needed for a cosmetic.
	float Phase = 0.0f;
	if (!FMath::IsNearlyZero(ConvergeFanTwistRate))
	{
		if (const UWorld* World = GetWorld())
		{
			Phase = FMath::DegreesToRadians(ConvergeFanTwistRate * World->GetTimeSeconds());
		}
	}

	// RING: N evenly spaced around the axis. N==2 falls out of this as the symmetric PAIR (theta 0 and
	// PI give exactly +/-Right), so twin-prong and starburst share one code path and both can twist.
	for (int32 i = 0; i < N; ++i)
	{
		const float Theta = Phase + (2.0f * PI * static_cast<float>(i)) / static_cast<float>(N);
		const FVector Off = (Right * FMath::Cos(Theta) + Up * FMath::Sin(Theta)) * ConvergeFanRadius;
		if (i == 0) { OutOffsets[0] = Off; } else { OutOffsets.Add(Off); }
	}
}

FLinearColor UAFLBeamVisualComponent::ResolveFanBeamColor(int32 FanIndex, const FLinearColor& Fallback) const
{
	// Per-beam override when authored; otherwise the weapon's unified tint. A short list is legal --
	// indices past the end fall back, so a 2-colour list on a 6-beam fan tints two and leaves four.
	if (ConvergeFanColors.IsValidIndex(FanIndex) && ConvergeFanColors[FanIndex].A > 0.0f)
	{
		return ConvergeFanColors[FanIndex];
	}
	return Fallback;
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

	// BLOCK 22: read OUR slot, not the pawn's single set. Before this, both cannons' visual components
	// read the same published pair, so the one drawn beam's origin flipped between the two maws every
	// tick as each ability published -- the "alternating fire" symptom. Slot 0 for every weapon that
	// isn't the left cannon, so this is byte-identical for all 60 shipped weapons.
	const int32   Slot        = ResolveBeamSlot();
	const FVector ImpactPoint = Channel->GetBeamImpactPoint(Slot);
	const FVector Muzzle      = Channel->GetBeamMuzzleLocation(Slot);

	// Position the NS start at the muzzle (the persistent component follows the weapon, but we set
	// world location so the beam start sits exactly at the published barrel tip). With Absolute Beam
	// End on the marketplace NS, the endpoint is world-space and needs no component rotation.
	// CONVERGING FAN: every beam ends at the SAME confirmed impact point -- only the ORIGIN differs, so
	// they visually converge on the crosshair target. ⚠ This is Niagara only: the ability ran ONE trace
	// and applied ONE beam's damage, and nothing here touches either. At ConvergeFanCount == 1 the
	// offset list is a single zero vector and this is byte-identical to the pre-fan drive.
	TArray<FVector> Offsets;
	ComputeFanOffsets((ImpactPoint - Muzzle).GetSafeNormal(), Offsets);

	if (!Muzzle.IsNearlyZero())
	{
		BeamNC->SetWorldLocation(Muzzle + Offsets[0]);
	}
	if (!ImpactPoint.IsNearlyZero())
	{
		BeamNC->SetVariableVec3(BeamEndParam, ImpactPoint);
	}

	for (int32 i = 0; i < ExtraBeamNCs.Num(); ++i)
	{
		UNiagaraComponent* Extra = ExtraBeamNCs[i];
		if (!Extra || !Extra->IsActive())
		{
			continue;
		}
		const FVector Off = Offsets.IsValidIndex(i + 1) ? Offsets[i + 1] : FVector::ZeroVector;
		if (!Muzzle.IsNearlyZero())
		{
			Extra->SetWorldLocation(Muzzle + Off);
		}
		if (!ImpactPoint.IsNearlyZero())
		{
			Extra->SetVariableVec3(BeamEndParam, ImpactPoint);   // SHARED endpoint == the convergence
		}
	}
}

int32 UAFLBeamVisualComponent::ResolveBeamSlot()
{
	if (CachedBeamSlot != INDEX_NONE)
	{
		return CachedBeamSlot;
	}

	const AActor* DisplayActor = GetOwner();

	// Do NOT latch while the owner is still unattached -- its socket reads NAME_None on the first
	// frames (and on a proxy until attachment replicates), and latching then would pin the LEFT cannon
	// to slot 0 permanently. Stay unresolved and answer 0 until a real socket appears.
	if (!DisplayActor || DisplayActor->GetAttachParentSocketName().IsNone())
	{
		return 0;
	}

	// Same function the publishing ability calls -- one rule, so reader and writer cannot disagree.
	CachedBeamSlot = UAFLBeamChannelComponent::ResolveBeamSlotForActor(DisplayActor);
	return CachedBeamSlot;
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

FLinearColor UAFLBeamVisualComponent::ResolveProviderTint() const
{
	// The weapon DISPLAY actor (our owner) is spawned by a ULyraEquipmentInstance with the firing pawn
	// as Owner (Lyra SpawnEquipmentActors). Walk owner-actor -> pawn -> equipment manager, find the
	// instance whose spawned actors include us, and read its IAFLLaserVisualProvider::GetBeamColor --
	// the SAME provider the FX cues read (mirrors AFLAG_Laser_Base::ResolveLaserVisualProvider). The
	// colour is editor-authored data, identical on every machine, so resolving locally needs no replication.
	AActor* DisplayActor = GetOwner();
	if (!DisplayActor)
	{
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	APawn* OwningPawn = Cast<APawn>(DisplayActor->GetOwner());
	if (!OwningPawn)
	{
		OwningPawn = DisplayActor->GetInstigator();
	}
	if (!OwningPawn)
	{
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	ULyraEquipmentManagerComponent* EquipMgr = OwningPawn->FindComponentByClass<ULyraEquipmentManagerComponent>();
	if (!EquipMgr)
	{
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	// The instance that spawned this display actor IS the laser-visual provider (the BP weapon instance
	// implements IAFLLaserVisualProvider directly). Match by spawned-actor containment.
	const TArray<ULyraEquipmentInstance*> Instances =
		EquipMgr->GetEquipmentInstancesOfType(ULyraEquipmentInstance::StaticClass());
	for (ULyraEquipmentInstance* Instance : Instances)
	{
		if (Instance
			&& Instance->GetSpawnedActors().Contains(DisplayActor)
			&& Instance->GetClass()->ImplementsInterface(UAFLLaserVisualProvider::StaticClass()))
		{
			// Reflection read (dispatch-proof) -- replaces Execute_GetBeamColor, whose bridge-wired BP
			// override returns the C++ default at runtime. A<=0 sentinel keeps the proven green: the
			// A<=0 fallback in ApplyBeamActiveState still applies BeamColorOverride exactly as today.
			return UAFLLaserVisualStatics::ReadLaserTint(Instance);
		}
	}

	// No matching provider -> A<=0 sentinel -> caller keeps the BeamColorOverride default.
	return FLinearColor(0.f, 0.f, 0.f, 0.f);
}
