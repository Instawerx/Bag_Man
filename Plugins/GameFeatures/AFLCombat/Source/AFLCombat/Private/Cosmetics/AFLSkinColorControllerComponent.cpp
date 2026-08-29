// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLSkinColorControllerComponent.h"

#include "AbilitySystemGlobals.h"                    // Block 44: spawn-race probe (ASC-not-ready Warning)
#include "Components/ChildActorComponent.h"
#include "Cook/AFLCookedAssetRegistry.h"
#include "Cosmetics/AFLBrandEdgeMap.h"
#include "Cosmetics/AFLCharacterPartActor.h"
#include "AFLCosmeticCatalogSubsystem.h"  // S-ECON-CAT (AFLCosmeticCore module): id->asset registry (replaces ResolveEdgeById stopgap)
#include "Cosmetics/AFLCosmeticLoadoutComponent.h"  // #43: read the player's replicated selection
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Cosmetics/AFLSkinColorComponent.h"
#include "Materials/MaterialInstanceConstant.h"   // facemask: the resolved mask MIC swapped onto slot 1
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"               // #43: reach the loadout component on the PlayerState
#include "GameplayTagContainer.h"
// #43 WeaponId consumer -- resolve the selected weapon SKU to its equipment def + equip it on the pawn.
#include "Cosmetics/AFLWeaponCosmeticAsset.h"         // the carrier (WeaponId -> EquipmentDefinition); AFLCombat-homed, brings the full ULyraEquipmentDefinition type
#include "Equipment/LyraEquipmentManagerComponent.h"  // EquipItem / UnequipItem / GetEquipmentInstancesOfType
#include "Equipment/LyraEquipmentInstance.h"          // the equipped instance we track + unequip
#include "Inventory/LyraInventoryItemDefinition.h"    // Block 28: the QuickBar rail grants an ITEM, not equipment
#include "Inventory/LyraInventoryItemInstance.h"      // ... the granted instance handed to AddItemToSlot
#include "Inventory/LyraInventoryManagerComponent.h"  // ... AddItemDefinition (UE_API-exported, safe from C++)
#include "Weapons/LyraRangedWeaponInstance.h"         // the weapon instance type we replace (AFL weapons derive from it)

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

