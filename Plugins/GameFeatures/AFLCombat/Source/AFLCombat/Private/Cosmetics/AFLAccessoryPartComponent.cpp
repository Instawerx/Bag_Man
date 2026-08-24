// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLAccessoryPartComponent.h"

#include "AFLCombat.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

void UAFLAccessoryPartComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AController* C = GetController<AController>())
	{
		C->OnPossessedPawnChanged.AddDynamic(this, &UAFLAccessoryPartComponent::HandlePossessedPawnChanged);

		// CATCH A PAWN THAT IS ALREADY POSSESSED. Component BeginPlay can land after possession, and the
		// delegate only fires on a CHANGE -- the body sibling carries the same catch for the same reason.
		if (APawn* Already = C->GetPawn())
		{
			RefreshAccessoriesForPawn(Already);
		}
	}
}

void UAFLAccessoryPartComponent::HandlePossessedPawnChanged(APawn* /*OldPawn*/, APawn* NewPawn)
{
	// A fresh pawn starts with no parts, so the record of what we added no longer describes anything.
	// Clearing it here prevents a remove against a list that never had them.
	AddedParts.Reset();
	if (NewPawn)
	{
		RefreshAccessoriesForPawn(NewPawn);
	}
}

UActorComponent* UAFLAccessoryPartComponent::FindStockPartsComponent() const
{
	const AController* C = GetController<AController>();
	if (!C) { return nullptr; }

	TInlineComponentArray<UActorComponent*> Comps(const_cast<AController*>(C));
	for (UActorComponent* Comp : Comps)
	{
		if (!Comp) { continue; }
		for (const UClass* K = Comp->GetClass(); K; K = K->GetSuperClass())
		{
			if (K->GetName().Contains(TEXT("LyraControllerComponent_CharacterParts")))
			{
				return Comp;
			}
		}
	}
	return nullptr;
}

void UAFLAccessoryPartComponent::RefreshAccessoriesForPawn(APawn* Pawn)
{
	AController* C = GetController<AController>();
	if (!C || !Pawn || !C->HasAuthority())
	{
		// AUTHORITY ONLY. AddCharacterPart is BlueprintAuthorityOnly and the list replicates to clients;
		// a client-side add would be a second, unreplicated source of parts.
		return;
	}

	const APlayerState* PS = C->PlayerState;
	const UAFLCosmeticLoadoutComponent* Loadout =
		PS ? PS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
	if (!Loadout) { return; }

	UActorComponent* Stock = FindStockPartsComponent();
	UFunction* AddFn = Stock ? Stock->FindFunction(FName(TEXT("AddCharacterPart"))) : nullptr;
	UFunction* RemoveFn = Stock ? Stock->FindFunction(FName(TEXT("RemoveCharacterPart"))) : nullptr;
	if (!Stock || !AddFn || !RemoveFn)
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("[AFLAccessoryPart] stock parts component/functions not found (stock=%s add=%s remove=%s) -- nothing attached."),
			Stock ? TEXT("ok") : TEXT("NULL"), AddFn ? TEXT("ok") : TEXT("NULL"), RemoveFn ? TEXT("ok") : TEXT("NULL"));
		return;
	}

	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this);
	if (!Catalog) { return; }

	struct FPartArgs { FLyraCharacterPart Part; };

	// --- 1. TAKE OFF WHAT WE PUT ON, BY VALUE ----------------------------------------------------
	// RemoveAllCharacterParts would take the BODY with it: the body selector and this component share
	// ONE stock component on the controller. Its own comment predicted this exact collision.
	for (const TPair<uint8, FLyraCharacterPart>& KV : AddedParts)
	{
		FPartArgs Args; Args.Part = KV.Value;
		Stock->ProcessEvent(RemoveFn, &Args);
	}
	AddedParts.Reset();

	// --- 2. PUT ON WHAT THE SELECTION SAYS -------------------------------------------------------
	// PENDANT IS ABSENT FROM THIS LIST DELIBERATELY. It is spawned by the chain's part actor onto the
	// chain's mesh; AddCharacterPart can only attach to the PAWN's mesh, so a pendant added here would
	// hang off the body and would render with no chain equipped -- a state that is ruled unreachable.
	static const EAFLAccessorySlot PawnSlots[] = {
		EAFLAccessorySlot::Neck, EAFLAccessorySlot::WristL, EAFLAccessorySlot::WristR };

	const FAFLCosmeticSelection& Sel = Loadout->GetSelection();
	int32 Attached = 0, Skipped = 0;
	for (const EAFLAccessorySlot Slot : PawnSlots)
	{
		const FAFLAccessoryPlacement* P = Sel.AccessorySet.Find(Slot);
		if (!P || !P->IsSet()) { continue; }

		const FAFLCatalogEntry* Row = Catalog->FindEntry(P->AccessoryId);
		if (!Row) { ++Skipped; continue; }

		// The part class is a soft class on the ROW, so a new accessory needs no component edit. A row
		// with none set is skipped LOUDLY rather than silently: an owned, equipped, invisible item is
		// exactly the shape CC-X37 was about.
		UClass* PartClass = Row->AccessoryPartClass.LoadSynchronous();
		if (!PartClass)
		{
			++Skipped;
			UE_LOG(LogAFLCombat, Warning,
				TEXT("[AFLAccessoryPart] %s is equipped at slot %d but its row has no AccessoryPartClass -- nothing to spawn."),
				*P->AccessoryId.ToString(), static_cast<int32>(Slot));
			continue;
		}

		const FName Socket = AFLAccessorySockets::ResolveSocket(Slot);
		if (Socket.IsNone()) { ++Skipped; continue; }

		FPartArgs Args;
		Args.Part.PartClass = PartClass;
		Args.Part.SocketName = Socket;
		Args.Part.CollisionMode = ECharacterCustomizationCollisionMode::NoCollision;
		Stock->ProcessEvent(AddFn, &Args);

		AddedParts.Add(static_cast<uint8>(Slot), Args.Part);
		++Attached;
	}

	// PRESENCE OF OUTPUT: a run that attached nothing and a run that never happened must not look alike.
	UE_LOG(LogAFLCombat, Log, TEXT("[AFLAccessoryPart] %s: attached=%d skipped=%d (pendant is the chain's, not ours)"),
		*GetNameSafe(Pawn), Attached, Skipped);
}
