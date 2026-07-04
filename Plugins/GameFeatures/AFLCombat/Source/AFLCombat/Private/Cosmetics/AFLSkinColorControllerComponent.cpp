// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLSkinColorControllerComponent.h"

#include "Components/ChildActorComponent.h"
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
		const TCHAR* SelResolveVia = TEXT("-");
		if (Loadout)
		{
			SelectedEdgeId = Loadout->GetSelection().EdgeId;
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
		if (Loadout)
		{
			SelectedBodyId = Loadout->GetSelection().BodyId;
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
		UAFLSkinColorAsset* EffectiveBody =
			(SelectedBody != nullptr) ? SelectedBody
			: bBrandResolved          ? ResolvedEdge
			: PersistentSkinColor.Get();

		if (AFLSkinDiag::IsOn())
		{
			// Report BOTH axes by name -- the respawn re-proof AND the #1 fresh-spawn gate are read FROM this line.
			const TCHAR* BodyTier =
				(SelectedBody != nullptr) ? TEXT("selection")
				: bBrandResolved          ? TEXT("brand")
				: TEXT("fallback");

			UE_LOG(LogAFLSkinDiag, Log,
				TEXT("%s%s : PushToPawn(dual) brandTag=%s mapSet=%s | EDGE edge=%s (sel=%s) | BODY tier=%s body=%s (selBody=%s via=%s brandDefault=%s persistent=%s)"),
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
				PersistentSkinColor ? *PersistentSkinColor->GetName() : TEXT("null"));
		}

		if (UAFLSkinColorComponent* PawnComp = Pawn->FindComponentByClass<UAFLSkinColorComponent>())
		{
			// Authority -> sets the replicated BodyColor + SkinColor (two DOREPLIFETIME props) -> all clients
			// re-apply via OnRep (PATH 2) + the new pawn's parts self-color on their BeginPlay (PATH 1). The body
			// rides DOREPLIFETIME BodyColor exactly as the edge rides DOREPLIFETIME SkinColor (parallel axes).
			PawnComp->SetBodyColor(EffectiveBody);   // body finish (TeamColor)
			PawnComp->SetSkinColor(EffectiveEdge);   // edge overlay (emissive); null = no edge
		}
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

	const UAFLCosmeticLoadoutComponent* Loadout =
		SelectionPS ? SelectionPS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;

	UMaterialInstanceConstant* FacemaskMIC = nullptr;
	const FName FacemaskId = Loadout ? Loadout->GetSelection().FacemaskId : NAME_None;
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

	// NEW-PAWN RESET (respawn / first possession): the prior pawn's instance died with it -> drop stale tracking
	// so the fresh pawn re-equips clean (and a cross-pawn idempotency false-positive can't skip the equip).
	if (WeaponTrackedPawn.Get() != Pawn)
	{
		WeaponTrackedPawn = Pawn;
		SelectedWeaponInstance = nullptr;
		EquippedWeaponId = NAME_None;
	}

	// Read the selected WeaponId off the PAWN's PlayerState first (respawn-race-safe -- the exact PS resolution
	// RefreshSkinForPawn uses), falling back to the controller's.
	const APlayerState* PawnPS = Pawn->GetPlayerState();
	const AController* OwningController = GetController<AController>();
	const APlayerState* CtrlPS = OwningController ? OwningController->PlayerState : nullptr;
	const APlayerState* SelectionPS = PawnPS ? PawnPS : CtrlPS;

	const UAFLCosmeticLoadoutComponent* Loadout =
		SelectionPS ? SelectionPS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
	const FName WeaponId = Loadout ? Loadout->GetSelection().WeaponId : NAME_None;

	// IDEMPOTENT: already realized this WeaponId on this pawn -> no-op. The dual spine re-runs (possession + OnRep
	// + nudge) MUST NOT re-equip/stack. A dropped instance (id set but the instance went stale) falls through ->
	// self-heals by re-equipping.
	if (WeaponId == EquippedWeaponId && (WeaponId == NAME_None || SelectedWeaponInstance.IsValid()))
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

	if (EquipDef) // a valid, entitled weapon selection -> REPLACE the primary
	{
		// D2 REPLACE (self-contained -- NO AFLHeroComponent coupling, the standing hazard): unequip every
		// currently-equipped weapon (the hero default primary AND any prior selection) so the new selection
		// replaces rather than stacks a second held weapon. Targeted to the ranged-weapon instance type (all AFL
		// weapons derive from it); broaden to ULyraWeaponInstance if a melee weapon ever lands.
		for (ULyraEquipmentInstance* Existing : EquipMgr->GetEquipmentInstancesOfType(ULyraRangedWeaponInstance::StaticClass()))
		{
			if (Existing)
			{
				EquipMgr->UnequipItem(Existing);
			}
		}
		// EQUIP -> Lyra's FLyraEquipmentList fast-array replicates to all clients (OnEquipped -> SpawnedActors).
		// Proven rail; zero new replication code.
		SelectedWeaponInstance = EquipMgr->EquipItem(EquipDef);
	}
	else // WeaponId == NAME_None -> deselect: remove only OUR tracked selection (first cut: no default restore --
	{    // the QuickBar era owns slot restore; the D5 near-horizon).
		if (SelectedWeaponInstance.IsValid())
		{
			EquipMgr->UnequipItem(SelectedWeaponInstance.Get());
		}
		SelectedWeaponInstance = nullptr;
	}
	EquippedWeaponId = WeaponId;

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : RefreshWeapon weaponId=%s -> instance=%s"),
			*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
			(WeaponId != NAME_None) ? *WeaponId.ToString() : TEXT("<none>"),
			SelectedWeaponInstance.IsValid() ? *SelectedWeaponInstance->GetName() : TEXT("none"));
	}
}