UAFLSkinColorControllerComponent::UAFLSkinColorControllerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// BASE FACEMASK default (SSOT player-flow 9.2): the IRONICS HUD visor. A SOFT path -> the CDO carries the
	// cook reference but the asset isn't force-loaded until RefreshFacemaskForPawn actually falls back to it.
	// Configured as a DATA ASSET (resolved directly, never through the CosmeticId catalog), mirroring
	// BrandEdgeMap -> robust to the facemask catalog's id state. A plugin->/Game C++ asset ref is the
	// established AFL pattern (cf. AAFLDismemberedHead's gib FObjectFinder); only /Game->plugin is restricted.
	BaseFacemask = TSoftObjectPtr<UAFLSkinColorAsset>(FSoftObjectPath(
		TEXT("/Game/BagMan/Characters/Cosmetics/SkinColors/DA_AFL_Facemask_IroVisor.DA_AFL_Facemask_IroVisor")));

	// GROUND-ZERO body default (house identity). Same shape as BaseFacemask above: a DIRECT data-asset path,
	// not a CosmeticId, so the base look resolves regardless of catalog/entitlement state. This is the LAST
	// body tier -- it fires ONLY when a brand has no authored default in BrandEdgeMap.
	BaseBodyFinish = TSoftObjectPtr<UAFLSkinColorAsset>(FSoftObjectPath(
		TEXT("/Game/BagMan/Characters/Cosmetics/Finishes/DA_AFL_Finish_Blue_Ironics.DA_AFL_Finish_Blue_Ironics")));
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
				// Facemask FIRST (slot-1 material swap), THEN skin (param push) -> the finish layers on top of
				// the swapped material; the swap never strands the finish (the composition order, server-side).
				RefreshFacemaskForPawn(ControlledPawn);
				// #43 WeaponId consumer: equip the selected weapon (D2 replace) -- already-possessed-at-BeginPlay case.
				RefreshWeaponForPawn(ControlledPawn);
				// ... then apply the weapon COLOR (the WeaponId suffix) -- AFTER equip so the weapon mesh exists.
				RefreshWeaponSkinForPawn(ControlledPawn);
				// INDEPENDENT BeamId axis: apply the selected beam to the equipped weapon (overrides its default beam).
				RefreshBeamColorForPawn(ControlledPawn);
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
	// respawn / re-possession. Facemask FIRST (material swap), THEN skin (param push) -- composition order.
	if (NewPawn)
	{
		RefreshFacemaskForPawn(NewPawn);
		// #43 WeaponId consumer: equip the selected weapon (D2 replace) on possession/respawn.
		RefreshWeaponForPawn(NewPawn);
		// ... then apply the weapon COLOR (the WeaponId suffix) -- AFTER equip so the weapon mesh exists.
		RefreshWeaponSkinForPawn(NewPawn);
		// INDEPENDENT BeamId axis: apply the selected beam to the equipped weapon (overrides its default beam).
		RefreshBeamColorForPawn(NewPawn);
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

	// DUAL-MODE WALK (AFL-3214): the display pawn wears parts as attached actors, not CACs.
	TArray<AAFLCharacterPartActor*> BrandParts;
	AAFLCharacterPartActor::CollectPartsOn(Pawn, BrandParts);
	for (const AAFLCharacterPartActor* Part : BrandParts)
	{
		if (!Part)
		{
			continue;
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

// --- STORE PREVIEW (front-end try-before-buy) --------------------------------------------------------
// The 5 Refresh*ForPawn read their axis ids through GetEffectiveSelection. With no preview set it returns the
// committed loadout selection (the exact in-match behavior); with a preview set it returns the override, so the
// display pawn shows an item WITHOUT committing it -- and since the entitlement gate lives only in the commit
// (ServerSetCosmeticSelection), an UNOWNED id previews for free. Never set in-match -> in-match is unchanged.
void UAFLSkinColorControllerComponent::SetPreviewSelection(const FAFLCosmeticSelection& InPreview)
{
	PreviewSelection = InPreview;
}

void UAFLSkinColorControllerComponent::ClearPreviewSelection()
{
	PreviewSelection.Reset();
}

const FAFLCosmeticSelection* UAFLSkinColorControllerComponent::GetEffectiveSelection(const APlayerState* SelectionPS) const
{
	if (PreviewSelection.IsSet())
	{
		return &PreviewSelection.GetValue();
	}
	const UAFLCosmeticLoadoutComponent* Loadout =
		SelectionPS ? SelectionPS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
	return Loadout ? &Loadout->GetSelection() : nullptr;
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
		// STORE PREVIEW: read the axis ids from the effective selection (preview override if set, else committed).
		const FAFLCosmeticSelection* EffSel = GetEffectiveSelection(SelectionPS);
		FName SelectedEdgeId = NAME_None;
		const TCHAR* SelResolveVia = TEXT("-");
		if (EffSel)
		{
			SelectedEdgeId = EffSel->EdgeId;
			if (SelectedEdgeId != NAME_None)
			{
				// S-ECON-CAT: resolve the EdgeId through the catalog (the id->asset registry) -- the ONE source
				// every economy system reads. The cast is safe: a SkinColor_Edge entry's Asset is a
				// UAFLSkinColorAsset. The transitional ResolveEdgeById/BrandEdgeMap-scan stopgap was RETIRED
				// once resolveVia=catalog was watched-clean (catalog proven the live source): catalog resolution
				// is now the SOLE path, so a miss on a real selection fails LOUD (SelectedEdge stays null ->
				// falls to the brand default in the tier resolution below, resolveVia logs nothing fired) rather
				// than silently riding a fallback -- the unforgiving bar the non-skin types (helmet/EMP) need too.
				if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
				{
					SelectedEdge = Cast<UAFLSkinColorAsset>(Catalog->ResolveAsset(SelectedEdgeId));
					if (SelectedEdge)
					{
						SelResolveVia = TEXT("catalog");
					}
				}
			}
		}
		const bool bSelectionResolved = (SelectedEdge != nullptr);

		if (AFLSkinDiag::IsOn())
		{
			// The inactive-PS probe: log the PS instance NAMES (pawn-side vs ctrl-side), which one we read,
			// whether a loadout comp was found there, and the raw EdgeId it held. On a failing respawn this
			// reveals if PawnPS != the committed PS (instance swap) or the same PS read empty (timing).
			UE_LOG(LogAFLSkinDiag, Log,
				TEXT("%s%s : SelRead path=%s pawnPS=%s ctrlPS=%s loadout=%s rawEdgeId=%s resolveVia=%s"),
				*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
				SelectionPSPath,
				PawnPS ? *PawnPS->GetName() : TEXT("null"),
				CtrlPS ? *CtrlPS->GetName() : TEXT("null"),
				Loadout ? TEXT("found") : TEXT("MISSING"),
				(SelectedEdgeId != NAME_None) ? *SelectedEdgeId.ToString() : TEXT("<none>"),
				SelResolveVia);  // S-ECON-CAT: "catalog" = catalog resolved the id; "-" = no id set OR catalog miss (stopgap retired -> a real-selection miss shows here, falls to brand default below)
		}

		// OPTION B dual-resolve. The brand default is a Finish (a BODY color, not an edge glow) -> it, and the
		// PersistentSkinColor fallback, belong on the BODY axis. The EDGE axis is therefore SELECTION-ONLY now:
		// a player edge choice or nothing. This is the #1 correctness re-route (approved): fresh spawn / no
		// selection -> the brand-default Finish drives the body (IRONICS red via the body axis), edge overlay absent.
		// .Get() so all ternary arms are raw UAFLSkinColorAsset* (PersistentSkinColor is a TObjectPtr; mixing it
		// with a raw arm is the C2445 ambiguous-conditional error otherwise).
		UAFLSkinColorAsset* EffectiveEdge = bSelectionResolved ? SelectedEdge : nullptr;

		// BODY axis (TeamColor): resolve the player's BodyId -> a Finish via the SAME catalog the edge uses; else
		// the brand-default Finish; else the persistent fallback (keeps the non-null default an unmapped robot shows).
		UAFLSkinColorAsset* SelectedBody = nullptr;
		FName SelectedBodyId = NAME_None;
		const TCHAR* BodyResolveVia = TEXT("-");
		if (EffSel)
		{
			SelectedBodyId = EffSel->BodyId;
			if (SelectedBodyId != NAME_None)
			{
				if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
				{
					SelectedBody = Cast<UAFLSkinColorAsset>(Catalog->ResolveAsset(SelectedBodyId));
					if (SelectedBody)
					{
						BodyResolveVia = TEXT("catalog");
					}
				}
			}
		}
		// GROUND-ZERO tier (house default). Soft-load ONLY if we actually reach it -- a brand with an authored
		// default never touches this, so the 28 signature colours are untouched and nothing extra is loaded.
		UAFLSkinColorAsset* BaseBody = nullptr;
		if (SelectedBody == nullptr && !bBrandResolved && !BaseBodyFinish.IsNull())
		{
			BaseBody = BaseBodyFinish.LoadSynchronous();
		}

		// All arms RAW UAFLSkinColorAsset* (PersistentSkinColor via .Get()) -- mixing a TObjectPtr arm into
		// this conditional is the C2445 ambiguous-conditional error.
		UAFLSkinColorAsset* EffectiveBody =
			(SelectedBody != nullptr) ? SelectedBody
			: bBrandResolved          ? ResolvedEdge
			: (BaseBody != nullptr)   ? BaseBody
			: PersistentSkinColor.Get();

		if (AFLSkinDiag::IsOn())
		{
			// Report BOTH axes by name -- the respawn re-proof AND the #1 fresh-spawn gate are read FROM this line.
			const TCHAR* BodyTier =
				(SelectedBody != nullptr) ? TEXT("selection")
				: bBrandResolved          ? TEXT("brand")
				: (BaseBody != nullptr)   ? TEXT("base")
				: TEXT("fallback");

			UE_LOG(LogAFLSkinDiag, Log,
				TEXT("%s%s : PushToPawn(dual) brandTag=%s mapSet=%s | EDGE edge=%s (sel=%s) | BODY tier=%s body=%s (selBody=%s via=%s brandDefault=%s base=%s persistent=%s)"),
				*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
				BrandTag.IsValid() ? *BrandTag.ToString() : TEXT("<none>"),
				BrandEdgeMap ? TEXT("y") : TEXT("n"),
				EffectiveEdge ? *EffectiveEdge->GetName() : TEXT("<none>"),
				bSelectionResolved ? *SelectedEdge->GetName() : TEXT("<none>"),
				BodyTier,
				EffectiveBody ? *EffectiveBody->GetName() : TEXT("null"),
				(SelectedBody != nullptr) ? *SelectedBody->GetName() : TEXT("<none>"),
				BodyResolveVia,
				bBrandResolved ? *ResolvedEdge->GetName() : TEXT("<none>"),
				BaseBody ? *BaseBody->GetName() : TEXT("<not reached>"),
				PersistentSkinColor ? *PersistentSkinColor->GetName() : TEXT("null"));
		}

		// CREATOR COLOUR OVERLAY (CC-2.1): build the per-channel override from the effective selection's creator
		// fields and PASS IT INTO the push (never pulled inside ApplySkinColor). Invalid unless bUseCreatorColors ->
		// every non-creator push is byte-identical. Rides the SAME re-driven resolve path as the edge/body axes,
		// so the OnRep_Selection -> NudgeControllerReapply re-drive covers the late-Selection race for it too.
		const FAFLColorOverride ColorOverride =
			EffSel ? UAFLCosmeticLoadoutComponent::BuildColorOverride(*EffSel) : FAFLColorOverride();

		if (UAFLSkinColorComponent* PawnComp = Pawn->FindComponentByClass<UAFLSkinColorComponent>())
		{
			// Authority -> sets the replicated BodyColor + SkinColor (two DOREPLIFETIME props) -> all clients
			// re-apply via OnRep (PATH 2) + the new pawn's parts self-color on their BeginPlay (PATH 1). The body
			// rides DOREPLIFETIME BodyColor exactly as the edge rides DOREPLIFETIME SkinColor (parallel axes).
			// SERVER-SIDE write of the pawn-side replicated overlay -- the one moment holding BOTH the PlayerState
			// (carrying the selection) and the pawn. Set BEFORE the colour pushes so the listen-host re-apply sees it.
			PawnComp->SetColorOverride(ColorOverride);
			PawnComp->SetBodyColor(EffectiveBody);   // body finish (TeamColor)
			PawnComp->SetSkinColor(EffectiveEdge);   // edge overlay (emissive); null = no edge
		}
	}
}

void UAFLSkinColorControllerComponent::RefreshEmblemForPawn(APawn* Pawn) const
{
	if (!Pawn) { return; }

	// SAME selection resolution as the facemask: pawn PlayerState first, controller's as the fallback,
	// through GetEffectiveSelection so a creator PREVIEW override is honoured. One path, not two.
	const APlayerState* PawnPS = Pawn->GetPlayerState();
	const AController* OwningController = GetController<AController>();
	const APlayerState* CtrlPS = OwningController ? OwningController->PlayerState : nullptr;
	const APlayerState* SelectionPS = PawnPS ? PawnPS : CtrlPS;
	const FAFLCosmeticSelection* EffSel = GetEffectiveSelection(SelectionPS);

	UMaterialInstanceConstant* EmblemMIC = nullptr;
	const FName EmblemId = EffSel ? EffSel->EmblemId : NAME_None;
	if (EmblemId != NAME_None)
	{
		if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
		{
			if (const UAFLSkinColorAsset* EmblemAsset = Cast<UAFLSkinColorAsset>(Catalog->ResolveAsset(EmblemId)))
			{
				EmblemMIC = EmblemAsset->GetEmblemMaterial();
			}
		}
	}

	// NO BRAND-DEFAULT FALLBACK, deliberately, and this is where it differs from the facemask. An unset
	// emblem means "no mark", and ApplyEmblem(nullptr) restores the chassis-authored decal. Substituting
	// a default here would put a brand on a player who chose none -- and the identity tint already
	// reaches the decal by the separate path that has always existed.
	if (APlayerState* PS = Pawn->GetPlayerState())
	{
		if (UAFLSkinColorComponent* SkinComp = PS->FindComponentByClass<UAFLSkinColorComponent>())
		{
			SkinComp->SetEmblem(EmblemMIC);
			return;
		}
	}
	if (UAFLSkinColorComponent* PawnComp = Pawn->FindComponentByClass<UAFLSkinColorComponent>())
	{
		PawnComp->SetEmblem(EmblemMIC);
	}
}

void UAFLSkinColorControllerComponent::RefreshFacemaskForPawn(APawn* Pawn) const
{
	// MIRRORS RefreshSkinForPawn's resolve+push shape, for the FACEMASK axis (a slot-1 base-MATERIAL swap, not a
	// param push). TWO TIERS like the skin's selection > brand-default: resolve the player's equipped FacemaskId
	// off the PlayerState loadout (catalog resolveVia -> the facemask UAFLSkinColorAsset -> its FacemaskMaterial
	// MIC); on no selection (or a catalog miss) fall to the configured BaseFacemask DATA ASSET (the base visor) so
	// robots are never bare-headed. Push the resolved MIC to the pawn component's replicated Facemask so all
	// clients converge. Only an empty BaseFacemask leaves it null -> un-equip. Authority-only (skin-push guard).
	if (!Pawn)
	{
		return;
	}

	// Read the selection from the PAWN's PlayerState first (populated in PossessedBy before this refresh),
	// falling back to the controller's -- the SAME respawn-race-safe PS resolution RefreshSkinForPawn uses.
	const APlayerState* PawnPS = Pawn->GetPlayerState();
	const AController* OwningController = GetController<AController>();
	const APlayerState* CtrlPS = OwningController ? OwningController->PlayerState : nullptr;
	const APlayerState* SelectionPS = PawnPS ? PawnPS : CtrlPS;

	// STORE PREVIEW: the effective selection is the preview override if set, else the committed loadout.
	const FAFLCosmeticSelection* EffSel = GetEffectiveSelection(SelectionPS);

	UMaterialInstanceConstant* FacemaskMIC = nullptr;
	const FName FacemaskId = EffSel ? EffSel->FacemaskId : NAME_None;
	const bool bSelection = (FacemaskId != NAME_None);
	const TCHAR* Tier = TEXT("none");

	if (bSelection)
	{
		// SELECTION TIER: the player's equipped facemask, resolved through the catalog id->asset registry. The
		// facemask CosmeticId resolves to a UAFLSkinColorAsset whose FacemaskMaterial is the slot-1 MIC (the
		// proven MI_AFL_FaceMask_* path). A miss leaves FacemaskMIC null -> falls to the base-default tier below.
		if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
		{
			if (const UAFLSkinColorAsset* MaskAsset = Cast<UAFLSkinColorAsset>(Catalog->ResolveAsset(FacemaskId)))
			{
				FacemaskMIC = MaskAsset->GetFacemaskMaterial();
				if (FacemaskMIC) { Tier = TEXT("selection"); }
			}
		}
	}

	if (!FacemaskMIC)
	{
		// BASE-DEFAULT TIER -- the exact mirror of RefreshSkinForPawn's brand-default, which resolves a configured
		// DATA ASSET (BrandEdgeMap->ResolveEdge -> a UAFLSkinColorAsset), NOT a CosmeticId. So this resolves the
		// configured BaseFacemask DATA ASSET DIRECTLY: no selection (or a selection that missed the catalog) falls
		// to the base visor (DA_AFL_Facemask_IroVisor / T_AFL_Visor_Ironics, SSOT player-flow 9.2) instead of
		// un-equipping -> robots are never bare-headed. Resolving the DATA ASSET directly (not via the CosmeticId
		// catalog) keeps the base visor robust to the facemask catalog's id state. Empty BaseFacemask -> un-equip.
		if (const UAFLSkinColorAsset* BaseMask = BaseFacemask.LoadSynchronous())
		{
			FacemaskMIC = BaseMask->GetFacemaskMaterial();
			if (FacemaskMIC) { Tier = TEXT("base-default"); }
		}
	}

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : RefreshFacemask facemaskId=%s tier=%s -> mic=%s"),
			*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
			bSelection ? *FacemaskId.ToString() : TEXT("<none>"),
			Tier,
			FacemaskMIC ? *FacemaskMIC->GetName() : TEXT("null"));
	}

	if (UAFLSkinColorComponent* PawnComp = Pawn->FindComponentByClass<UAFLSkinColorComponent>())
	{
		// Authority -> replicated Facemask -> all clients swap slot-1 via OnRep_Facemask (PATH 2) + the new
		// pawn's parts pick it up on BeginPlay (PATH 1). The pawn component re-applies the finish AFTER the swap
		// (it passes the current SkinColor into ApplyFacemask) so the composition order holds on every client.
		PawnComp->SetFacemask(FacemaskMIC);
	}
}

