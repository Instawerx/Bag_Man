// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLCharacterPartSelectorComponent.h"

#include "Cosmetics/AFLCharacterPartMap.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"   // read the player's replicated selection (#43)
#include "Cosmetics/AFLSkinColorComponent.h"          // LogAFLSkinDiag + AFLSkinDiag (shared diag channel)
#include "GameFramework/Actor.h"            // TInlineComponentArray
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Cosmetics/LyraCharacterPartTypes.h"  // FLyraCharacterPart (the ProcessEvent arg struct member)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCharacterPartSelectorComponent)

void UAFLCharacterPartSelectorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Mirror UAFLSkinColorControllerComponent: authority-only possession hook + already-possessed catch.
	if (HasAuthority())
	{
		if (AController* OwningController = GetController<AController>())
		{
			// AController::OnPossessedPawnChanged is a public engine delegate (AddDynamic linkable, NOT
			// reflection). Re-resolve the body on each possession so the right robot survives respawn /
			// re-possession (the body-axis analogue of the color sibling's re-push).
			OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

			// Cover the already-possessed case (BeginPlay after possession): resolve for the current pawn.
			if (APawn* ControlledPawn = GetPawn<APawn>())
			{
				ResolveBodyForPawn(ControlledPawn);
			}
		}
	}
}

void UAFLCharacterPartSelectorComponent::OnPossessedPawnChanged(APawn* /*OldPawn*/, APawn* NewPawn)
{
	// Authority-only (bound under HasAuthority() in BeginPlay). Resolve the body for the new pawn.
	if (NewPawn)
	{
		ResolveBodyForPawn(NewPawn);
	}
}

FName UAFLCharacterPartSelectorComponent::ResolveIdentityId(APawn* Pawn) const
{
	if (!Pawn)
	{
		return NAME_None;
	}

	// RESPAWN-RACE FIX (mirrors RefreshSkinForPawn): read the loadout off the PAWN's PlayerState FIRST,
	// falling back to the controller's. Pawn->GetPlayerState() is assigned in APawn::PossessedBy, which runs
	// EARLIER in the possession sequence than this hook, so for a pawn that exists the pawn-side PS link is
	// reliably populated -- whereas OwningController->PlayerState is transiently null during the respawn
	// possession window.
	const APlayerState* PawnPS = Pawn->GetPlayerState();
	const AController* OwningController = GetController<AController>();
	const APlayerState* CtrlPS = OwningController ? OwningController->PlayerState : nullptr;
	const APlayerState* SelectionPS = PawnPS ? PawnPS : CtrlPS;

	const UAFLCosmeticLoadoutComponent* Loadout =
		SelectionPS ? SelectionPS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;

	return Loadout ? Loadout->GetSelection().GetActiveIdentityId() : NAME_None;
}

