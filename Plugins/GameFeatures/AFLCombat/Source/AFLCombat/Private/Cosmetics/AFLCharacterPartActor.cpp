// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLCharacterPartActor.h"

#include "AFLCombat.h"                             // LogAFLCombat (the UV1 variant SkinDiag lines; unity-shuffle-proof)
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Cosmetics/AFLSkinColorComponent.h"
#include "Cosmetics/AFLSkinColorControllerComponent.h"
#include "AFLColorIdentityRegistry.h"        // FAFLColorIdentity / FAFLSkinFinish -- the resolve target (AFLCosmeticCore)
#include "AFLCosmeticCatalogSubsystem.h"     // UAFLCosmeticCatalogSubsystem::ResolveColorIdentity (tag -> identity)
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CheatManagerDefines.h"        // UE_WITH_CHEAT_MANAGER guard for the panel-watch DebugSetMID* (undefined macro would silently compile them out -> cheat link error)
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"   // ApplyFacemask param type (the slot-1 base MIC the facemask swaps in)
#include "Materials/MaterialInterface.h"           // UMaterialInterface (slot base type used in the swap/restore)
#include "Misc/PackageName.h"                      // FPackageName::GetLongPackagePath (UV1 visor-variant path resolve)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCharacterPartActor)

void AAFLCharacterPartActor::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	// Byte-identical to ALyraTaggedActor: feeds the reparented robots' Cosmetic.* tags to Lyra's AnimBP
	// (animation-style + body-proportion selection) via IGameplayTagAssetInterface.
	TagContainer.AppendTags(StaticGameplayTags);
}

void AAFLCharacterPartActor::BeginPlay()
{
	Super::BeginPlay();

	// PATH 1 of 2 (covers PART-ARRIVES-SECOND): resolve the owning pawn's color component and self-apply
	// the CURRENT color. The part is a child-actor attached to the pawn (spawned by the pawn's CharacterParts
	// component), so the pawn + its UAFLSkinColorComponent are guaranteed present before this BeginPlay.
	// If SkinColor's VALUE hasn't replicated yet (color-arrives-second), GetSkinColor() is null -> we no-op
	// here, and the component's OnRep-push (PATH 2) applies when the color arrives. BOTH paths are required.
	AActor* PawnActor = GetParentActor();
	const UAFLSkinColorComponent* ColorComp = PawnActor ? PawnActor->FindComponentByClass<UAFLSkinColorComponent>() : nullptr;

	// #38a PART-ARRIVAL RE-RESOLVE (authority-only, additive trigger): a runtime robot swap
	// (ReplaceCharacterPart) spawns a NEW branded part but does NOT re-fire the controller's possess-time
	// resolve, so the pawn would keep the PREVIOUS robot's edge. Here -- now that THIS (possibly-new) part
	// exists and its Cosmetic.Brand.* tag is readable -- ask the controller to re-resolve THIS pawn's brand
	// edge and re-push it. The controller's RefreshSkinForPawn is the SAME resolve+push body the possess
	// path uses (no duplicated logic) and drives the SAME SetSkinColor route (propagation unchanged); it
	// re-applies to all parts (incl. this one). Every deref guarded -> no-op (never crash) if any link is
	// absent. On a normal possess this fires harmlessly alongside the possess-time resolve (idempotent).
	if (PawnActor && PawnActor->HasAuthority())
	{
		if (APawn* OwningPawn = Cast<APawn>(PawnActor))
		{
			if (AController* OwningController = OwningPawn->GetController())
			{
				if (const UAFLSkinColorControllerComponent* SkinCtrl =
						OwningController->FindComponentByClass<UAFLSkinColorControllerComponent>())
				{
					SkinCtrl->RefreshSkinForPawn(OwningPawn);
				}
			}
		}
	}

	// Capture the color AFTER the authority re-resolve above, so PATH 1 applies the freshly-resolved brand
	// edge (not a stale pre-resolve value). On non-authority / no-controller this is unchanged behavior.
	const UAFLSkinColorAsset* ResolvedColor = ColorComp ? ColorComp->GetSkinColor() : nullptr;

	if (AFLSkinDiag::IsOn())
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s/%s : BeginPlay resolve: pawn=%s colorComp=%s color=%s"),
			*AFLSkinDiag::Prefix(this),
			PawnActor ? *PawnActor->GetName() : TEXT("<none>"), *GetName(),
			PawnActor ? TEXT("y") : TEXT("n"),
			ColorComp ? TEXT("y") : TEXT("n"),
			ResolvedColor ? *ResolvedColor->GetName() : TEXT("null"));
	}

	if (ColorComp)
	{
		// PATH 1 (facemask): self-apply the CURRENT facemask FIRST (slot-1 swap), mirroring the possess
		// composition order (facemask then skin). The facemask previously had ONLY PATH 2 (the pawn component's
		// OnRep-push to already-spawned parts) -- a part that spawned AFTER the facemask replicated on a client
		// was MISSED (no self-apply to catch it) -> bare head on THAT machine while the other showed the visor
		// (the per-window asymmetry: the visor is on the head where it shows, and drops with the severed head).
		// This closes the race exactly as the color PATH 1 below does for SkinColor. GetFacemask() null (not set
		// yet) -> ApplyFacemask restores authored slot-1; the later OnRep (PATH 2) swaps the visor in.
		ApplyFacemask(ColorComp->GetFacemask(), ResolvedColor);

		// PATH 1 (composition, Option B): body Finish FIRST (TeamColor + emissive), then the edge (emissive
		// overlays -> edge wins the shared emissive keys; the Finish supplies the TeamColor). Each axis null ->
		// ApplySkinColor early-returns (guard), so an unset axis is a safe no-op. Mirrors the pawn component's
		// ReapplyBodyColorToAllParts apply order so PATH 1 (part-second) and PATH 2 (color-second) agree.
		ApplySkinColor(ColorComp->GetBodyColor()); // body finish (TeamColor axis); null -> guarded no-op
		ApplySkinColor(ResolvedColor);             // edge overlays (emissive); null -> guarded no-op
	}
}

