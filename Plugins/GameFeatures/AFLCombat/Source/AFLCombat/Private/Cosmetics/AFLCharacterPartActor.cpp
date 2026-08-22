// Copyright C12 AI Gaming. All Rights Reserved.

#include "Cosmetics/AFLCharacterPartActor.h"
#include "Cosmetics/AFLCosmeticSelectionTypes.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

#include "AFLCombat.h"                             // LogAFLCombat (the UV1 variant SkinDiag lines; unity-shuffle-proof)
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Cosmetics/AFLSkinColorComponent.h"
#include "Cosmetics/AFLSkinColorControllerComponent.h"
#include "AFLColorIdentityRegistry.h"        // FAFLColorIdentity / FAFLSkinFinish -- the resolve target (AFLCosmeticCore)
#include "AFLCosmeticCatalogSubsystem.h"     // UAFLCosmeticCatalogSubsystem::ResolveColorIdentity (tag -> identity)
#include "Components/DecalComponent.h"       // UDecalComponent -- the chest EMBLEM layer (a USceneComponent, NOT a UMeshComponent)
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
		ApplyFacemask(ColorComp->GetFacemask(), ResolvedColor, ColorComp->GetColorOverride());

		// PATH 1 (composition, Option B): body Finish FIRST (TeamColor + emissive), then the edge (emissive
		// overlays -> edge wins the shared emissive keys; the Finish supplies the TeamColor). Each axis null ->
		// ApplySkinColor early-returns (guard), so an unset axis is a safe no-op. Mirrors the pawn component's
		// ReapplyBodyColorToAllParts apply order so PATH 1 (part-second) and PATH 2 (color-second) agree.
		// PATH 1 (creator overlay): read the pawn-side REPLICATED override, exactly as the two axes below read
		// their replicated assets. Not yet replicated -> invalid -> byte-identical to pre-overlay behaviour;
		// OnRep_ColorOverride (PATH 2) covers the override-arrives-second ordering.
		const FAFLColorOverride& CreatorOverride = ColorComp->GetColorOverride();
		if (AFLSkinDiag::IsOn())
		{
			UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : PATH1 overlay bValid=%d Body=(%.4f,%.4f,%.4f) body=%s edge=%s"),
				*AFLSkinDiag::Prefix(this), *GetName(), CreatorOverride.bValid ? 1 : 0,
				CreatorOverride.BodyColor.R, CreatorOverride.BodyColor.G, CreatorOverride.BodyColor.B,
				ColorComp->GetBodyColor() ? TEXT("set") : TEXT("null"), ResolvedColor ? TEXT("set") : TEXT("null"));
		}

		ApplySkinColor(ColorComp->GetBodyColor(), CreatorOverride); // body finish (TeamColor axis); null -> no-op
		ApplySkinColor(ResolvedColor, CreatorOverride);             // edge overlays (emissive); null -> no-op
	}
}