void UAFLCharacterPartSelectorComponent::ResolveBodyForPawn(APawn* Pawn) const
{
	if (!Pawn)
	{
		return;
	}

	// --- RESOLUTION ---------------------------------------------------------------------------------
	// The player's identity id (AFL.Team.<Name> / AFL.Character.<Name>) -> the robot body class via the map;
	// on any miss (no loadout / unset id / unmapped / null map) fall back to the configured default body
	// (FallbackPartClass = B_AFL_Robot_IRONICS, the free-grant brand-default base identity).
	const FName IdentityId = ResolveIdentityId(Pawn);

	TSoftClassPtr<AActor> ResolvedSoft;
	if (PartMap && !IdentityId.IsNone())
	{
		ResolvedSoft = PartMap->ResolveCharacterPart(IdentityId);
	}

	const bool bMapped = !ResolvedSoft.IsNull();
	const TSoftClassPtr<AActor>& ChosenSoft = bMapped ? ResolvedSoft : FallbackPartClass;

	// Synchronous load: robot BPs are already-cooked content, tiny per-class, loaded on demand (the same
	// pattern UAFLCosmeticCatalogSubsystem uses for cosmetic assets). A possession is a rare event, not a
	// per-tick hot path, so a blocking load here is acceptable.
	UClass* PartClass = ChosenSoft.IsNull() ? nullptr : ChosenSoft.LoadSynchronous();

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log,
			TEXT("%s%s : ResolveBody identityId=%s tier=%s partClass=%s (mapSet=%s fallbackSet=%s)"),
			*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
			IdentityId.IsNone() ? TEXT("<none>") : *IdentityId.ToString(),
			bMapped ? TEXT("map") : TEXT("fallback"),
			PartClass ? *PartClass->GetName() : TEXT("null"),
			PartMap ? TEXT("y") : TEXT("n"),
			FallbackPartClass.IsNull() ? TEXT("n") : TEXT("y"));
	}

	if (!PartClass)
	{
		// Nothing resolved AND no fallback set -> do not add a body (don't spawn a null/garbage part).
		return;
	}

	// --- ADD ----------------------------------------------------------------------------------------
	// Add the body via the stock ULyraControllerComponent_CharacterParts on THIS controller -- that's the
	// canonical seam (it's B_BagMan_AssignCharacterPart's own base) and it feeds the pawn's customizer
	// FastArray internally (DOREPLIFETIME CharacterPartList -> PostReplicatedAdd spawns on every client).
	//
	// CALLED VIA REFLECTION (ProcessEvent), NOT a direct C++ call: ULyra*Component_CharacterParts and their
	// AddCharacterPart(...) are module-private to LyraGame (the classes carry NO LYRAGAME_API export), so a
	// direct call link-errors cross-module (LNK2019 -- the documented Lyra-cosmetic export trap). Both
	// AddCharacterPart overloads ARE UFUNCTIONs, so the reflected thunk is callable. We find the stock
	// controller component by class NAME (no compile-time type dep on the unexported class) and FindFunction
	// + ProcessEvent it with an arg struct laid out to match `void AddCharacterPart(FLyraCharacterPart)`.
	// Find the stock ULyraControllerComponent_CharacterParts on this controller. It is a BP SUBCLASS
	// (B_BagMan_AssignCharacterPart_C), so a substring match on the leaf class NAME misses -- we must walk
	// the class SUPER-CHAIN for the base name (the unexported base can't be IsA<>'d at compile time).
	AController* OwningController = GetController<AController>();
	UActorComponent* StockPartsComp = nullptr;
	if (OwningController)
	{
		TInlineComponentArray<UActorComponent*> Comps(OwningController);
		for (UActorComponent* Comp : Comps)
		{
			if (!Comp) { continue; }
			for (const UClass* C = Comp->GetClass(); C; C = C->GetSuperClass())
			{
				if (C->GetName().Contains(TEXT("LyraControllerComponent_CharacterParts")))
				{
					StockPartsComp = Comp;
					break;
				}
			}
			if (StockPartsComp) { break; }
		}
	}

	UFunction* AddFn = StockPartsComp ? StockPartsComp->FindFunction(FName(TEXT("AddCharacterPart"))) : nullptr;
	// REPLACE-NOT-APPEND: AddCharacterPart APPENDS to the FastArray, so a double-fire (BeginPlay catch +
	// OnPossessedPawnChanged) or a re-resolve would STACK robots (the bug: an ARIA fallback-fire + an IRONICS
	// fire left two overlapping bodies -> z-fight glitch + ResolveBrandTag reading the wrong/first part's
	// brand). Clear first: RemoveAllCharacterParts is the body-axis idempotency the color sibling gets for
	// free (color is a param overwrite; a body is an add, so we must remove-then-add). The selector is the
	// SOLE body source on this stock comp (static CharacterParts pin is empty), so clearing all is correct
	// here. (If a future non-body part ever rides this same stock comp, switch to handle-tracked single
	// removal instead of clear-all.)
	UFunction* RemoveAllFn = StockPartsComp ? StockPartsComp->FindFunction(FName(TEXT("RemoveAllCharacterParts"))) : nullptr;

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log,
			TEXT("%s%s : AddBody partClass=%s stockComp=%s addFn=%s removeAllFn=%s"),
			*AFLSkinDiag::Prefix(this), *GetNameSafe(Pawn), *GetNameSafe(PartClass),
			StockPartsComp ? *StockPartsComp->GetClass()->GetName() : TEXT("NULL"),
			AddFn ? TEXT("found") : TEXT("NULL"),
			RemoveAllFn ? TEXT("found") : TEXT("NULL"));
	}

	if (StockPartsComp && AddFn)
	{
		// Clear any previously-added body first (idempotent replace). RemoveAllCharacterParts takes no args.
		if (RemoveAllFn)
		{
			StockPartsComp->ProcessEvent(RemoveAllFn, nullptr);
		}

		// Arg layout MUST match the UFUNCTION signature: a single `FLyraCharacterPart NewPart`.
		struct FAddCharacterPartArgs { FLyraCharacterPart NewPart; };
		FAddCharacterPartArgs Args;
		Args.NewPart.PartClass = PartClass;
		Args.NewPart.SocketName = NAME_None;
		Args.NewPart.CollisionMode = ECharacterCustomizationCollisionMode::NoCollision;
		StockPartsComp->ProcessEvent(AddFn, &Args);
	}
}
