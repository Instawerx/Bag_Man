// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLSkinColorControllerComponent.h"

#include "Components/ChildActorComponent.h"
#include "Cosmetics/AFLBrandEdgeMap.h"
#include "Cosmetics/AFLCharacterPartActor.h"
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Cosmetics/AFLSkinColorComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
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

		// Fallback preserves current behavior: an unmapped / un-tagged robot still gets its default,
		// never null.
		// .Get() so both ternary arms are raw UAFLSkinColorAsset* (PersistentSkinColor is a TObjectPtr;
		// mixing it with the raw ResolvedEdge is the C2445 ambiguous-conditional error otherwise).
		UAFLSkinColorAsset* EffectiveColor = bBrandResolved ? ResolvedEdge : PersistentSkinColor.Get();

		if (AFLSkinDiag::IsOn())
		{
			UE_LOG(LogAFLSkinDiag, Log,
				TEXT("%s%s : PushToPawn brandTag=%s mapSet=%s resolve=%s -> %s edge=%s (persistentFallback=%s)"),
				*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
				BrandTag.IsValid() ? *BrandTag.ToString() : TEXT("<none>"),
				BrandEdgeMap ? TEXT("y") : TEXT("n"),
				bBrandResolved ? TEXT("HIT") : TEXT("MISS"),
				bBrandResolved ? TEXT("brand") : TEXT("fallback"),
				EffectiveColor ? *EffectiveColor->GetName() : TEXT("null"),
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
