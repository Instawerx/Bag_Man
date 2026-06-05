// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLSkinColorControllerComponent.h"

#include "Components/ChildActorComponent.h"
#include "Cosmetics/AFLBrandEdgeMap.h"
#include "Cosmetics/AFLCharacterPartActor.h"
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"  // #43: read the player's replicated selection
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Cosmetics/AFLSkinColorComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"               // #43: reach the loadout component on the PlayerState
#include "GameplayTagContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSkinColorControllerComponent)

namespace
{
	// Parent tag the brand identity tags live under (Cosmetic.Brand.<BRAND>). RequestGameplayTag at
	// file/function scope is safe (NOT in a ctor/CDO path) -- this runs at possess, long after tag scan.
	static const FGameplayTag& AFLBrandParentTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("Cosmetic.Brand")));
		return Tag;
	}
}

void UAFLSkinColorControllerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (AController* OwningController = GetController<AController>())
		{
			// AController::OnPossessedPawnChanged is a public engine delegate (AddDynamic linkable -- NOT
			// reflection). Re-push the persistent color on each possession so the skin survives respawn.
			OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

			// Cover the already-possessed case (BeginPlay after possession): push to the current pawn.
			if (APawn* ControlledPawn = GetPawn<APawn>())
			{
				RefreshSkinForPawn(ControlledPawn);
			}
		}
	}
}

void UAFLSkinColorControllerComponent::SetPersistentSkinColor(UAFLSkinColorAsset* NewColor)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		PersistentSkinColor = NewColor;
		if (APawn* ControlledPawn = GetPawn<APawn>())
		{
			RefreshSkinForPawn(ControlledPawn);
		}
	}
}

void UAFLSkinColorControllerComponent::OnPossessedPawnChanged(APawn* /*OldPawn*/, APawn* NewPawn)
{
	// Authority-only (bound under HasAuthority() in BeginPlay). Re-push so the selection survives
	// respawn / re-possession.
	if (NewPawn)
	{
		RefreshSkinForPawn(NewPawn);
	}
}

FGameplayTag UAFLSkinColorControllerComponent::ResolveBrandTag(APawn* Pawn) const
{
	// Same part discovery as UAFLSkinColorComponent::ReapplyColorToAllParts: the body parts are
	// UChildActorComponents on the pawn whose child actor is an AAFLCharacterPartActor. We read the brand
	// tag off the part's StaticGameplayTags via the IGameplayTagAssetInterface it already implements
	// (GetOwnedGameplayTags) -- NO new accessor added to the part. First tag under Cosmetic.Brand wins.
	if (!Pawn)
	{
		return FGameplayTag();
	}

	const FGameplayTag& BrandParent = AFLBrandParentTag();

	TArray<UChildActorComponent*> ChildActorComps;
	Pawn->GetComponents<UChildActorComponent>(ChildActorComps);
	for (UChildActorComponent* CAC : ChildActorComps)
	{
		const AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr);
		if (!Part)
		{
			continue; // non-body child actor (e.g. a weapon) -> skip, same filter as the pawn component
		}

		FGameplayTagContainer PartTags;
		Part->GetOwnedGameplayTags(PartTags); // IGameplayTagAssetInterface (the existing contract)
		for (const FGameplayTag& Tag : PartTags)
		{
			// MatchesTag(BrandParent) is true for Cosmetic.Brand AND any child (Cosmetic.Brand.ARIA, ...).
			// We want a concrete child, so also require it is not the bare parent itself.
			if (Tag.MatchesTag(BrandParent) && Tag != BrandParent)
			{
				return Tag;
			}
		}
	}

	return FGameplayTag();
}