void UAFLSkinColorControllerComponent::RefreshWeaponForPawn(APawn* Pawn)
{
	// #43 WeaponId consumer -- the weapon-EQUIP axis on the SAME proven spine as RefreshSkin/Facemask (possession
	// + OnRep + nudge). D2 = REPLACE via an owned instance: resolve the selected WeaponId -> a
	// UAFLWeaponCosmeticAsset carrier -> its ULyraEquipmentDefinition -> EquipItem, having first unequipped the
	// current primary so the selection REPLACES rather than stacks. Server-only (EquipItem is authority-only);
	// Lyra's FLyraEquipmentList fast-array replicates the equipped weapon to every client -- no client push here.
	if (!Pawn)
	{
		return;
	}

	// AUTHORITY GATE: only the server equips. On a remote client the pawn is simulated -> bail; it converges via
	// the equipment fast-array (mirrors SetSkinColor's internal authority guard). NudgeControllerReapply reaches
	// here on clients (OnRep) too, so this guard is load-bearing.
	if (!HasAuthority())
	{
		return;
	}

	// FIX A (bot-fire, 2026-07-17): the WEAPON-EQUIP axis is PLAYER-facing cosmetics only. On an AI/bot pawn it
	// would unequip the loadout weapon (PulseCarbine -- valid instigator, bot-fireable AFLAG_Laser_Pulse) and
	// EquipItem the cosmetic weapon WITHOUT calling SetInstigator (the ~L494 replace). BTS_Shoot's GetCurrentWeapon
	// then reads a NULL instigator -> the cast to the inventory item fails -> CanShoot=false -> the bot never sends
	// InputTag.Weapon.Fire, so it moves/engages but never shoots. Humans are unaffected (input-fire activates the
	// ability directly and never reads the weapon instigator). Bots need no weapon cosmetic -- skip THIS axis for
	// them. The other cosmetic axes are SEPARATE functions (RefreshSkinForPawn / RefreshFacemaskForPawn /
	// RefreshWeaponSkinForPawn / RefreshBeamColorForPawn), so bots still get those harmless tints; only this
	// gameplay-breaking weapon swap is skipped. (Owner controller is used -- always set on this controller
	// component; IsPlayerController() distinguishes a human APlayerController from an AAIController-driven bot
	// WITHOUT pulling the AIModule header into this Unity TU.)
	const AController* OwnerController = GetController<AController>();
	if (OwnerController != nullptr && !OwnerController->IsPlayerController())
	{
		return;
	}

	// NEW-PAWN RESET (respawn / first possession): the prior pawn's instance died with it -> drop stale tracking
	// so the fresh pawn re-equips clean (and a cross-pawn idempotency false-positive can't skip the equip).
	if (WeaponTrackedPawn.Get() != Pawn)
	{
		WeaponTrackedPawn = Pawn;
		SelectedWeaponInstance = nullptr;
		EquippedWeaponId = NAME_None;
		// DUAL-MOUNT: drop the LEFT-hand tracking too so a respawn re-equips both cannons clean (shared tracking
		// with RefreshHandCannonsForPawn, which is only ever dispatched from below).
		SelectedLeftWeaponInstance = nullptr;
		EquippedLeftWeaponId = NAME_None;
	}

	// Read the selected WeaponId off the PAWN's PlayerState first (respawn-race-safe -- the exact PS resolution
	// RefreshSkinForPawn uses), falling back to the controller's.
	const APlayerState* PawnPS = Pawn->GetPlayerState();
	const AController* OwningController = GetController<AController>();
	const APlayerState* CtrlPS = OwningController ? OwningController->PlayerState : nullptr;
	const APlayerState* SelectionPS = PawnPS ? PawnPS : CtrlPS;

	// STORE PREVIEW: the effective selection is the preview override if set, else the committed loadout.
	const FAFLCosmeticSelection* EffSel = GetEffectiveSelection(SelectionPS);
	const FName WeaponId = EffSel ? EffSel->WeaponId : NAME_None;
	const FName LeftWeaponId = EffSel ? EffSel->LeftWeaponId : NAME_None;

	// DUAL-MOUNT DISPATCH (Hand-Cannon line): a LEFT weapon set -> the akimbo body that holds BOTH cannons at once
	// (D2/D3). Every single-held gun leaves LeftWeaponId == NAME_None and never enters here, so the single path
	// below stays byte-identical. Reached AFTER the authority gate + bot skip + new-pawn reset above.
	if (LeftWeaponId != NAME_None)
	{
		RefreshHandCannonsForPawn(Pawn, WeaponId, LeftWeaponId);
		return;
	}

	// DUAL->SINGLE transition: we WERE holding a left cannon but the selection dropped it (back to a single gun).
	// The single replace below unequips ALL ranged (which includes the now-orphaned left cannon), so the gun ends
	// up alone -- just clear the stale left tracking here so it doesn't linger as a false idempotency key.
	if (EquippedLeftWeaponId != NAME_None)
	{
		SelectedLeftWeaponInstance = nullptr;
		EquippedLeftWeaponId = NAME_None;
	}

	// IDEMPOTENT: already realized this WeaponId on this pawn -> no-op. The dual spine re-runs (possession + OnRep
	// + nudge) MUST NOT re-equip/stack. A dropped instance (id set but the instance went stale) falls through ->
	// self-heals by re-equipping.
	// ⚠ QuickBarRoutedItem is the handle the RAIL sets; SelectedWeaponInstance is the handle the legacy path
	// set. Testing only the latter (as this did) meant the guard NEVER short-circuited for a routed weapon,
	// so every possession/respawn re-ran the route and granted ANOTHER inventory item -- one orphan per
	// respawn, invisible in PIE because slot 3 always displays the newest.
	if (WeaponId == EquippedWeaponId
		&& (WeaponId == NAME_None || QuickBarRoutedItem.IsValid() || SelectedWeaponInstance.IsValid()))
	{
		return;
	}

	ULyraEquipmentManagerComponent* EquipMgr = Pawn->FindComponentByClass<ULyraEquipmentManagerComponent>();
	if (!EquipMgr)
	{
		// Equipment manager not ready this early in possession -> bail; the dual spine re-drives later (OnRep /
		// next possession), the same idempotent re-apply the skin path relies on. Leave tracking so the retry
		// re-resolves.
		return;
	}

	// Resolve the equipment definition BEFORE tearing anything down: a catalog/carrier MISS must NOT strip the
	// current primary (fail SAFE for the weapon, and LOUD in the diag -- mirrors the edge axis' no-silent-ride).
	TSubclassOf<ULyraEquipmentDefinition> EquipDef = nullptr;
	if (WeaponId != NAME_None)
	{
		if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
		{
			// UNIFORM resolution (D1): ResolveAsset -> the UAFLWeaponCosmeticAsset carrier -> EquipmentDefinition.
			if (const UAFLWeaponCosmeticAsset* WeaponAsset =
					Cast<UAFLWeaponCosmeticAsset>(Catalog->ResolveAsset(WeaponId)))
			{
				EquipDef = WeaponAsset->EquipmentDefinition.LoadSynchronous();
			}
		}
		if (!EquipDef)
		{
			if (AFLSkinDiag::IsOn())
			{
				UE_LOG(LogAFLSkinDiag, Warning, TEXT("%s%s : RefreshWeapon weaponId=%s MISS (no carrier/EquipDef) -> primary kept"),
					*AFLSkinDiag::Prefix(this), *Pawn->GetName(), *WeaponId.ToString());
			}
			EquippedWeaponId = WeaponId; // record so we don't re-resolve the same miss every spine tick
			return;
		}
	}

	// THE ONE EQUIP RAIL (Block 34). Every weapon goes through the QuickBar -- no allowlist, no legacy
	// direct-equip branch. The QuickBar owns equip/unequip, which is what makes cycling work, stops a pickup
	// stacking a third weapon, and keeps the held-weapon state single-sourced.
	//
	// The old direct EquipMgr->EquipItem branch that used to live here is DELETED. It equipped outside the
	// QuickBar, so the QuickBar could never unequip what it had not equipped: the mesh stayed in hand while
	// cycling swapped the QuickBar item underneath and the player held two weapons with the wrong one's
	// abilities live (watched on ASTRA). That layer-skip was the whole defect.
	// SPAWN-RACE VOICE (Block 44). DIAGNOSTIC ONLY -- this does NOT gate, bail, or change the equip.
	//
	// FLyraEquipmentList::AddEntry grants the equipment's AbilitySets only `if (ASC)` and its else branch is
	// literally `//@TODO: Warning logging?` -- nothing. SpawnEquipmentActors then runs REGARDLESS. So when
	// this fires before ULyraPawnExtensionComponent has wired the ASC (Lyra keeps it on the PlayerState, and
	// OnPossessedPawnChanged broadcasts from AController::SetPawn which precedes that init), the weapon
	// spawns looking perfect, sits correctly, and has NO fire ability -- with zero trace anywhere. Cycling
	// out and back re-equips once the ASC exists, which is why it then works.
	//
	// That silence is the actual defect being addressed here. The race itself is NOT fixed by this commit --
	// the durable fix is registering as an init-state feature so GameplayReady gates the equip. Until then,
	// equipping anyway is deliberately preserved: an ability-less weapon is better in play than no weapon.
	// PREVIEW-PAWN SUPPRESSION (Block 46). Both warnings below are silenced for a pawn with NO CONTROLLER.
	//
	// Gated on the controller, not on a class name, because no-controller IS the cause: no controller means no
	// ULyraInventoryManagerComponent and no ULyraQuickBarComponent (both are UControllerComponents), so the
	// route legitimately cannot run and the ASC legitimately does not exist. AFLLoadoutDisplayPawn is today's
	// instance, but any future controller-less preview pawn is covered by the same test.
	//
	// This exists because a log that fires every time the front-end opens is a log nobody reads -- which is
	// precisely how the ASC spawn race stayed hidden through several watches. The 7 SPAWN RACE hits in the
	// Block 44 run were 3 real (B_Hero_BagMan_C_0) and 4 preview noise; only the real ones should speak.
	const bool bIsPreviewPawn = (Pawn && Pawn->GetController() == nullptr);

	if (WeaponId != NAME_None && Pawn && !bIsPreviewPawn && !UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
	{
		UE_LOG(LogAFLSkinDiag, Warning,
			TEXT("%s%s : SPAWN RACE -- equipping '%s' with NO AbilitySystemComponent yet. Lyra grants the "
			     "weapon's AbilitySets only when the ASC exists (FLyraEquipmentList::AddEntry) and logs "
			     "nothing when it does not, so this weapon will SPAWN AND LOOK CORRECT BUT CANNOT FIRE until "
			     "it is re-equipped (cycle out and back). Equipping anyway -- behaviour unchanged."),
			*AFLSkinDiag::Prefix(this), *Pawn->GetName(), *WeaponId.ToString());
	}

	if (WeaponId != NAME_None)
	{
		// DISPLAY-ONLY RAIL (operator 08-29, weapon-in-hands preview): a controller-less preview pawn
		// has no Inventory and no QuickBar (both UControllerComponents), so the ONE QuickBar rail
		// legitimately cannot run here -- and the ASTRA layer-skip defect cannot occur either, because
		// there is no QuickBar to disagree with. Direct equip, tracked in SelectedWeaponInstance,
		// replaced on every change. REAL pawns never enter this branch.
		if (bIsPreviewPawn)
		{
			// REPLACE BY SWEEP, not by tracked handle: the tracking is per-CONTROLLER and the spine
			// alternates pawns (display pawn vs gameplay), so a tracked-handle replace orphans an
			// instance per thrash -- the P34 double-Arclight. Unequipping every ranged instance on
			// THIS manager is idempotent whatever the tracking says.
			for (ULyraEquipmentInstance* Existing :
				EquipMgr->GetEquipmentInstancesOfType(ULyraRangedWeaponInstance::StaticClass()))
			{
				EquipMgr->UnequipItem(Existing);
			}
			SelectedWeaponInstance = EquipMgr->EquipItem(EquipDef);
			EquippedWeaponId = WeaponId;
			if (AFLSkinDiag::IsOn())
			{
				UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : preview DIRECT equip '%s' -> %s"),
					*AFLSkinDiag::Prefix(this), *Pawn->GetName(), *WeaponId.ToString(),
					SelectedWeaponInstance.IsValid() ? *SelectedWeaponInstance->GetName() : TEXT("FAILED"));
			}
			return;
		}
		if (TryEquipWeaponViaQuickBar(WeaponId))
		{
			EquippedWeaponId = WeaponId;
		}
		else
		{
			// PERMANENT NET for a REAL pawn. There is no fallback path any more, so a weapon that cannot
			// route does not equip at all -- this line is the only thing that names it. Reaching here on a
			// controlled pawn means the SKU is mis-authored (not Weapon-typed, or no ItemDefClass), not that
			// the rail is broken.
			//
			// Suppressed on a controller-less preview pawn: the route CANNOT succeed there (inventory and
			// QuickBar are both UControllerComponents), so the failure is expected and says nothing. That
			// accounts for the 4 route-FAILED lines in the Block 44 run, all on AFLLoadoutDisplayPawn_0.
			UE_CLOG(!bIsPreviewPawn, LogAFLSkinDiag, Warning,
				TEXT("%s%s : QuickBar route FAILED for '%s' -- weapon NOT equipped. Check the catalog row is "
				     "Type==Weapon and carries an ItemDefClass."),
				*AFLSkinDiag::Prefix(this), *Pawn->GetName(), *WeaponId.ToString());
			EquippedWeaponId = WeaponId;   // record so we do not re-attempt the same bad row every re-drive
		}
		return;
	}

	// DESELECT (WeaponId == NAME_None) -- the QuickBar-side replacement for the deleted else branch. The old
	// one unequipped SelectedWeaponInstance, a handle the rail never sets, so deselecting a routed weapon
	// removed nothing and left it in slot 3 forever.
	if (bIsPreviewPawn)
	{
		// The display rail sweeps its manager clean; there is no QuickBar to clear.
		for (ULyraEquipmentInstance* Existing :
			EquipMgr->GetEquipmentInstancesOfType(ULyraRangedWeaponInstance::StaticClass()))
		{
			EquipMgr->UnequipItem(Existing);
		}
		SelectedWeaponInstance = nullptr;
		EquippedWeaponId = NAME_None;
		return;
	}
	ClearWeaponFromQuickBar();
	QuickBarRoutedItem = nullptr;
	EquippedWeaponId = NAME_None;

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : RefreshWeapon weaponId=%s -> instance=%s"),
			*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
			(WeaponId != NAME_None) ? *WeaponId.ToString() : TEXT("<none>"),
			SelectedWeaponInstance.IsValid() ? *SelectedWeaponInstance->GetName() : TEXT("none"));
	}

	// [WEAPON-POS DIAG] where did the weapon actually spawn/attach? (hand vs base) -- the store "no weapon visible"
	// probe. Logs the spawned weapon actor's world Z + its attach parent + socket, so ONE PIE tells us whether the
	// gun is at the hand (socket weapon_r) or dumped at the pawn origin (null owner mesh -> the char-parts reset
	// gotcha: ApplyDrivingMesh must re-apply SKM_Manny_Invis so weapon_r resolves).
	if (AFLSkinDiag::IsOn() && SelectedWeaponInstance.IsValid())
	{
		for (AActor* WA : SelectedWeaponInstance->GetSpawnedActors())
		{
			if (!WA) { continue; }
			const USceneComponent* Root = WA->GetRootComponent();
			const USceneComponent* AttachParent = Root ? Root->GetAttachParent() : nullptr;
			UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : WEAPON-POS actor=%s worldZ=%.1f attachParent=%s socket=%s"),
				*AFLSkinDiag::Prefix(this), *Pawn->GetName(), *WA->GetName(), WA->GetActorLocation().Z,
				AttachParent ? *AttachParent->GetName() : TEXT("<none>"),
				Root ? *Root->GetAttachSocketName().ToString() : TEXT("<none>"));
		}
	}
}

