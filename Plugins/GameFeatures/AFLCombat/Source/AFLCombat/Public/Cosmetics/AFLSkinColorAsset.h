// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

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

	/** Direct typed getters -- ZERO reflection at the apply site. */
	const TMap<FName, float>& GetScalars() const { return ScalarParameters; }
	const TMap<FName, FLinearColor>& GetColors() const { return ColorParameters; }
	const TMap<FName, TObjectPtr<UTexture>>& GetTextures() const { return TextureParameters; }
};