void AAFLCharacterPartActor::ApplySkinColor(const UAFLSkinColorAsset* ColorAsset)
{
	const bool bDiag = AFLSkinDiag::IsOn();
	if (bDiag)
	{
		UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : ApplySkinColor(%s)"),
			*AFLSkinDiag::Prefix(this), *GetName(),
			ColorAsset ? *ColorAsset->GetName() : TEXT("null"));
	}

	if (ColorAsset == nullptr)
	{
		return; // GUARD: no color -> never touch materials -> never create a MID
	}

	// DEFECT-2: record the asset we are painting THIS part's MIDs with -- this IS the finish that ends up on the
	// live runtime MID. The dismember gib color source reads it back (server-side) so a severed head/limb gib
	// reproduces the live part's finish, not the pawn component's possibly-drifted GetSkinColor(). Additive: the
	// param-writing below is byte-unchanged. const_cast: we only ever READ it back (a const-correctness artifact).
	LastAppliedColor = const_cast<UAFLSkinColorAsset*>(ColorAsset);

	// SKIN PALETTE MIGRATION (locked plan section 4, ADDITIVE). Resolve this preset's ColorIdentityTag ONCE ->
	// the registry's full color identity (one identity -> the multi-tone SkinFinish). bIdentityResolved == false
	// (un-tagged preset, OR registry unloaded / tag absent) -> the baked ColorParameters are used below, EXACTLY
	// as before -- the fallback that keeps every un-migrated preset byte-identical. REPLICATION UNTOUCHED: the
	// FLinearColor never crosses the wire -- the selection FName + the resolved UAFLSkinColorAsset* are what
	// replicate; this resolve is LOCAL on every machine (the registry asset is identical everywhere), so the
	// proven selection->resolve->OnRep convergence path stays byte-identical.
	FAFLColorIdentity ResolvedIdentity;
	const bool bIdentityResolved =
		ColorAsset->GetColorIdentityTag().IsValid() &&
		UAFLCosmeticCatalogSubsystem::ResolveColorIdentity(this, ColorAsset->GetColorIdentityTag(), ResolvedIdentity);

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh))
		{
			continue;
		}

		FAFLSkinMIDSlots& Slots = OwnedMIDs.FindOrAdd(Mesh); // our cache for this mesh
		const int32 NumMaterials = Mesh->GetNumMaterials();
		for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
		{
			// FIX 1 (a) -- OWN-YOUR-MID. Use the MID WE created+cached for this slot. We do NOT reuse a
			// foreign MID found in the slot (e.g. one the hit-flash / HitPosition0 path created) -- writing
			// skin params onto someone else's MID, or theirs stomping ours, is the collision we avoid.
			UMaterialInstanceDynamic* MID = Slots.SlotMIDs.FindRef(SlotIndex);

			// RACE B CRITICAL: log the material on the slot BEFORE we touch it. If this is ever the engine
			// default (not the baked MI_<team>_Body / MI_*_Limbs), that is a true unstyled frame. Normally it
			// is the base team MI (baked into the BP SCS, present pre-BeginPlay) -> never default grey.
			if (bDiag)
			{
				const UMaterialInterface* Pre = Mesh->GetMaterial(SlotIndex);
				UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : slot[%d] preexisting mat=%s"),
					*AFLSkinDiag::Prefix(this), *GetName(), SlotIndex,
					Pre ? *Pre->GetName() : TEXT("null"));
			}

			// (Re)create if: we never made one for this slot, OR the slot no longer holds OUR MID (a foreign
			// system replaced it -> we re-establish ours). Otherwise reuse -> create-once, no duplicate, no leak.
			if (!IsValid(MID) || Mesh->GetMaterial(SlotIndex) != MID)
			{
				MID = Mesh->CreateAndSetMaterialInstanceDynamic(SlotIndex); // ENGINE_API
				Slots.SlotMIDs.Add(SlotIndex, MID);                         // cache OURS
				if (bDiag)
				{
					UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : slot[%d] MID CREATED"),
						*AFLSkinDiag::Prefix(this), *GetName(), SlotIndex);
				}
			}
			else if (bDiag)
			{
				UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : slot[%d] MID reused"),
					*AFLSkinDiag::Prefix(this), *GetName(), SlotIndex);
			}
			if (!MID)
			{
				continue;
			}

			for (const TPair<FName, float>& KV : ColorAsset->GetScalars())
			{
				MID->SetScalarParameterValue(KV.Key, KV.Value);
			}
			// COLOR (the migration seam): the preset's GetColors() KEYS still decide WHICH params are written
			// (the param SHAPE -- an Edge preset writes emissive+edge, a Finish preset also writes TeamColor);
			// only the VALUE source swaps. Resolved identity -> the registry tone for that param; un-tagged /
			// unresolved / unknown-key param -> the baked value (byte-identical to before). Scalars + textures
			// (the loops above and below) are SHAPE -- read straight from the preset, untouched.
			for (const TPair<FName, FLinearColor>& KV : ColorAsset->GetColors())
			{
				const FLinearColor* RegistryTone = bIdentityResolved ? ResolvedIdentity.SkinFinish.FindToneForParam(KV.Key) : nullptr;
				MID->SetVectorParameterValue(KV.Key, FVector(RegistryTone ? *RegistryTone : KV.Value));
			}
			for (const TPair<FName, TObjectPtr<UTexture>>& KV : ColorAsset->GetTextures())
			{
				MID->SetTextureParameterValue(KV.Key, KV.Value);
			}
		}
	}
}