bool UAFLSkinColorControllerComponent::TryEquipWeaponViaQuickBar(FName WeaponId)
{
	AController* OwningController = GetController<AController>();
	if (!OwningController)
	{
		return false;
	}

	// 1. id -> the Lyra ITEM-def class. ResolveWeaponItemDefClass is type-gated to Weapon rows and returns
	//    null when the row carries no ItemDefClass, so a mis-authored SKU fails here rather than silently
	//    granting nothing. (It hands back UClass* because AFLCosmeticCore does not link LyraGame.)
	const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this);
	UClass* ItemDefClass = Catalog ? Catalog->ResolveWeaponItemDefClass(WeaponId) : nullptr;
	if (!ItemDefClass)
	{
		return false;
	}

	// 2. Grant the item into the CONTROLLER's inventory. AddItemDefinition is UE_API-exported -> a direct
	//    C++ call is safe (the same call UAFLAG_GrantLoadout makes).
	ULyraInventoryManagerComponent* Inventory = OwningController->FindComponentByClass<ULyraInventoryManagerComponent>();
	if (!Inventory)
	{
		return false;
	}
	ULyraInventoryItemInstance* Instance =
		Inventory->AddItemDefinition(TSubclassOf<ULyraInventoryItemDefinition>(ItemDefClass), /*StackCount=*/1);
	if (!Instance)
	{
		return false;
	}

	// 3. Find the QuickBar by CLASS NAME. ULyraQuickBarComponent has no LYRAGAME_API export, so this module
	//    cannot name the type at all -- walk the super-chain by name, exactly as the CharacterParts selector
	//    does for its unexported stock component. B_QuickBarComponent is a BP subclass, so a leaf-name match
	//    would miss; the super-chain walk is load-bearing, not defensive.
	UActorComponent* QuickBar = nullptr;
	TInlineComponentArray<UActorComponent*> Comps(OwningController);
	for (UActorComponent* Comp : Comps)
	{
		if (!Comp) { continue; }
		for (const UClass* C = Comp->GetClass(); C; C = C->GetSuperClass())
		{
			if (C->GetName().Contains(TEXT("LyraQuickBarComponent")))
			{
				QuickBar = Comp;
				break;
			}
		}
		if (QuickBar) { break; }
	}
	if (!QuickBar)
	{
		return false;
	}

	UFunction* RemoveFn = QuickBar->FindFunction(FName(TEXT("RemoveItemFromSlot")));
	UFunction* AddFn    = QuickBar->FindFunction(FName(TEXT("AddItemToSlot")));
	UFunction* SetFn    = QuickBar->FindFunction(FName(TEXT("SetActiveSlotIndex")));
	if (!AddFn || !SetFn)
	{
		return false;
	}

	// 4. REMOVE-then-ADD on our one designated slot. AddItemToSlot no-ops SILENTLY on an occupied slot, so
	//    the remove is what makes a re-drive (respawn, nudge, re-selection) idempotent instead of a no-op.
	//    ⚠ RemoveItemFromSlot RETURNS a value -- the return occupies a trailing member of the ProcessEvent
	//    arg struct. Omitting it corrupts the stack frame.
	if (RemoveFn)
	{
		struct FRemoveItemFromSlotArgs { int32 SlotIndex; ULyraInventoryItemInstance* ReturnValue; };
		FRemoveItemFromSlotArgs RemoveArgs{ CosmeticWeaponQuickBarSlot, nullptr };
		QuickBar->ProcessEvent(RemoveFn, &RemoveArgs);
	}

	struct FAddItemToSlotArgs { int32 SlotIndex; ULyraInventoryItemInstance* Item; };
	FAddItemToSlotArgs AddArgs{ CosmeticWeaponQuickBarSlot, Instance };
	QuickBar->ProcessEvent(AddFn, &AddArgs);

	// 5. Make it the held weapon. ⚠ SetActiveSlotIndex is UFUNCTION(Server, Reliable) -- NOT symmetric with
	//    the two above. We are on the authority here (RefreshWeaponForPawn's HasAuthority gate), so
	//    ProcessEvent runs _Implementation locally rather than dispatching an RPC.
	struct FSetActiveSlotIndexArgs { int32 NewIndex; };
	FSetActiveSlotIndexArgs SetArgs{ CosmeticWeaponQuickBarSlot };
	QuickBar->ProcessEvent(SetFn, &SetArgs);

	// IDEMPOTENCY HANDLE -- see QuickBarRoutedItem's comment. Without this the guard at the top of
	// RefreshWeaponForPawn never short-circuits for a routed weapon and every respawn grants another item.
	QuickBarRoutedItem = Instance;

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log,
			TEXT("%sQuickBar route: %s -> item=%s slot=%d (active)"),
			*AFLSkinDiag::Prefix(this), *WeaponId.ToString(), *GetNameSafe(Instance), CosmeticWeaponQuickBarSlot);
	}
	return true;
}

