// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLAccessoryChainActor.h"

#include "AFLCombat.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Components/ChildActorComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Cosmetics/AFLAccessoryIKComponent.h"

AAFLAccessoryChainActor::AAFLAccessoryChainActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAFLAccessoryChainActor::BeginPlay()
{
	Super::BeginPlay();
	// The chain is not a wrist item, so the base's wrist correction is a no-op here; the pendant is our
	// job. Read it once on spawn -- re-equipping the chain spawns a fresh actor that runs this again.
	RefreshPendant();
}

USkeletalMeshComponent* AAFLAccessoryChainActor::FindChainMesh() const
{
	// The chain's skeletal mesh lives on the BP that derives from us. Take the first skeletal mesh
	// component -- a chain part BP carries exactly one.
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(this);
	return Meshes.Num() > 0 ? Meshes[0] : nullptr;
}

FName AAFLAccessoryChainActor::ResolvePendantId() const
{
	// this actor -> ChildActorComponent -> pawn mesh -> pawn -> PlayerState -> loadout selection.
	// GetOwner walks to the pawn because the customizer set us up under the pawn's ChildActorComponent.
	const AActor* OwnerActor = GetOwner();
	const APawn* Pawn = Cast<APawn>(OwnerActor);
	if (!Pawn)
	{
		// The ChildActorComponent's owner is the pawn; our own GetOwner may be that component's owner.
		if (const UChildActorComponent* PC = Cast<UChildActorComponent>(GetParentComponent()))
		{
			Pawn = Cast<APawn>(PC->GetOwner());
		}
	}
	const APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	const UAFLCosmeticLoadoutComponent* Loadout =
		PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
	if (!Loadout) { return NAME_None; }

	const FAFLAccessoryPlacement* P = Loadout->GetSelection().AccessorySet.Find(EAFLAccessorySlot::Pendant);
	return (P && P->IsSet()) ? P->AccessoryId : NAME_None;
}

