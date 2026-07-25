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
