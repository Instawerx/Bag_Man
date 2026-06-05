// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLCosmeticTypes.generated.h"

/**
 * Shared cosmetic-economy vocabulary for BagMan robot skins (mirrors Lyra's
 * LyraCharacterPartTypes.h pattern -- a project types header grouping cosmetic enums).
 * Used by UAFLSkinColorAsset (the per-color preset) and, later, the brand-edge map +
 * store/entitlement code. Pure enums, no runtime behavior.
 */

UENUM(BlueprintType)
enum class EAFLCosmeticRarity : uint8
{
	Common      UMETA(DisplayName = "Common"),
	Uncommon    UMETA(DisplayName = "Uncommon"),
	Rare        UMETA(DisplayName = "Rare"),
	Epic        UMETA(DisplayName = "Epic"),
	Legendary   UMETA(DisplayName = "Legendary")
};

UENUM(BlueprintType)
enum class EAFLCosmeticAxis : uint8
{
	Edge    UMETA(DisplayName = "Edge Glow"),
	Body    UMETA(DisplayName = "Body Color")
};
