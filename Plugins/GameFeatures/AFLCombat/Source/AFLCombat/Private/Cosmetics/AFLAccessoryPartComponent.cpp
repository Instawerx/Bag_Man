// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLAccessoryPartComponent.h"

#include "AFLCombat.h"
#include "AFLCosmeticCatalogSubsystem.h"
#include "AFLCosmeticCoreTypes.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/AFLAccessoryIKComponent.h"
#include "Cosmetics/AFLAccessoryPartActor.h"

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

	// ASK THE COMPONENT THE ENGINE WILL ACTUALLY ATTACH TO, not a component of our choosing. Lyra's
	// ULyraPawnComponent_CharacterParts::GetSceneComponentToAttachTo returns Cast<ACharacter>(Owner)->GetMesh(),
	// so resolving the mesh any other way would guard a component the attach never touches -- the guard
	// would pass and the part would still land at the origin.
	const ACharacter* AsCharacter = Cast<ACharacter>(Pawn);
	const USkeletalMeshComponent* PawnMesh = AsCharacter ? AsCharacter->GetMesh() : nullptr;

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

		// FAIL LOUDLY, NEVER FALL BACK TO THE ROOT. AddCharacterPart hands this name to
		// FLyraCharacterPartList::SpawnActorForEntry, which calls
		// SetupAttachment(GetSceneComponentToAttachTo(), SocketName). USceneComponent::SetupAttachment
		// does NOT fail on an unknown socket -- it parents at the component's ORIGIN. Nothing errors,
		// and the piece renders at the pawn's feet looking like an art bug rather than a missing socket.
		// The pendant path has guarded this since it was written (AFLAccessoryChainActor: DoesSocketExist
		// -> "pendant NOT spawned"); this is the same guard on the path that rides the engine's attach,
		// which was the only one still unguarded. A refused attach is recoverable; an invisible silent
		// one is the shape CC-X37 was about.
		if (!PawnMesh || !PawnMesh->DoesSocketExist(Socket))
		{
			++Skipped;
			UE_LOG(LogAFLCombat, Error,
				TEXT("[AFLAccessoryPart] socket '%s' is NOT on %s (mesh=%s) -- %s NOT attached. A silent "
				     "attach would have rendered it at the pawn origin."),
				*Socket.ToString(), *GetNameSafe(Pawn), *GetNameSafe(PawnMesh), *P->AccessoryId.ToString());
			continue;
		}

		FPartArgs Args;
		Args.Part.PartClass = PartClass;
		Args.Part.SocketName = Socket;
		Args.Part.CollisionMode = ECharacterCustomizationCollisionMode::NoCollision;
		Stock->ProcessEvent(AddFn, &Args);

		AddedParts.Add(static_cast<uint8>(Slot), Args.Part);
		++Attached;
	}

	// ---- BRIDGE HOOK 1 of 2: publish the VISIBLE BODY surface ------------------------------------
	// The accessories now fit a surface rather than a fixed offset, and the surface is whatever body
	// part is equipped -- the pawn's own mesh is SKM_Manny_Invis, which carries collision and draws
	// nothing. Registering here rather than inside Lyra's SpawnActorForEntry keeps the hook in AFL
	// code: this runs on every re-resolve, which is exactly when the body could have changed.
	//
	// The body part is the child actor that is NOT one of ours. Identified by class, not by socket:
	// it attaches at socket None, and so would anything else added without one.
	{
		USkeletalMeshComponent* BodyMesh = nullptr;
		TArray<UChildActorComponent*> CACs;
		Pawn->GetComponents<UChildActorComponent>(CACs);
		// PRESENCE OF OUTPUT: the hook running and finding nothing must be distinguishable from the
		// hook never running. The first pass of this block logged only on failure, so a silent
		// RegisterSurface produced no line at all and the two cases were indistinguishable.
		UE_LOG(LogAFLCombat, Log, TEXT("[AFLAccessoryPart] IK hook: scanning %d child-actor component(s) on %s"),
			CACs.Num(), *GetNameSafe(Pawn));
		for (UChildActorComponent* CAC : CACs)
		{
			AActor* Child = CAC ? CAC->GetChildActor() : nullptr;
			if (!Child || Child->IsA<AAFLAccessoryPartActor>()) { continue; }   // ours, not the body
			if (USkeletalMeshComponent* M = Child->FindComponentByClass<USkeletalMeshComponent>())
			{
				BodyMesh = M;
				break;
			}
		}
		if (BodyMesh)
		{
			UAFLAccessoryIKComponent::RegisterSurface(Pawn, FName(TEXT("Body")), BodyMesh);
		}
		else
		{
			// Distinct from "registered and found nothing": if no body part is resolvable the IK has
			// no surface at all, and four zero offsets would otherwise read as a clean fit.
			UE_LOG(LogAFLCombat, Warning,
				TEXT("[AFLAccessoryPart] %s: no VISIBLE body part found to register as the IK surface."),
				*GetNameSafe(Pawn));
		}
	}

	// PRESENCE OF OUTPUT: a run that attached nothing and a run that never happened must not look alike.
	UE_LOG(LogAFLCombat, Log, TEXT("[AFLAccessoryPart] %s: attached=%d skipped=%d (pendant is the chain's, not ours)"),
		*GetNameSafe(Pawn), Attached, Skipped);
}