void UAFLSkinColorControllerComponent::RefreshWeaponSkinForPawn(APawn* Pawn) const
{
	// MIRRORS RefreshFacemaskForPawn's resolve+push shape, for the weapon COLOR axis. The weapon color rides in
	// the WeaponId SUFFIX ("AFL.Weapon.<W>.<Color>"): RefreshWeaponForPawn consumed the equip-half (resolve ->
	// carrier -> EquipItem); THIS consumes the color-half (parse the suffix -> the NeonCamo MI -> push to the
	// pawn component's replicated WeaponSkin so all clients converge via OnRep_WeaponSkin). Rides the EXISTING
	// FAFLCosmeticSelection WeaponId axis -- completes the #43 generalization, no new axis, no new net code (the
	// WeaponSkin MI replicates on the pawn component exactly like Facemask). Called AFTER RefreshWeaponForPawn on
	// the spine so the weapon is equipped before the color applies. NAME_None / no suffix -> null (baked default).
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

	const UAFLCosmeticLoadoutComponent* Loadout =
		SelectionPS ? SelectionPS->FindComponentByClass<UAFLCosmeticLoadoutComponent>() : nullptr;
	const FName WeaponId = Loadout ? Loadout->GetSelection().WeaponId : NAME_None;

	// Parse the ".<Color>" suffix: "AFL.Weapon.<W>.<Color>" -> the 4th token. No 4th token -> no override.
	UMaterialInstanceConstant* SkinMIC = nullptr;
	if (WeaponId != NAME_None)
	{
		TArray<FString> Tokens;
		WeaponId.ToString().ParseIntoArray(Tokens, TEXT("."));
		if (Tokens.Num() >= 4)
		{
			const FString& Color = Tokens[3];
			const FString MIPath = FString::Printf(
				TEXT("/Game/Weapons/AFL/Skins/MI_AFL_WeaponSkin_NeonCamo_%s.MI_AFL_WeaponSkin_NeonCamo_%s"),
				*Color, *Color);
			SkinMIC = Cast<UMaterialInstanceConstant>(FSoftObjectPath(MIPath).TryLoad());
		}
	}

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : RefreshWeaponSkin weaponId=%s -> mic=%s"),
			*AFLSkinDiag::Prefix(this), *Pawn->GetName(),
			(WeaponId != NAME_None) ? *WeaponId.ToString() : TEXT("<none>"),
			SkinMIC ? *SkinMIC->GetName() : TEXT("null"));
	}

	if (UAFLSkinColorComponent* PawnComp = Pawn->FindComponentByClass<UAFLSkinColorComponent>())
	{
		// Authority -> replicated WeaponSkin -> all clients apply via OnRep_WeaponSkin (mirrors SetFacemask).
		PawnComp->SetWeaponSkin(SkinMIC);
	}
}
