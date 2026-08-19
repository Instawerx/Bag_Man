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

	/** CC-6.4 -> "BaseTint" (visor base). Seeded FROM BodyColor whenever the player has not chosen a
	 *  visor colour, so the un-split case resolves to an identical value and the migration is
	 *  invisible -- an existing robot does not restyle itself because a field appeared. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Cosmetic|Creator")
	FLinearColor VisorColor = FLinearColor::White;

	FAFLColorOverride() = default;
	FAFLColorOverride(const FLinearColor& InBody, const FLinearColor& InEdge, const FLinearColor& InGlow)
		: bValid(true), BodyColor(InBody), EdgeColor(InEdge), GlowColor(InGlow), VisorColor(InBody) {}

	/** CC-6.4 four-channel ctor. The 3-arg form above SEEDS VisorColor FROM BodyColor deliberately, so
	 *  every existing call site keeps producing pre-split rendering without being touched. */
	FAFLColorOverride(const FLinearColor& InBody, const FLinearColor& InEdge, const FLinearColor& InGlow,
		const FLinearColor& InVisor)
		: bValid(true), BodyColor(InBody), EdgeColor(InEdge), GlowColor(InGlow), VisorColor(InVisor) {}

	/** Colour param KEY -> the override tone, or nullptr if invalid / not one of the three creator channels.
	 *  Same shape + precedence slot as FAFLSkinFinish::FindToneForParam. */
	const FLinearColor* FindOverrideForParam(const FName& ParamName) const
	{
		if (!bValid) { return nullptr; }
		static const FName NTeam(TEXT("TeamColor"));
		static const FName NEdgeGlow(TEXT("EdgeGlowColor"));
		static const FName NEmissive(TEXT("EmissiveColor"));
		// CC-2.2: the VISOR base tint. Both slot-1 masters (M_AFL_Visor_Clean, M_AFL_FaceMask_Visor) expose the
		// same vector pair BaseTint + EmissiveColor, so ONE mapping serves both and no branch on which is bound is
		// needed. EmissiveColor already reached the visor for free -- the colour loop writes every preset key to
		// EVERY slot MID (measured: 88 writes per key on slot 1, identical to slot 0), so the visor glow has
		// followed the creator since CC-2.1. BaseTint is the one visor channel the creator could not reach.
		// DELIBERATELY THE SAME VALUE AS TeamColor (operator ruling): the visor base tracks the chassis body
		// colour rather than being an independent choice. No new field on this struct -- adding one would drag in
		// a clamp axis, a persistence field and an entitlement surface for what is currently one mapping. Splitting
		// BaseTint onto its own creator channel later is a strict superset of this and costs nothing now.
		// SAFE ON THE BODY BY CONSTRUCTION: M_AFL_Character exposes NO BaseTint parameter (verified: 0 matches,
		// any case, in its T3D export), so the loop's blanket write of this key to slot 0 is an inert no-op --
		// SetVectorParameterValue on an absent parameter is ignored. The body cannot be tinted by this.
		static const FName NBaseTint(TEXT("BaseTint"));
		if (ParamName == NTeam)     { return &BodyColor; }
		// CC-6.4: BaseTint resolves to the VISOR colour now, not the body colour -- the split deferred
		// at CC-2.2. VisorColor is seeded from BodyColor when unset, so this returns the same value in
		// the un-split case and nothing that renders today changes.
		if (ParamName == NBaseTint) { return &VisorColor; }
		if (ParamName == NEdgeGlow) { return &EdgeColor; }
		if (ParamName == NEmissive) { return &GlowColor; }
		return nullptr;
	}
};