void AAFLCharacterPartActor::ApplySkinColor(const UAFLSkinColorAsset* ColorAsset, const FAFLColorOverride& ColorOverride)
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

	// GIB TINT FIX (2026-07-25): LastAppliedColor is whatever was applied MOST RECENTLY, and the composition
	// order is body-finish FIRST then edge overlay -- so it is ALWAYS the edge. The dismember gib path read it
	// and painted severed limbs with the player's EDGE colour: a FANATICS body rendering brand red threw
	// NeonBlue limbs (log: "limb gib ... color=DA_AFL_Edge_NeonBlue"). Record the BODY layer separately, by
	// AXIS, so the gib can ask for the layer it actually wants. Body/Finish are the base-colour axes; Edge and
	// Emblem are overlays and must NOT overwrite this.
	const EAFLCosmeticAxis Axis = ColorAsset->GetAxis();
	if (Axis == EAFLCosmeticAxis::Body || Axis == EAFLCosmeticAxis::Finish)
	{
		LastAppliedBodyColor = const_cast<UAFLSkinColorAsset*>(ColorAsset);
	}

	// SKIN PALETTE MIGRATION (locked plan section 4, ADDITIVE). Resolve this preset's ColorIdentityTag ONCE ->
	// the registry's full color identity (one identity -> the multi-tone SkinFinish). bIdentityResolved == false
	// (un-tagged preset, OR registry unloaded / tag absent) -> the baked ColorParameters are used below, EXACTLY
	// as before -- the fallback that keeps every un-migrated preset byte-identical. REPLICATION UNTOUCHED: the
	// FLinearColor never crosses the wire -- the selection FName + the resolved UAFLSkinColorAsset* are what
	// replicate; this resolve is LOCAL on every machine (the registry asset is identical everywhere), so the
	// proven selection->resolve->OnRep convergence path stays byte-identical.
	FAFLColorIdentity ResolvedIdentity;
	bool bIdentityResolved =
		ColorAsset->GetColorIdentityTag().IsValid() &&
		UAFLCosmeticCatalogSubsystem::ResolveColorIdentity(this, ColorAsset->GetColorIdentityTag(), ResolvedIdentity);

	// SPONSOR LOCK (two identity classes). A SPONSOR body (BrandColorIdentityTag set + that registry row
	// flagged bColorLocked) OVERRIDES the incoming preset's identity with its OWN brand identity: the brand
	// color IS the product, so a player color selection must not repaint it. FANATICS_X's red is therefore
	// LOCKED, not merely defaulted. A STANDARD body (tag unset, or the row not flagged) falls through
	// untouched -> the player's chosen color owns every surface, byte-identical to before.
	//
	// Deliberately overrides only the TONES: the preset below still decides SHAPE (which params, which
	// scalars/textures) so a sponsor keeps whatever finish/edge/emblem STRUCTURE the player equipped -- it
	// just renders in brand color. Resolve failure (registry unloaded / tag absent) leaves the player's
	// color intact rather than blanking the part.
	// SCOPED TO A COMPETING IDENTITY (regression fix, PIE 2026-07-25). Require bIdentityResolved: the lock
	// exists to stop a PLAYER COLOR CHOICE from repainting a sponsor, and a player choice ALWAYS arrives as a
	// preset carrying its own ColorIdentityTag. A preset with NO tag is AUTHORED ART whose baked
	// ColorParameters are the intended look (DA_AFL_Finish_GlossBlack, the brand's own body finish, is
	// deliberately untagged) -- overriding that repaints art nobody asked to recolor. First pass omitted this
	// and forced FANATICS tones over GlossBlack's baked values, swapping its two PURPLE accent tones
	// (EmissiveColor2/3) to red. TeamColor + EmissiveColor + EdgeGlowColor were already identical, so it did
	// not turn the body red -- but it flattened the authored accents, which is wrong either way.
	if (bIdentityResolved && BrandColorIdentityTag.IsValid())
	{
		FAFLColorIdentity BrandIdentity;
		if (UAFLCosmeticCatalogSubsystem::ResolveColorIdentity(this, BrandColorIdentityTag, BrandIdentity)
			&& BrandIdentity.bColorLocked)
		{
			ResolvedIdentity = BrandIdentity;
			bIdentityResolved = true;

			if (bDiag)
			{
				UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : SPONSOR LOCK -> forcing identity %s (preset %s ignored for tone)"),
					*AFLSkinDiag::Prefix(this), *GetName(),
					*BrandColorIdentityTag.ToString(), *ColorAsset->GetName());
			}
		}
	}

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

			// PIPING-IS-PIPING GUARD, part 1 -- THE FLOOD DRIVER (measured 2026-07-25 by A/B probe: two bodies,
			// identical colours, ONLY EdgeGlowMagnitude differing -> 0.0 renders gloss-black chassis + clean
			// pipes; 0.8 renders a SOLID colour body with white-cored pipes). On the stock M_Mannequin path
			// EdgeGlowMagnitude is a thin rim; on M_AFL_Character it is a BROAD SURFACE WASH that floods the
			// whole chassis with EdgeGlowColor. MI_AFL_IRONICS_Body authors it 0.0 and DA_AFL_Finish_GlossBlack
			// is 0.0 -- but ALL 13 Edge presets carry 0.8, so equipping ANY edge flooded a unique body.
			// This is the actual cause of every "the whole body is red" report; EmissiveStrength (absent from
			// this master), NeonIntensity, RampBoost and EmissiveColor2/3 were all ruled out by probe.
			// Skipping it leaves the MI's authored 0.0 and does NOT break the edge axis: an Edge preset also
			// writes EmissiveColor, which is what recolours the pipes.
			static const FName NEdgeGlowMag(TEXT("EdgeGlowMagnitude"));
			for (const TPair<FName, float>& KV : ColorAsset->GetScalars())
			{
				if (bUniqueBodyUVs && KV.Key == NEdgeGlowMag)
				{
					continue;
				}
				MID->SetScalarParameterValue(KV.Key, KV.Value);
			}
			// COLOR (the migration seam): the preset's GetColors() KEYS still decide WHICH params are written
			// (the param SHAPE -- an Edge preset writes emissive+edge, a Finish preset also writes TeamColor);
			// only the VALUE source swaps. Resolved identity -> the registry tone for that param; un-tagged /
			// unresolved / unknown-key param -> the baked value (byte-identical to before). Scalars + textures
			// (the loops above and below) are SHAPE -- read straight from the preset, untouched.
			// PIPING-IS-PIPING GUARD, part 2 -- keep EmissiveColor2/3 NEUTRAL on unique bodies (operator ruling).
			// HONEST SCOPE: a later A/B probe (2/3 white vs dim-red vs black, all at EdgeGlowMagnitude 0)
			// rendered IDENTICALLY, so 2/3 are visually INERT on this master and this skip is NOT what fixes the
			// flood -- part 1 above is. It is kept because the ruling is to hold 2/3 neutral on unique bodies,
			// and because a future master rewire could make them live; skipping keeps that future change from
			// silently re-introducing a wash. Stock bodies (M_Mannequin) still get 2/3 -- there they are the
			// genuine secondary/tertiary ramp.
			bool bWroteBaseTint = false;
			static const FName NEmissive2(TEXT("EmissiveColor2"));
			static const FName NEmissive3(TEXT("EmissiveColor3"));
			for (const TPair<FName, FLinearColor>& KV : ColorAsset->GetColors())
			{
				if (bUniqueBodyUVs && (KV.Key == NEmissive2 || KV.Key == NEmissive3))
				{
					continue;
				}
				// CREATOR OVERLAY (CC-2.1): highest-precedence value source, INSIDE the EmissiveColor2/3 skip guards
				// above -> a skipped key stays skipped. Invalid override -> nullptr -> the expression below is
				// byte-identical to before (regression guarantee, by construction). Precedence: override > registry > baked.
				const FLinearColor* OverrideTone = ColorOverride.FindOverrideForParam(KV.Key);
				const FLinearColor* RegistryTone = bIdentityResolved ? ResolvedIdentity.SkinFinish.FindToneForParam(KV.Key) : nullptr;
				const FLinearColor FinalVal = OverrideTone ? *OverrideTone : (RegistryTone ? *RegistryTone : KV.Value);
				MID->SetVectorParameterValue(KV.Key, FVector(FinalVal));
				if (bDiag)
				{
					// PER-KEY PRECEDENCE PROOF. override=miss on a key the player set means some call site dropped the
					// overlay (the CC-2.1 facemask-re-layer defect); this line is what localised it.
					UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s slot=%d mid=%s : key=%s override=%s registry=%s FINAL=(%.4f,%.4f,%.4f)"),
						*AFLSkinDiag::Prefix(this), *GetName(), SlotIndex, *MID->GetName(), *KV.Key.ToString(),
						OverrideTone ? TEXT("HIT") : TEXT("miss"), RegistryTone ? TEXT("HIT") : TEXT("miss"),
						FinalVal.R, FinalVal.G, FinalVal.B);
				}
			}
			// CC-2.2 -- VISOR BaseTint, OVERRIDE-ONLY. Deliberately NOT a key in the preset's ColorParameters map,
			// which is where this was first specified. Measured why not: BaseTint is authored PER MATERIAL INSTANCE
			// and the 71 visor/facemask instances hold THREE distinct values -- (0.006,0.006,0.008) x3 (IRONICS),
			// (0,0,0) x37, (0.040,0.040,0.050) x32 (facemask). A preset key is written on EVERY apply, creator ON or
			// OFF, so seeding the presets to any single value would have changed 69 of 71 instances with
			// bUseCreatorColors == false -- every black visor lifted off black, every facemask darkened. BaseTint is
			// a per-visor-IDENTITY property; the Finish preset is per-body-COLOUR. Putting it in the map moves
			// authorship from the instance to the preset and flattens three authored values into one.
			// Writing it ONLY when the override is valid keeps OFF byte-identical BY CONSTRUCTION (no write at all,
			// so the instance's authored tint stands) rather than by seeding a value that is wrong for most of them.
			// Both slot-1 masters (M_AFL_Visor_Clean, M_AFL_FaceMask_Visor) expose BaseTint, so no branch is needed;
			// M_AFL_Character does NOT expose it, so the same call on slot 0 is an inert no-op.
			static const FName NBaseTint(TEXT("BaseTint"));
			if (const FLinearColor* BaseTintOverride = ColorOverride.FindOverrideForParam(NBaseTint))
			{
				MID->SetVectorParameterValue(NBaseTint, FVector(*BaseTintOverride));
				bWroteBaseTint = true;
			}
			if (bDiag)
			{
				// WRITTEN-KEY LIST, not just values. ABSENT and PRESENT-AT-SEED are different outcomes that a value
				// read alone cannot distinguish -- that ambiguity is exactly what would have let the preset-key
				// approach pass its own regression arm while clobbering 69 instances. Emitting whether the key was
				// written at all makes the creator-OFF arm capable of FAILING, which is the only thing that makes it
				// evidence. Expected: OFF -> baseTintWritten=0 ; ON -> baseTintWritten=1 on slot 1.
				UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s slot=%d mid=%s : WROTEKEYS baseTintWritten=%d overrideValid=%d"),
					*AFLSkinDiag::Prefix(this), *GetName(), SlotIndex, *MID->GetName(),
					bWroteBaseTint ? 1 : 0, ColorOverride.bValid ? 1 : 0);
			}
			for (const TPair<FName, TObjectPtr<UTexture>>& KV : ColorAsset->GetTextures())
			{
				MID->SetTextureParameterValue(KV.Key, KV.Value);
			}
		}
	}

	// EMBLEM UNIFY (the missing third layer). Body + edge + visor all ride MATERIAL SLOTS, so the mesh loop
	// above already resolves them by tag. The chest emblem does NOT: it is a UDecalComponent projection, and
	// UDecalComponent derives from USceneComponent -- NOT UMeshComponent -- so GetComponents<UMeshComponent>
	// never returned it and the emblem kept whatever tint was BAKED into its MI while body/edge moved. That is
	// the entire reason the emblem drifted off the color axis. Gather decals explicitly and write the SAME
	// resolved tone, so one identity choice tints body + edge + visor + emblem together.
	//
	// Only the tint is driven: M_AFL_Branding_Decal's BrandMaskTex (which brand mark) and BrandIntensity stay
	// AUTHORED -- the emblem's SHAPE is the product, its COLOR is the axis. Unresolved identity -> skip
	// entirely, leaving the authored MI exactly as-is (no regression for un-migrated bodies).
	if (bIdentityResolved)
	{
		static const FName NeonColorParam(TEXT("NeonColor"));
		if (const FLinearColor* EmblemTone = ResolvedIdentity.SkinFinish.FindToneForParam(NeonColorParam))
		{
			TArray<UDecalComponent*> Decals;
			GetComponents<UDecalComponent>(Decals);
			for (UDecalComponent* Decal : Decals)
			{
				if (!IsValid(Decal))
				{
					continue;
				}

				// OWN-YOUR-MID, same discipline as the mesh slots above: use the MID WE created for this
				// decal, and re-create it if something else swapped the decal material out from under us.
				UMaterialInstanceDynamic* DecalMID = OwnedDecalMIDs.FindRef(Decal);
				if (!IsValid(DecalMID) || Decal->GetDecalMaterial() != DecalMID)
				{
					DecalMID = Decal->CreateDynamicMaterialInstance();
					OwnedDecalMIDs.Add(Decal, DecalMID);
				}
				if (!DecalMID)
				{
					continue;
				}

				DecalMID->SetVectorParameterValue(NeonColorParam, *EmblemTone);

				if (bDiag)
				{
					UE_LOG(LogAFLSkinDiag, Log, TEXT("%s%s : emblem decal %s NeonColor <- %s"),
						*AFLSkinDiag::Prefix(this), *GetName(), *Decal->GetName(), *EmblemTone->ToString());
				}
			}
		}
	}
}