void UAFLSkinColorControllerComponent::ClearWeaponFromQuickBar()
{
	AController* OwningController = GetController<AController>();
	if (!OwningController)
	{
		return;
	}

	// Same class-NAME super-chain walk as the equip side -- ULyraQuickBarComponent has no LYRAGAME_API
	// export, so this module cannot name the type.
	UActorComponent* QuickBar = nullptr;
	TInlineComponentArray<UActorComponent*> Comps(OwningController);
	for (UActorComponent* Comp : Comps)
	{
		if (!Comp) { continue; }
		for (const UClass* C = Comp->GetClass(); C; C = C->GetSuperClass())
		{
			if (C->GetName().Contains(TEXT("LyraQuickBarComponent"))) { QuickBar = Comp; break; }
		}
		if (QuickBar) { break; }
	}
	if (!QuickBar)
	{
		return;
	}

	UFunction* GetActiveFn = QuickBar->FindFunction(FName(TEXT("GetActiveSlotIndex")));
	UFunction* RemoveFn    = QuickBar->FindFunction(FName(TEXT("RemoveItemFromSlot")));
	UFunction* SetFn       = QuickBar->FindFunction(FName(TEXT("SetActiveSlotIndex")));
	if (!RemoveFn)
	{
		return;
	}

	// Was OUR slot the one being held? Ask BEFORE removing -- afterwards ActiveSlotIndex is already -1 and
	// the answer is unrecoverable. (Return value occupies a trailing arg-struct member.)
	bool bWasActive = false;
	if (GetActiveFn)
	{
		struct FGetActiveSlotIndexArgs { int32 ReturnValue; };
		FGetActiveSlotIndexArgs GetArgs{ INDEX_NONE };
		QuickBar->ProcessEvent(GetActiveFn, &GetArgs);
		bWasActive = (GetArgs.ReturnValue == CosmeticWeaponQuickBarSlot);
	}

	struct FRemoveItemFromSlotArgs { int32 SlotIndex; ULyraInventoryItemInstance* ReturnValue; };
	FRemoveItemFromSlotArgs RemoveArgs{ CosmeticWeaponQuickBarSlot, nullptr };
	QuickBar->ProcessEvent(RemoveFn, &RemoveArgs);

	// ⚠ THE EMPTY-HAND CASE. RemoveItemFromSlot, when the removed slot is active, calls UnequipItemInSlot()
	// and sets ActiveSlotIndex = -1 -- and NOTHING re-equips, so the player is left holding nothing with no
	// slot selected (mouse-wheel cycling from -1 also behaves oddly). Re-activate slot 0, which is the
	// loadout's first weapon. Only when our slot WAS active: otherwise this would yank the player off a
	// weapon they were deliberately holding.
	if (bWasActive && SetFn)
	{
		struct FSetActiveSlotIndexArgs2 { int32 NewIndex; };
		FSetActiveSlotIndexArgs2 SetArgs{ 0 };
		QuickBar->ProcessEvent(SetFn, &SetArgs);
	}

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log,
			TEXT("%sQuickBar deselect: cleared slot %d (wasActive=%d -> %s)"),
			*AFLSkinDiag::Prefix(this), CosmeticWeaponQuickBarSlot, bWasActive ? 1 : 0,
			bWasActive ? TEXT("re-activated slot 0") : TEXT("active slot untouched"));
	}
}

