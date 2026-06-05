// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Cosmetics/AFLCosmeticTypes.h"
#include "Engine/DataAsset.h"

#include "AFLSkinColorAsset.generated.h"

class UTexture;

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

	// --- Cosmetic economy metadata (store / wallet / drops) ---

	/** Immutable machine key. Format AFL.<Axis>.<Color>, e.g. AFL.Edge.NeonPurple. NEVER change once shipped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Cosmetic|Identity")
	FName CosmeticId;

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

	/** Cosmetic-metadata getters (mirror the L5 getter style; UPROPERTYs are the contract). */
	FName GetCosmeticId() const { return CosmeticId; }
	const FText& GetDisplayName() const { return DisplayName; }
	EAFLCosmeticRarity GetRarity() const { return Rarity; }
	FName GetCollectionId() const { return CollectionId; }
	EAFLCosmeticAxis GetAxis() const { return Axis; }
	int32 GetVersion() const { return Version; }
};