void UAFLSkinColorControllerComponent::RefreshSkinForPawn(APawn* Pawn) const
{
	if (Pawn)
	{
		// --- RESOLUTION (the only thing #38a changes) ---------------------------------------------
		// Decide WHICH preset to push: the robot's brand-default edge if we can resolve one, else the
		// existing PersistentSkinColor. The PROPAGATION below is byte-for-byte the proven path.
		const FGameplayTag BrandTag = ResolveBrandTag(Pawn);
		UAFLSkinColorAsset* ResolvedEdge =
			(BrandEdgeMap && BrandTag.IsValid()) ? BrandEdgeMap->ResolveEdge(BrandTag) : nullptr;
		const bool bBrandResolved = (ResolvedEdge != nullptr);

		// #43: the player's explicit selection takes priority over the #38a brand default for the edge axis.
		// Resolve its EdgeId FName to a preset (against the brand-edge map's known presets until the catalog
		// (S-ECON-CAT) lands).
		//
		// RESPAWN-RACE FIX (Option 1): read the loadout component off the PAWN's PlayerState FIRST, falling
		// back to the controller's only if the pawn isn't linked yet. Pawn->GetPlayerState() is assigned in
		// APawn::PossessedBy, which runs EARLIER in the possession sequence than this cosmetic refresh, so for
		// a pawn that exists the pawn-side PS link is reliably populated -- whereas OwningController->PlayerState
		// is transiently null during the respawn possession window (the bug that dropped the selection to the
		// brand default on _C_3). The component-driven re-push (Option 2, UAFLCosmeticLoadoutComponent OnRep +
		// possession hook) covers the OTHER race -- the selection VALUE arriving after the pawn on a remote
		// client -- so whichever lands last triggers the correct push (the skin pillar's PATH1/PATH2 shape).
		// Capture WHICH PlayerState the selection is read from, for the inactive-PS diagnosis: when a respawn
		// resolves selection=<none> despite the value being committed, we need to know if the pawn's PS is a
		// DIFFERENT instance than the one holding the value (Lyra inactive-PlayerState swap).
		const APlayerState* PawnPS = Pawn->GetPlayerState();
		const AController* OwningController = GetController<AController>();
		const APlayerState* CtrlPS = OwningController ? OwningController->PlayerState : nullptr;

		const APlayerState* SelectionPS = PawnPS ? PawnPS : CtrlPS;
		const TCHAR* SelectionPSPath = PawnPS ? TEXT("pawn") : (CtrlPS ? TEXT("ctrl") : TEXT("none"));

		UAFLSkinColorAsset* SelectedEdge = nullptr;
		const UAFLCosmeticLoadoutComponent* Loadout =
			SelectionPS ? SelectionPS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
		FName SelectedEdgeId = NAME_None;
		if (Loadout)
		{
			SelectedEdgeId = Loadout->GetSelection().EdgeId;
			if (SelectedEdgeId != NAME_None && BrandEdgeMap)
			{
				SelectedEdge = BrandEdgeMap->ResolveEdgeById(SelectedEdgeId);
			}
		}
		const bool bSelectionResolved = (SelectedEdge != nullptr);

		if (AFLSkinDiag::IsOn())
		{
			// The inactive-PS probe: log the PS instance NAMES (pawn-side vs ctrl-side), which one we read,
			// whether a loadout comp was found there, and the raw EdgeId it held. On a failing respawn this
			// reveals if PawnPS != the committed PS (instance swap) or the same PS read empty (timing).
			UE_LOG(LogAFLSkinDiag, Log,
				TEXT("%s%s : SelRead path=%s pawnPS=%s ctrlPS=%s loadout=%s rawEdgeId=%s"),
				*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
				SelectionPSPath,
				PawnPS ? *PawnPS->GetName() : TEXT("null"),
				CtrlPS ? *CtrlPS->GetName() : TEXT("null"),
				Loadout ? TEXT("found") : TEXT("MISSING"),
				(SelectedEdgeId != NAME_None) ? *SelectedEdgeId.ToString() : TEXT("<none>"));
		}

		// Three-tier priority (#43): the player's selection wins; else the #38a brand default; else the
		// PersistentSkinColor fallback (preserves current behavior -- an unmapped/un-tagged robot still gets
		// a non-null default).
		// .Get() so all ternary arms are raw UAFLSkinColorAsset* (PersistentSkinColor is a TObjectPtr;
		// mixing it with a raw arm is the C2445 ambiguous-conditional error otherwise).
		UAFLSkinColorAsset* EffectiveColor =
			bSelectionResolved ? SelectedEdge
			: bBrandResolved   ? ResolvedEdge
			: PersistentSkinColor.Get();

		if (AFLSkinDiag::IsOn())
		{
			// Report the TRUE winning tier by name -- the respawn re-proof is read FROM this line, so the
			// instrument must not claim "brand" when the SELECTION produced EffectiveColor.
			const TCHAR* WinningTier =
				bSelectionResolved ? TEXT("selection")
				: bBrandResolved   ? TEXT("brand")
				: TEXT("fallback");

			UE_LOG(LogAFLSkinDiag, Log,
				TEXT("%s%s : PushToPawn brandTag=%s mapSet=%s tier=%s edge=%s (selection=%s brandDefault=%s persistentFallback=%s)"),
				*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
				BrandTag.IsValid() ? *BrandTag.ToString() : TEXT("<none>"),
				BrandEdgeMap ? TEXT("y") : TEXT("n"),
				WinningTier,
				EffectiveColor ? *EffectiveColor->GetName() : TEXT("null"),
				bSelectionResolved ? *SelectedEdge->GetName() : TEXT("<none>"),
				bBrandResolved ? *ResolvedEdge->GetName() : TEXT("<none>"),
				PersistentSkinColor ? *PersistentSkinColor->GetName() : TEXT("null"));
		}

		if (UAFLSkinColorComponent* PawnComp = Pawn->FindComponentByClass<UAFLSkinColorComponent>())
		{
			// Authority -> sets the replicated SkinColor -> all clients re-apply via OnRep (PATH 2) +
			// the new pawn's parts self-color on their BeginPlay (PATH 1). UNCHANGED propagation route:
			// the resolved value rides DOREPLIFETIME SkinColor exactly as PersistentSkinColor did.
			PawnComp->SetSkinColor(EffectiveColor);
		}
	}
}