void UAFLSkinColorControllerComponent::RefreshHandCannonsForPawn(APawn* Pawn, FName RightWeaponId, FName LeftWeaponId)
{
	// DUAL-MOUNT body (Hand-Cannon line). Reached ONLY from RefreshWeaponForPawn when LeftWeaponId != NAME_None,
	// AFTER its authority gate + bot skip + new-pawn reset -- so Pawn is valid, we are the server, this is a player
	// pawn, and WeaponTrackedPawn == Pawn (tracking is fresh on a respawn). This holds BOTH cannons at once (D2
	// both-at-once / D3 one-trigger-per-hand): a TARGETED unequip that keeps our two tracked instances, then an
	// independent equip/replace per hand. NOTHING here runs for single-held guns -- they never carry a LeftWeaponId.
	ULyraEquipmentManagerComponent* EquipMgr = Pawn->FindComponentByClass<ULyraEquipmentManagerComponent>();
	if (!EquipMgr)
	{
		// Equipment manager not ready this early in possession -> bail; the spine re-drives later (OnRep / next
		// possession), the same idempotent re-apply the single path relies on. Leave tracking so the retry re-resolves.
		return;
	}

	// Resolve BOTH definitions BEFORE tearing anything down (fail SAFE + LOUD, exactly like the single path): a
	// per-hand catalog/carrier MISS keeps that hand's current cannon and never strips the other hand.
	auto ResolveEquipDef = [this](FName WeaponId) -> TSubclassOf<ULyraEquipmentDefinition>
	{
		if (WeaponId == NAME_None)
		{
			return nullptr;
		}
		if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
		{
			// UNIFORM resolution (D1): ResolveAsset -> the UAFLWeaponCosmeticAsset carrier -> EquipmentDefinition.
			if (const UAFLWeaponCosmeticAsset* WeaponAsset =
					Cast<UAFLWeaponCosmeticAsset>(Catalog->ResolveAsset(WeaponId)))
			{
				return WeaponAsset->EquipmentDefinition.LoadSynchronous();
			}
		}
		return nullptr;
	};
	const TSubclassOf<ULyraEquipmentDefinition> RightDef = ResolveEquipDef(RightWeaponId);
	const TSubclassOf<ULyraEquipmentDefinition> LeftDef = ResolveEquipDef(LeftWeaponId);

	// DISPLAY-ONLY RAIL (operator 08-29, akimbo preview -- mirrors the single path's). On a
	// controller-less preview pawn the per-CONTROLLER tracked handles may point at OTHER pawns'
	// instances (the thrash behind the P34 double-equip), so the targeted-unequip/idempotency
	// guards below mis-key. Sweep THIS manager's ranged set and equip both hands fresh --
	// idempotent and pawn-local, exactly what a preview needs. The sockets stay DATA on each
	// cannon's EquipmentDefinition, so left/right land correctly with no extra plumbing.
	if (Pawn->GetController() == nullptr)
	{
		for (ULyraEquipmentInstance* Existing :
			EquipMgr->GetEquipmentInstancesOfType(ULyraRangedWeaponInstance::StaticClass()))
		{
			if (Existing) { EquipMgr->UnequipItem(Existing); }
		}
		SelectedWeaponInstance     = (RightDef != nullptr) ? EquipMgr->EquipItem(RightDef) : nullptr;
		SelectedLeftWeaponInstance = (LeftDef  != nullptr) ? EquipMgr->EquipItem(LeftDef)  : nullptr;
		EquippedWeaponId = RightWeaponId;
		EquippedLeftWeaponId = LeftWeaponId;
		if (AFLSkinDiag::IsOn())
		{
			UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : preview DUAL equip R='%s' L='%s' -> R=%s L=%s"),
				*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
				*RightWeaponId.ToString(), *LeftWeaponId.ToString(),
				SelectedWeaponInstance.IsValid() ? TEXT("ok") : TEXT("none"),
				SelectedLeftWeaponInstance.IsValid() ? TEXT("ok") : TEXT("none"));
		}
		return;
	}

	// TARGETED UNEQUIP -- the divergence from the single path's D2 "unequip ALL". Drop every ranged instance that
	// is NOT one of our two tracked cannons: this removes the hero default primary + any stale prior selection, but
	// KEEPS the other hand alive so equipping/refreshing one cannon never tears down the other (D2 coexist, not
	// replace). The attach socket (weapon_lowerarm_r / _l) is DATA on each cannon's EquipmentDefinition (AIK) -- not
	// set here; both hands share this one equip rail.
	for (ULyraEquipmentInstance* Existing : EquipMgr->GetEquipmentInstancesOfType(ULyraRangedWeaponInstance::StaticClass()))
	{
		if (!Existing)
		{
			continue;
		}
		if (Existing == SelectedWeaponInstance.Get() || Existing == SelectedLeftWeaponInstance.Get())
		{
			continue; // one of ours -> keep (this is exactly what makes the two coexist)
		}
		EquipMgr->UnequipItem(Existing);
	}

	// RIGHT hand -- (re)equip when the id changed OR the tracked instance went stale (self-heal parity with the
	// single path). A resolve MISS records the id (no re-resolve churn beyond the warning) and keeps whatever holds.
	if (RightWeaponId != EquippedWeaponId || (RightWeaponId != NAME_None && !SelectedWeaponInstance.IsValid()))
	{
		if (RightDef)
		{
			if (SelectedWeaponInstance.IsValid())
			{
				EquipMgr->UnequipItem(SelectedWeaponInstance.Get());
			}
			SelectedWeaponInstance = EquipMgr->EquipItem(RightDef);
			EquippedWeaponId = RightWeaponId;
		}
		else if (RightWeaponId != NAME_None) // MISS -> keep current hand, don't strip
		{
			if (AFLSkinDiag::IsOn())
			{
				UE_LOG(LogAFLSkinDiag, Warning, TEXT("%s%s : RefreshHandCannons R weaponId=%s MISS (no carrier/EquipDef) -> hand kept"),
					*AFLSkinDiag::Prefix(this), *Pawn->GetName(), *RightWeaponId.ToString());
			}
			EquippedWeaponId = RightWeaponId;
		}
	}

	// LEFT hand -- symmetric to the right, against the parallel left-hand tracking.
	if (LeftWeaponId != EquippedLeftWeaponId || (LeftWeaponId != NAME_None && !SelectedLeftWeaponInstance.IsValid()))
	{
		if (LeftDef)
		{
			if (SelectedLeftWeaponInstance.IsValid())
			{
				EquipMgr->UnequipItem(SelectedLeftWeaponInstance.Get());
			}
			SelectedLeftWeaponInstance = EquipMgr->EquipItem(LeftDef);
			EquippedLeftWeaponId = LeftWeaponId;
		}
		else if (LeftWeaponId != NAME_None) // MISS -> keep current hand, don't strip
		{
			if (AFLSkinDiag::IsOn())
			{
				UE_LOG(LogAFLSkinDiag, Warning, TEXT("%s%s : RefreshHandCannons L weaponId=%s MISS (no carrier/EquipDef) -> hand kept"),
					*AFLSkinDiag::Prefix(this), *Pawn->GetName(), *LeftWeaponId.ToString());
			}
			EquippedLeftWeaponId = LeftWeaponId;
		}
	}

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : RefreshHandCannons R=%s L=%s -> Rinst=%s Linst=%s"),
			*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
			(RightWeaponId != NAME_None) ? *RightWeaponId.ToString() : TEXT("<none>"),
			(LeftWeaponId != NAME_None) ? *LeftWeaponId.ToString() : TEXT("<none>"),
			SelectedWeaponInstance.IsValid() ? *SelectedWeaponInstance->GetName() : TEXT("none"),
			SelectedLeftWeaponInstance.IsValid() ? *SelectedLeftWeaponInstance->GetName() : TEXT("none"));
	}
}