void AAFLCharacterPartActor::ApplyFacemask(UMaterialInstanceConstant* FacemaskMIC, const UAFLSkinColorAsset* ColorToReapply,
	const FAFLColorOverride& ColorOverride)
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
		// THE FIX: carry the overlay into the re-layer. Without it this call wrote registry/brand tones LAST and
		// undid PATH 1 / PATH 2 (measured: override=miss -> FINAL=(0.03,0.03,0.035)).
		ApplySkinColor(ColorToReapply, ColorOverride);
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

// --- CC-7 STICKER COMPOSITION ------------------------------------------------------------------
void AAFLCharacterPartActor::ApplyStickerSet(const FAFLStickerSet& Set, const UAFLCosmeticCatalogSubsystem* Catalog)
{
	static const FName P_Tex(TEXT("StickerAtlasTex"));
	static const FName P_Scale(TEXT("StickerUVScale"));
	static const FName P_U(TEXT("StickerUOffset"));
	static const FName P_V(TEXT("StickerVOffset"));
	static const FName P_Int(TEXT("StickerIntensity"));

	// Which zones actually carry a sticker, and can the catalog tell us their atlas tile?
	struct FDraw { int32 Zone; int32 Tile; FVector2D Pos; float Scale; float Rot; };
	TArray<FDraw> Draws;
	for (int32 z = 0; z < FAFLStickerSet::ZoneCount && Catalog; ++z)
	{
		const FAFLStickerPlacement* P = Set.Find(static_cast<EAFLStickerZone>(z));
		if (!P || !P->IsSet()) { continue; }
		const FAFLCatalogEntry* Row = Catalog->FindEntry(P->StickerId);
		// A row with no tile (-1) is REFUSED, not defaulted to 0. Tile 0 is a real sticker, so
		// defaulting would silently draw someone else's art on an unconfigured row.
		if (!Row || Row->StickerAtlasTile < 0)
		{
			UE_LOG(LogAFLSkinDiag, Warning,
				TEXT("[Sticker] zone %d -> %s has no atlas tile; skipped (not defaulted to 0)"),
				z, *P->StickerId.ToString());
			continue;
		}
		Draws.Add({ z, Row->StickerAtlasTile, P->Position, P->Scale, P->RotationDegrees });
	}

	if (Draws.Num() == 0)
	{
		// NOTHING EQUIPPED: force the layer off. Leaving a stale intensity would keep the last
		// player's stickers on this pawn after a respawn with none.
		ForEachOwnedMID(this, OwnedMIDs, [](UMaterialInstanceDynamic* MID)
		{
			MID->SetScalarParameterValue(P_Int, 0.0f);
		});
		return;
	}

	UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/BagMan/Characters/Cosmetics/Stickers/T_BagMan_StickerAtlas.T_BagMan_StickerAtlas"));
	if (!Atlas)
	{
		UE_LOG(LogAFLSkinDiag, Warning, TEXT("[Sticker] atlas missing -- layer left OFF."));
		return;
	}

	const int32 RTSize = 1024;
	if (!StickerRT)
	{
		StickerRT = NewObject<UTextureRenderTarget2D>(this);
		StickerRT->RenderTargetFormat = RTF_RGBA8;
		StickerRT->ClearColor = FLinearColor(0, 0, 0, 0);
		StickerRT->bAutoGenerateMips = false;
		StickerRT->InitAutoFormat(RTSize, RTSize);
	}
	UKismetRenderingLibrary::ClearRenderTarget2D(this, StickerRT, FLinearColor(0, 0, 0, 0));

	UCanvas* Canvas = nullptr; FVector2D CanvasSize; FDrawToRenderTargetContext Ctx;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, StickerRT, Canvas, CanvasSize, Ctx);
	if (Canvas)
	{
		const float Cell = RTSize / 3.0f;         // zone grid, matching UV2
		const float Tile = 1.0f / 4.0f;           // atlas grid, 4x4
		for (const FDraw& D : Draws)
		{
			const int32 zc = D.Zone % 3, zr = D.Zone / 3;
			const int32 tc = D.Tile % 4, tr = D.Tile / 4;
			const float Size = Cell * FMath::Clamp(D.Scale, 0.05f, 1.0f);
			// Placement is normalised ZONE space, so it cannot leave the cell by construction.
			const FVector2D Centre(zc * Cell + D.Pos.X * Cell, zr * Cell + D.Pos.Y * Cell);
			Canvas->K2_DrawTexture(Atlas,
				Centre - FVector2D(Size * 0.5f, Size * 0.5f), FVector2D(Size, Size),
				FVector2D(tc * Tile, tr * Tile), FVector2D(Tile, Tile),
				FLinearColor::White, BLEND_Translucent, D.Rot, FVector2D(0.5f, 0.5f));
			UE_LOG(LogAFLSkinDiag, Log,
				TEXT("[Sticker] drew zone=%d tile=%d at cell(%d,%d) pos=(%.2f,%.2f) scale=%.2f rot=%.0f"),
				D.Zone, D.Tile, zc, zr, D.Pos.X, D.Pos.Y, D.Scale, D.Rot);
		}
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Ctx);

	// UV2 ADDRESSES THE RT DIRECTLY: scale 1, offset 0. The tile arithmetic already happened above,
	// which is the whole point of compositing -- the material does not have to know about tiles.
	const int32 Count = Draws.Num();
	ForEachOwnedMID(this, OwnedMIDs, [this, Count](UMaterialInstanceDynamic* MID)
	{
		MID->SetTextureParameterValue(P_Tex, StickerRT);
		MID->SetScalarParameterValue(P_Scale, 1.0f);
		MID->SetScalarParameterValue(P_U, 0.0f);
		MID->SetScalarParameterValue(P_V, 0.0f);
		MID->SetScalarParameterValue(P_Int, 1.0f);
	});
	UE_LOG(LogAFLSkinDiag, Log, TEXT("[Sticker] composited %d sticker(s) -> RT, intensity 1"), Count);
}