void AAFLCharacterPartActor::ApplyFacemask(UMaterialInstanceConstant* FacemaskMIC, const UAFLSkinColorAsset* ColorToReapply)
{
	const bool bDiag = AFLSkinDiag::IsOn();

	// Slot 1 = M_HeadLegs (the head/visor region) on the BagMan robot's SKM_Manny. The facemask is a slot-1
	// base-MATERIAL swap (the proven MI_AFL_FaceMask_Pink path) -- NOT a param spray. We swap on slot 1 of every
	// mesh that has it (the visible CharacterPart SKM_Manny). Slot 0 (M_torso) keeps the body chest material.
	const int32 FacemaskSlot = 1;

	// UV1 VISOR CONTRACT (unique bodies): stock facemask MICs are authored on Manny's UV layout; a unique
	// body's slot-1 is VISOR-ONLY faces carrying the standardized UV1 projection. Swap in the AUTO-DERIVED
	// visor variant (MI_AFL_FaceMask_<X> -> MI_AFL_FaceMaskV_<X>, same folder, generated from the same
	// LogoTexture via M_AFL_FaceMask_Visor @ TexCoord1) so the mask DESIGN renders, not Manny-UV noise.
	// Missing variant -> fall back to the stock MIC (contained tint-only; never blocks the equip).
	if (bUniqueBodyUVs && FacemaskMIC)
	{
		FString VariantName = FacemaskMIC->GetName();
		if (VariantName.ReplaceInline(TEXT("MI_AFL_FaceMask_"), TEXT("MI_AFL_FaceMaskV_")) > 0)
		{
			const FString PackagePath = FPackageName::GetLongPackagePath(FacemaskMIC->GetPackage()->GetName());
			const FString VariantObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *VariantName, *VariantName);
			if (UMaterialInstanceConstant* Variant = LoadObject<UMaterialInstanceConstant>(nullptr, *VariantObjectPath))
			{
				if (bDiag)
				{
					UE_LOG(LogAFLSkinDiag, Display, TEXT("[SkinDiag] ApplyFacemask: unique-body UV1 variant %s"), *VariantName);
				}
				FacemaskMIC = Variant;
			}
			else if (bDiag)
			{
				UE_LOG(LogAFLSkinDiag, Display, TEXT("[SkinDiag] ApplyFacemask: no UV1 variant at %s -- stock MIC fallback"), *VariantObjectPath);
			}
		}
	}

	TArray<UMeshComponent*> Meshes;
	GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (!IsValid(Mesh) || Mesh->GetNumMaterials() <= FacemaskSlot)
		{
			continue; // 1-slot mesh (e.g. an invisible driver) has no head/visor slot -> skip
		}

		// Capture the AUTHORED slot-1 base material ONCE (the pre-swap material), so a later nullptr restores it.
		// We must capture the AUTHORED material, not a runtime MID -> if the current slot mat is one of OUR MIDs,
		// take its Parent; otherwise it is the authored base MI itself.
		if (!AuthoredSlot1Material.Contains(Mesh))
		{
			UMaterialInterface* Cur = Mesh->GetMaterial(FacemaskSlot);
			UMaterialInterface* Authored = Cur;
			if (UMaterialInstanceDynamic* CurMID = Cast<UMaterialInstanceDynamic>(Cur))
			{
				Authored = CurMID->Parent; // the base behind our runtime MID
			}
			AuthoredSlot1Material.Add(Mesh, Authored);
		}

		// SWAP: facemask MIC if equipping, else RESTORE the captured authored base material.
		UMaterialInterface* NewBase =
			FacemaskMIC ? static_cast<UMaterialInterface*>(FacemaskMIC) : AuthoredSlot1Material.FindRef(Mesh).Get();
		if (NewBase)
		{
			Mesh->SetMaterial(FacemaskSlot, NewBase);
		}

		// COMPOSITION: the swap replaced whatever was in slot 1, including OUR cached MID (where the finish's
		// color params lived). FORGET our slot-1 MID so ApplySkinColor below re-MIDs the SWAPPED material and
		// re-pushes the finish params on top -- material swap THEN param re-push, so the finish is never stranded.
		if (FAFLSkinMIDSlots* Slots = OwnedMIDs.Find(Mesh))
		{
			Slots->SlotMIDs.Remove(FacemaskSlot);
		}

		if (bDiag)
		{
			UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : ApplyFacemask slot[%d] -> %s (reapplyColor=%s)"),
				*AFLSkinDiag::Prefix(this), *GetName(), FacemaskSlot,
				NewBase ? *NewBase->GetName() : TEXT("null"),
				ColorToReapply ? *ColorToReapply->GetName() : TEXT("null"));
		}
	}

	// Re-layer the finish color params on top of the swapped material (ApplySkinColor re-creates the slot-1 MID
	// we just dropped + re-pushes). Null color -> ApplySkinColor early-returns (the facemask MIC still shows raw).
	if (ColorToReapply)
	{
		ApplySkinColor(ColorToReapply);
	}
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	// Shared by both SetParam variants: ensure THIS part has an OWN-MID on every slot of every mesh (same
	// create-once/own-your-MID pattern as ApplySkinColor) and hand each MID to the visitor. Returns the
	// number of MID slots visited. Pure helper -- the panel-watch instrument's only job is to set a param.
	template <typename FVisitor>
	int32 ForEachOwnedMID(AActor* Part, TMap<TObjectPtr<UMeshComponent>, FAFLSkinMIDSlots>& OwnedMIDs, FVisitor&& Visit)
	{
		int32 Written = 0;
		TArray<UMeshComponent*> Meshes;
		Part->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (!IsValid(Mesh))
			{
				continue;
			}
			FAFLSkinMIDSlots& Slots = OwnedMIDs.FindOrAdd(Mesh);
			const int32 NumMaterials = Mesh->GetNumMaterials();
			for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
			{
				UMaterialInstanceDynamic* MID = Slots.SlotMIDs.FindRef(SlotIndex);
				if (!IsValid(MID) || Mesh->GetMaterial(SlotIndex) != MID)
				{
					MID = Mesh->CreateAndSetMaterialInstanceDynamic(SlotIndex);
					Slots.SlotMIDs.Add(SlotIndex, MID);
				}
				if (MID)
				{
					Visit(MID);
					++Written;
				}
			}
		}
		return Written;
	}
}

int32 AAFLCharacterPartActor::DebugSetMIDVectorParam(FName ParamName, const FLinearColor& Value)
{
	return ForEachOwnedMID(this, OwnedMIDs, [ParamName, &Value](UMaterialInstanceDynamic* MID)
	{
		MID->SetVectorParameterValue(ParamName, Value);
	});
}

int32 AAFLCharacterPartActor::DebugSetMIDScalarParam(FName ParamName, float Value)
{
	return ForEachOwnedMID(this, OwnedMIDs, [ParamName, Value](UMaterialInstanceDynamic* MID)
	{
		MID->SetScalarParameterValue(ParamName, Value);
	});
}
#endif // UE_WITH_CHEAT_MANAGER
