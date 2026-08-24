// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLAccessoryChainActor.h"

#include "AFLCombat.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

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

	UE_LOG(LogAFLCombat, Log,
		TEXT("[AFLChain] %s: pendant %s spawned on '%s' (lowest simulated bone) -- sways with the chain."),
		*GetName(), *WantId.ToString(), *PendantSocket.ToString());
}