void UAFLSkinColorControllerComponent::RefreshWeaponSkinForPawn(APawn* Pawn) const
{
	// INDEPENDENT WeaponSkin axis consumer (aligned 2026-07-03). MIRRORS RefreshFacemaskForPawn's resolve+push
	// shape. The weapon skin is its OWN owned item (FAFLCosmeticSelection.WeaponSkinId, "AFL.WeaponSkin.<Pattern>.
	// <Color>") -- it applies to ANY equipped weapon, OVERRIDING the weapon's baked ORIGINAL color. This REPLACES
	// the retired per-weapon WeaponId ".<Color>" suffix coupling (a skin was wrongly a weapon property; now it is
	// an independent item, exactly like Beam/Facemask/Edge). Push the resolved MI to the pawn component's
	// replicated WeaponSkin so all clients converge via OnRep_WeaponSkin. Runs beside RefreshWeaponForPawn /
	// RefreshBeamColorForPawn on the spine. NAME_None / unresolved -> null (weapon keeps its baked original).
	if (!Pawn)
	{
		return;
	}

	// Read the selection off the PAWN's PlayerState first (respawn-race-safe -- the same PS resolution
	// RefreshWeaponForPawn / RefreshFacemaskForPawn use), falling back to the controller's.
	const APlayerState* PawnPS = Pawn->GetPlayerState();
	const AController* OwningController = GetController<AController>();
	const APlayerState* CtrlPS = OwningController ? OwningController->PlayerState : nullptr;
	const APlayerState* SelectionPS = PawnPS ? PawnPS : CtrlPS;

	// STORE PREVIEW: the effective selection is the preview override if set, else the committed loadout.
	const FAFLCosmeticSelection* EffSel = GetEffectiveSelection(SelectionPS);
	const FName WeaponSkinId = EffSel ? EffSel->WeaponSkinId : NAME_None;

	// "AFL.WeaponSkin.<Pattern>.<Color>" -> MI_AFL_WeaponSkin_<Pattern>_<Color>. token[2]=pattern (e.g. NeonCamo),
	// token[3]=color. Fewer than 4 tokens -> no override. The MI is triplanar (weapon-agnostic) -> it lands on
	// ANY equipped weapon's mesh; the SKU is independent of which weapon is held.
	UMaterialInstanceConstant* SkinMIC = nullptr;
	if (WeaponSkinId != NAME_None)
	{
		TArray<FString> Tokens;
		WeaponSkinId.ToString().ParseIntoArray(Tokens, TEXT("."));
		if (Tokens.Num() >= 4)
		{
			const FString& Pattern = Tokens[2];
			const FString& Color = Tokens[3];
			const FString MIPath = FString::Printf(
				TEXT("/Game/Weapons/AFL/Skins/MI_AFL_WeaponSkin_%s_%s.MI_AFL_WeaponSkin_%s_%s"),
				*Pattern, *Color, *Pattern, *Color);
			// Composed at runtime from the skin id, so Tools/AFL_Lint/cook_soft_refs.py cannot
			// resolve it -- it can only see the "MI_AFL_WeaponSkin_%s_%s" stem. Enrol the
			// concrete path so the next validation sweep reports it by name if the cook dropped
			// it. This is the case the build-time gate structurally cannot cover.
			FAFLCookedAssetRegistry::RegisterDynamic(
				MIPath, TEXT("AFLSkinColorControllerComponent::RefreshWeaponSkin"));
			SkinMIC = Cast<UMaterialInstanceConstant>(FSoftObjectPath(MIPath).TryLoad());
		}
	}

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : RefreshWeaponSkin weaponSkinId=%s -> mic=%s"),
			*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
			(WeaponSkinId != NAME_None) ? *WeaponSkinId.ToString() : TEXT("<none>"),
			SkinMIC ? *SkinMIC->GetName() : TEXT("null"));
	}

	if (UAFLSkinColorComponent* PawnComp = Pawn->FindComponentByClass<UAFLSkinColorComponent>())
	{
		// Authority -> replicated WeaponSkin -> all clients apply via OnRep_WeaponSkin (mirrors SetFacemask).
		PawnComp->SetWeaponSkin(SkinMIC);
	}
}

