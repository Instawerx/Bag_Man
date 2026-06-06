// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLCosmeticCoreTypes.generated.h"

class UPrimaryDataAsset;

/**
 * AFL cosmetic-economy CORE vocabulary (S-ECON-CAT). Lives in the always-loaded AFLCosmeticCore module
 * (not the AFLCombat GameFeature) so the catalog that uses these is loadable at the engine-startup
 * AssetManager scan. Pure enums + the catalog entry struct, no runtime behavior.
 *
 * EAFLCosmeticRarity moved here from AFLCombat's AFLCosmeticTypes.h (it is pure economy metadata the
 * catalog needs); UAFLSkinColorAsset still uses it (AFLCombat now depends on AFLCosmeticCore -- the
 * correct dependency direction). EAFLCosmeticAxis stays in AFLCombat (skin-pillar-specific; the catalog
 * uses EAFLCosmeticType's SkinColor_Edge/Body as its own discriminator instead).
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

/**
 * The catalog discriminator — what KIND of thing a catalog entry is. ONE unified type so the catalog /
 * store / wallet / showroom all key on a single "what is this" value. Wider than EAFLCosmeticAxis: it
 * absorbs the skin axes (SkinColor_Edge/Body map to a UAFLSkinColorAsset whose Axis matches -- no
 * migration of the proven #43/#38a assets) AND adds the kinds that ride DIFFERENT propagation paths:
 * Helmet (a CharacterPart-style part-swap), WeaponAccessory (the Lyra equipment grant path -- IRONICS
 * EMP from the grenade slot), Beam (a beam-VFX variant), and the either/or identity types.
 */
UENUM(BlueprintType)
enum class EAFLCosmeticType : uint8
{
	SkinColor_Edge    UMETA(DisplayName = "Skin Color - Edge"),    // AFL.Edge.<Color>    -> UAFLSkinColorAsset (Axis==Edge)
	SkinColor_Body    UMETA(DisplayName = "Skin Color - Body"),    // AFL.Body.<Color>    -> UAFLSkinColorAsset (Axis==Body)
	Helmet            UMETA(DisplayName = "Helmet"),               // AFL.Helmet.<Name>   -> CharacterPart-style part (part path)
	WeaponAccessory   UMETA(DisplayName = "Weapon Accessory"),     // AFL.Weapon.<Name>   -> equipment cosmetic (equip path; EMP)
	Beam              UMETA(DisplayName = "Beam"),                 // AFL.Beam.<Name>     -> beam VFX variant
	Team              UMETA(DisplayName = "Team Identity"),        // AFL.Team.<BRAND>    -> team identity (GrantedFree base)
	Character         UMETA(DisplayName = "Character Identity")    // AFL.Character.<Name>-> character identity (GrantedFree base)
};

/**
 * The paid cosmetic tier ladder (IRONICS economy, LOCKED -- see IRONICS_ECONOMY_SPEC.md / tracker
 * 882b0914). Prices live on the catalog entry; this is the rung. SPARK is the only Watts-buyable tier;
 * SURGE/ARC/THUNDERBOLT are Volts (Watts can discount, not fully buy). Identity + free base sit OUTSIDE
 * this ladder (Acquisition == GrantedFree) -- there is intentionally no "Free" rung here.
 */
UENUM(BlueprintType)
enum class EAFLCosmeticTier : uint8
{
	SPARK         UMETA(DisplayName = "SPARK (Accessible)"),    // 10,000 V or 100,000 W
	SURGE         UMETA(DisplayName = "SURGE (Standard)"),      // 16,000 V
	ARC           UMETA(DisplayName = "ARC (Premium)"),         // 23,000 V
	THUNDERBOLT   UMETA(DisplayName = "THUNDERBOLT (Prestige)") // 30,000 V
};

/**
 * How a cosmetic is acquired (IRONICS economy, LOCKED). Direct = bought in the item shop; BattlePass =
 * earned on a season track; GrantedFree = owned by everyone (all founding teams, free Character base,
 * basic colors). The entitlement gate (#43 permissive now, wallet later) keys off this + ownership.
 */
UENUM(BlueprintType)
enum class EAFLAcquisition : uint8
{
	Direct        UMETA(DisplayName = "Direct (Item Shop)"),
	BattlePass    UMETA(DisplayName = "Battle Pass"),
	GrantedFree   UMETA(DisplayName = "Granted Free")
};

/**
 * FAFLCatalogEntry — one row of the curated cosmetic manifest (S-ECON-CAT).
 *
 * The single join record every economy system reads: CosmeticId (the immutable FName key used by the
 * #43 selection, the store, the wallet, and the showroom) -> the cosmetic Asset + its economy metadata.
 * Prices are INTEGER Watts/Volts (peg discipline, never float) carrying the LOCKED IRONICS values:
 * SPARK 10,000 V / 100,000 W, SURGE 16,000 V, ARC 23,000 V, THUNDERBOLT 30,000 V; identity + free base
 * = Acquisition GrantedFree, prices 0.
 *
 * Asset is a SOFT ref so the catalog stays light (it doesn't drag every cosmetic into memory); the
 * registry resolves+loads on demand. AssetManager primary-asset rules keep the soft-referenced
 * cosmetics in the cook set (the catalog asset is the label that pulls them in).
 */
USTRUCT(BlueprintType)
struct FAFLCatalogEntry
{
	GENERATED_BODY()

	/** Immutable machine key. Format AFL.<Type>.<Name> (e.g. AFL.Edge.NeonGreen, AFL.Helmet.Visor01,
	 *  AFL.Weapon.EMP, AFL.Team.ARIA). NEVER change once shipped -- it is the join key everywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Identity")
	FName CosmeticId;

	/** What KIND this is -- the catalog discriminator. Selects which propagation path consumes it
	 *  (skin/part vs equipment vs identity). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Identity")
	EAFLCosmeticType Type = EAFLCosmeticType::SkinColor_Edge;

	/** The cosmetic asset this id resolves to (SOFT -- resolved/loaded on demand by the registry). The
	 *  concrete type depends on Type (UAFLSkinColorAsset for skin axes, a part/equipment asset otherwise). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Identity", meta = (AllowAbstract = "true"))
	TSoftObjectPtr<UPrimaryDataAsset> Asset;

	/** Player-facing, localizable. Marketing owns this; safe to change (unlike CosmeticId). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Identity")
	FText DisplayName;

	// --- Economy (LOCKED IRONICS values; integer, never float) ---

	/** Paid ladder rung. IGNORED when Acquisition == GrantedFree (identity / free base carry no tier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	EAFLCosmeticTier Tier = EAFLCosmeticTier::SPARK;

	/** Hard price in Volts. 0 for GrantedFree. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	int32 PriceVolts = 0;

	/** Soft price in Watts. 0 = NOT directly Watts-buyable (only SPARK is, at 100,000 W). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	int32 PriceWatts = 0;

	/** True if Watts can DISCOUNT the Volts price (SURGE and up). SPARK is fully Watts-buyable instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	bool bWattsDiscountable = false;

	/** How it's obtained -- Direct / BattlePass / GrantedFree. The entitlement gate keys off this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	EAFLAcquisition Acquisition = EAFLAcquisition::Direct;

	/** Season / set grouping (e.g. Founders, Season_1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	FName CollectionId;

	/** Cosmetic flavor rarity (orthogonal to Tier/price). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	EAFLCosmeticRarity Rarity = EAFLCosmeticRarity::Common;
};