void AAFLAccessoryChainActor::RefreshPendant()
{
	const FName WantId = ResolvePendantId();

	// Already showing the right pendant? Nothing to do -- avoids a destroy/respawn flicker on re-drives.
	if (SpawnedPendant && SpawnedPendantId == WantId) { return; }

	// Clear whatever we had. Un-equipping the pendant (WantId None) lands here and correctly leaves the
	// chain bare, while the chain itself is untouched.
	if (SpawnedPendant)
	{
		SpawnedPendant->DestroyComponent();
		SpawnedPendant = nullptr;
		SpawnedPendantId = NAME_None;
	}
	if (WantId.IsNone()) { return; }

	USkeletalMeshComponent* ChainMesh = FindChainMesh();
	if (!ChainMesh)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLChain] %s has no skeletal mesh -- cannot hang a pendant."), *GetName());
		return;
	}
	// RENDER-ONLY SUSPECT, GUARDED: a socket that does not exist would silently parent the pendant to the
	// mesh root -- the pendant would sit at the chain's origin, not on the lowest bone. Ask the mesh.
	if (!ChainMesh->DoesSocketExist(PendantSocket))
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLChain] %s: socket '%s' not on the chain mesh -- pendant NOT spawned (would hang at origin)."),
			*GetName(), *PendantSocket.ToString());
		return;
	}

	const UAFLCosmeticCatalogSubsystem* Cat = UAFLCosmeticCatalogSubsystem::Get(this);
	const FAFLCatalogEntry* Row = Cat ? Cat->FindEntry(WantId) : nullptr;
	UClass* PendantClass = Row ? Row->AccessoryPartClass.LoadSynchronous() : nullptr;
	if (!PendantClass)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLChain] pendant %s has no AccessoryPartClass -- nothing to spawn."), *WantId.ToString());
		return;
	}

	SpawnedPendant = NewObject<UChildActorComponent>(this);
	SpawnedPendant->SetupAttachment(ChainMesh, PendantSocket);
	SpawnedPendant->SetChildActorClass(PendantClass);
	SpawnedPendant->RegisterComponent();
	SpawnedPendantId = WantId;

	// ---- BRIDGE HOOK 2 of 2: publish the CHAIN as the pendant's surface --------------------------
	// The second entry point, and it cannot be folded into the first. AddCharacterPart attaches only to
	// the pawn's mesh (ULyraPawnComponent_CharacterParts::GetSceneComponentToAttachTo returns
	// Cast<ACharacter>(Owner)->GetMesh()), so the pendant is spawned HERE, onto the chain's own mesh.
	// Its surface is therefore this chain, not the body.
	//
	// AFTER RegisterComponent, deliberately: re-equipping a chain destroys and respawns this component,
	// so registering earlier would publish a mesh whose pendant does not exist yet.
	if (AActor* PawnOwner = GetOwner())
	{
		UAFLAccessoryIKComponent::RegisterSurface(PawnOwner, FName(TEXT("Neck")), ChainMesh);
	}

	UE_LOG(LogAFLCombat, Log,
		TEXT("[AFLChain] %s: pendant %s spawned on '%s' (lowest simulated bone) -- sways with the chain."),
		*GetName(), *WantId.ToString(), *PendantSocket.ToString());

	// ---- WHAT DID THE PENDANT ACTUALLY BECOME? --------------------------------------------------
	// Reported in-game as enormous and floating high and sideways, while the editor preview looks
	// correct. Every static reading says it should be ~9cm: sockets compound to 1.88/1.38/1.19 with no
	// shear (measured), the part Blueprint is scale 1.0, and the mesh is 1x4.3x5cm. So the discrepancy
	// is in the SPAWNED instance, and nothing currently prints it -- the pendant bails out of
	// ApplyIKOffset before the existing chain-walk diagnostic runs.
	//
	// Deferred by a tick: SetChildActorClass creates the child actor asynchronously, so reading the
	// transform here would report an actor that does not exist yet and log a confident zero.
	{
		TWeakObjectPtr<AAFLAccessoryChainActor> WeakSelf(this);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([WeakSelf]()
		{
			AAFLAccessoryChainActor* Self = WeakSelf.Get();
			UChildActorComponent* CAC = Self ? Self->SpawnedPendant : nullptr;
			AActor* Child = CAC ? CAC->GetChildActor() : nullptr;
			if (!Self || !Child)
			{
				UE_LOG(LogAFLCombat, Warning,
					TEXT("[AFLPENDANT] child actor never materialised (self=%s cac=%s)"),
					Self ? TEXT("ok") : TEXT("gone"), CAC ? TEXT("ok") : TEXT("null"));
				return;
			}
			FVector Origin = FVector::ZeroVector, Extent = FVector::ZeroVector;
			Child->GetActorBounds(false, Origin, Extent);
			const FVector CS = CAC->GetComponentScale();
			const FVector CL = CAC->GetComponentLocation();
			UMeshComponent* MC = Child->FindComponentByClass<UMeshComponent>();
			const FVector MS = MC ? MC->GetComponentScale() : FVector::ZeroVector;
			const FVector ML = MC ? MC->GetComponentLocation() : FVector::ZeroVector;
			// ROTATION, and the DELTA to the operator's intent.
			// The pendant takes the CHAIN socket's rotation; the operator tuned the BODY socket, which
			// the game never reads. Copying his numbers across would be wrong -- the chain socket lives
			// in the chain bone's space, which carries an axis permutation (measured earlier: one unit
			// of relative_location.y moves +Z in component space). So the DELTA is measured in world
			// space, where both are directly comparable, and applied afterwards.
			const FRotator PendW = CAC->GetComponentRotation();
			FRotator BodyW = FRotator::ZeroRotator;
			bool bHaveBody = false;
			if (const AActor* PawnOwner = Self->GetOwner())
			{
				if (const ACharacter* Ch = Cast<ACharacter>(PawnOwner))
				{
					if (const USkeletalMeshComponent* PM = Ch->GetMesh())
					{
						if (PM->DoesSocketExist(FName(TEXT("accessory_pendant"))))
						{
							BodyW = PM->GetSocketTransform(FName(TEXT("accessory_pendant")), RTS_World).Rotator();
							bHaveBody = true;
						}
					}
				}
			}
			const FQuat Delta = bHaveBody
				? (BodyW.Quaternion() * PendW.Quaternion().Inverse())
				: FQuat::Identity;

			UE_LOG(LogAFLCombat, Display,
				TEXT("[AFLPENDANT] %s on %s | CAC loc=%s scale=%s | worldBounds=%.1f x %.1f x %.1f cm | "
				     "pendantWorldRot=(P%.1f Y%.1f R%.1f) | bodySocketWorldRot=%s | DELTA=(P%.1f Y%.1f R%.1f)"),
				*GetNameSafe(Child), *Self->GetName(),
				*CL.ToCompactString(), *CS.ToCompactString(),
				Extent.X * 2, Extent.Y * 2, Extent.Z * 2,
				PendW.Pitch, PendW.Yaw, PendW.Roll,
				bHaveBody ? *FString::Printf(TEXT("(P%.1f Y%.1f R%.1f)"), BodyW.Pitch, BodyW.Yaw, BodyW.Roll)
				          : TEXT("<body socket missing>"),
				Delta.Rotator().Pitch, Delta.Rotator().Yaw, Delta.Rotator().Roll);
		}), 0.5f, false);
	}
}