void UAFLSkinColorControllerComponent::RefreshBeamColorForPawn(APawn* Pawn) const
{
	// INDEPENDENT BeamId axis consumer (the 3rd axis: weapon + weapon-skin + beam). Mirrors RefreshWeaponSkinForPawn's
	// resolve+push shape, but reads BeamId (NOT the WeaponId suffix) and resolves it the SAME way Edge/Body/Facemask
	// resolve -- catalog -> UAFLSkinColorAsset. The asset (its ColorParameters["BeamColor"] tint) is pushed to the
	// pawn component's replicated BeamColor so all clients converge (OnRep_BeamColor -> ApplyBeamColorToEquipped ->
	// reflection-write LaserTintColor). A beam is its OWN owned item and applies to ANY equipped weapon (special-gun-
	// locked excepted in the apply). NAME_None / unresolved -> null push (the weapon keeps its default beam).
	if (!Pawn)
	{
		return;
	}

	// Read the selection off the PAWN's PlayerState first (respawn-race-safe -- the same PS resolution the other
	// consumers use), falling back to the controller's.
	const APlayerState* PawnPS = Pawn->GetPlayerState();
	const AController* OwningController = GetController<AController>();
	const APlayerState* CtrlPS = OwningController ? OwningController->PlayerState : nullptr;
	const APlayerState* SelectionPS = PawnPS ? PawnPS : CtrlPS;

	// STORE PREVIEW: the effective selection is the preview override if set, else the committed loadout.
	const FAFLCosmeticSelection* EffSel = GetEffectiveSelection(SelectionPS);
	const FName BeamId = EffSel ? EffSel->BeamId : NAME_None;

	// Resolve BeamId -> the beam-color SKU (a UAFLSkinColorAsset) via the catalog -- the SAME resolve as Edge/Body.
	UAFLSkinColorAsset* BeamAsset = nullptr;
	if (BeamId != NAME_None)
	{
		if (const UAFLCosmeticCatalogSubsystem* Catalog = UAFLCosmeticCatalogSubsystem::Get(this))
		{
			BeamAsset = Cast<UAFLSkinColorAsset>(Catalog->ResolveAsset(BeamId));
		}
	}

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : RefreshBeamColor beamId=%s -> asset=%s"),
			*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
			(BeamId != NAME_None) ? *BeamId.ToString() : TEXT("<none>"),
			BeamAsset ? *BeamAsset->GetName() : TEXT("null"));
	}

	if (UAFLSkinColorComponent* PawnComp = Pawn->FindComponentByClass<UAFLSkinColorComponent>())
	{
		// Authority -> replicated BeamColor -> all clients apply via OnRep_BeamColor (mirrors SetWeaponSkin/SetFacemask).
		PawnComp->SetBeamColor(BeamAsset);
	}
}
