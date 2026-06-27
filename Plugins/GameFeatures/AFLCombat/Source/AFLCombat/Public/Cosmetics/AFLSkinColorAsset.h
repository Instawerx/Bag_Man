// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Cosmetics/AFLCosmeticTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "AFLSkinColorAsset.generated.h"

class UTexture;
class UMaterialInstanceConstant;

/**
 * AFL robot-skin COLOR preset (L5). A pure, typed param bag — ZERO reflection.
 *
 * Each color preset (Blue/Green/Purple/Pink/Red) is an instance. Drives the body material's named
 * params: ColorParameters (EmissiveColor/2/3, TeamColor, EdgeGlowColor), ScalarParameters
 * (EmissiveStrength*, EdgeGlowMagnitude), TextureParameters (optional, if a color ever needs a texture).
 *
 * Replaces the earlier ULyraTeamDisplayAsset type-reuse: this is a properly-named BagMan type with
 * C++-typed maps + direct getters, so the per-part apply reads it with NO FindFProperty / runtime
 * reflection lookup that could silently miss. Mark x color stay independent: MARK = which part actor
 * (replicated CharacterParts FastArray); COLOR = which UAFLSkinColorAsset is applied to the part's MIDs.
 */
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLSkinColorAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|SkinColor")
	TMap<FName, float> ScalarParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|SkinColor")
	TMap<FName, FLinearColor> ColorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|SkinColor")
	TMap<FName, TObjectPtr<UTexture>> TextureParameters;

	/** FACEMASK ONLY: the slot-1 base MATERIAL this facemask swaps in (the proven MI_AFL_FaceMask_* MIC). A
	 *  facemask's VISUAL is a slot-1 base-MI swap, NOT the param maps above (which stay empty for masks) -- so
	 *  the wrapper carries the MIC here, and the equip path (UAFLSkinColorComponent::SetFacemask via the
	 *  controller's RefreshFacemaskForPawn) reads it. Null for non-facemask assets (Edge/Body/Finish use the
	 *  param-push path and never set this). A UMaterialInstanceConstant content asset -> replication-safe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|SkinColor")
	TObjectPtr<UMaterialInstanceConstant> FacemaskMaterial = nullptr;

	// --- Cosmetic economy metadata (store / wallet / drops) ---

	/** Immutable machine key. Format AFL.<Axis>.<Color>, e.g. AFL.Edge.NeonPurple. NEVER change once shipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic|Identity")
	FName CosmeticId;

	/** SKIN PALETTE MIGRATION (locked plan section 4): the color identity this preset resolves its COLOR from
	 *  (Cosmetic.Identity.<Name>). When SET + resolvable, ApplySkinColor pulls the tones from the registry (one
	 *  identity -> full multi-tone look) and writes them to the MID INSTEAD of the baked ColorParameters above.
	 *  UNSET (default) -> the baked ColorParameters are used EXACTLY as before -- the fallback that keeps every
	 *  un-migrated preset byte-identical. The preset is thus SHAPE (scalars/textures/which params) + this TAG. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic|Identity", meta = (Categories = "Cosmetic.Identity"))
	FGameplayTag ColorIdentityTag;

	/** Player-facing, localizable. Marketing owns this; safe to change. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic|Economy")
	EAFLCosmeticRarity Rarity = EAFLCosmeticRarity::Common;

	/** Set/season grouping, e.g. Founders, Season_1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic|Economy")
	FName CollectionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic|Identity")
	EAFLCosmeticAxis Axis = EAFLCosmeticAxis::Edge;

	/** Iteration of this cosmetic; bump on a live retune (same CosmeticId, new look). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic|Economy")
	int32 Version = 1;

	/** Direct typed getters -- ZERO reflection at the apply site. */
	const TMap<FName, float>& GetScalars() const { return ScalarParameters; }
	const TMap<FName, FLinearColor>& GetColors() const { return ColorParameters; }
	const TMap<FName, TObjectPtr<UTexture>>& GetTextures() const { return TextureParameters; }

	/** FACEMASK: the slot-1 base MIC this facemask swaps in (null for non-facemask assets). */
	UMaterialInstanceConstant* GetFacemaskMaterial() const { return FacemaskMaterial; }

	/** Cosmetic-metadata getters (mirror the L5 getter style; UPROPERTYs are the contract). */
	FName GetCosmeticId() const { return CosmeticId; }
	const FGameplayTag& GetColorIdentityTag() const { return ColorIdentityTag; }
	const FText& GetDisplayName() const { return DisplayName; }
	EAFLCosmeticRarity GetRarity() const { return Rarity; }
	FName GetCollectionId() const { return CollectionId; }
	EAFLCosmeticAxis GetAxis() const { return Axis; }
	int32 GetVersion() const { return Version; }
};
