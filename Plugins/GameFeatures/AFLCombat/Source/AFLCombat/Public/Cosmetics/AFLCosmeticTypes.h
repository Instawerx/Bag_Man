// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
// EAFLCosmeticRarity moved to the always-loaded AFLCosmeticCore module (the catalog needs it at the
// engine-startup AssetManager scan, before this GameFeature loads). Re-included here because
// UAFLSkinColorAsset still uses it. AFLCombat now depends on AFLCosmeticCore (the correct direction).
#include "AFLCosmeticCoreTypes.h"

#include "AFLCosmeticTypes.generated.h"

/**
 * Skin-pillar-specific cosmetic axis for BagMan robot skins. Stays in AFLCombat (used only by
 * UAFLSkinColorAsset + the proven #43/#38a skin/controller code -- no churn to those assets). The
 * catalog uses EAFLCosmeticType's SkinColor_Edge/Body as its own discriminator instead (see
 * AFLCosmeticCoreTypes.h); a SkinColor_Edge catalog entry resolves to a UAFLSkinColorAsset whose
 * Axis == Edge. Pure enum, no runtime behavior.
 */
UENUM(BlueprintType)
enum class EAFLCosmeticAxis : uint8
{
	Edge    UMETA(DisplayName = "Edge Glow"),
	Body    UMETA(DisplayName = "Body Color"),
	// ADR Decision 5 (composable address scheme): a FULL base finish (body TeamColor + emissive +
	// edge-glow together) vs an edge-only preset. AFL.Finish.<Color> presets carry Axis==Finish; they are
	// the SOLE color source once identity MIs are color-neutralized (Ruling 1). Additive -- Edge/Body
	// (shipped AFL.Edge.* presets) are untouched.
	Finish  UMETA(DisplayName = "Finish (full base color)"),
	// X-line three-layer economy (Edge colour / Facemask visor / EMBLEM logo): the chest brand mark, carried as a
	// per-brand MI off M_AFL_Branding_Decal and projected by the identity's ChestEmblemDecal (bone-attached,
	// spine_04). Additive -- Edge/Body/Finish are untouched. A UAFLSkinColorAsset with Axis==Emblem wraps the
	// brand MI the same way Axis==Edge wraps an edge preset and the Facemask asset wraps the slot-1 mask MIC.
	Emblem  UMETA(DisplayName = "Emblem (chest brand decal)")
};

/**
 * FAFLColorOverride -- the CREATOR per-channel colour overlay (CC-2.1). Three colours that, when bValid, override
 * the resolved tone for exactly the three body colour params written on a unique-body chassis:
 *   BodyColor -> "TeamColor",  EdgeColor -> "EdgeGlowColor",  GlowColor -> "EmissiveColor".
 * PASSED IN through the controller push (never pulled inside ApplySkinColor). Default-constructs INVALID, so every
 * non-creator call path is byte-identical -- the regression guarantee is BY CONSTRUCTION (FindOverrideForParam
 * returns nullptr when !bValid, leaving the loop's value expression unchanged). NOT replicated: it is built from the
 * replicated FAFLCosmeticSelection creator fields in RefreshSkinForPawn. Mirrors FAFLSkinFinish::FindToneForParam so
 * it slots into the SAME in-loop precedence expression.
 */
USTRUCT(BlueprintType)
struct FAFLColorOverride
{
	GENERATED_BODY()

	/** FALSE (default) = no overlay; every consumer behaves exactly as before. TRUE = the three colours below win. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Cosmetic|Creator")
	bool bValid = false;

	/** -> "TeamColor" (body finish base shade). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Cosmetic|Creator")
	FLinearColor BodyColor = FLinearColor::White;

	/** -> "EdgeGlowColor" (rim glow). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Cosmetic|Creator")
	FLinearColor EdgeColor = FLinearColor::White;

	/** -> "EmissiveColor" (emissive base tone). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Cosmetic|Creator")
	FLinearColor GlowColor = FLinearColor::White;

	FAFLColorOverride() = default;
	FAFLColorOverride(const FLinearColor& InBody, const FLinearColor& InEdge, const FLinearColor& InGlow)
		: bValid(true), BodyColor(InBody), EdgeColor(InEdge), GlowColor(InGlow) {}

	/** Colour param KEY -> the override tone, or nullptr if invalid / not one of the three creator channels.
	 *  Same shape + precedence slot as FAFLSkinFinish::FindToneForParam. */
	const FLinearColor* FindOverrideForParam(const FName& ParamName) const
	{
		if (!bValid) { return nullptr; }
		static const FName NTeam(TEXT("TeamColor"));
		static const FName NEdgeGlow(TEXT("EdgeGlowColor"));
		static const FName NEmissive(TEXT("EmissiveColor"));
		if (ParamName == NTeam)     { return &BodyColor; }
		if (ParamName == NEdgeGlow) { return &EdgeColor; }
		if (ParamName == NEmissive) { return &GlowColor; }
		return nullptr;
	}
};
