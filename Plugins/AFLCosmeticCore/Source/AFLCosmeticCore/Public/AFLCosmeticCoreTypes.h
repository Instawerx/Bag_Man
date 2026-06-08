// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"   // FGameplayTag RarityTag (skill: rarity tag drives the shop frame color)

#include "AFLCosmeticCoreTypes.generated.h"

class UPrimaryDataAsset;
class UTexture2D;

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
 * migration of the proven #43/#38a assets) AND adds the kinds that ride DIFFERENT propagation paths.
 *
 * The type names the PROPAGATION MECHANISM (this is what makes the catalog one-source-many-readers work):
 *  - Helmet          -> CharacterPart part-swap (the #38a/#43 part path; a part added + colored).
 *  - AbilityCosmetic -> a GameplayAbility-grant cosmetic (e.g. IRONICS EMP = a GA_Grenade reskin granted
 *                       via an AbilitySet). DISTINCT from equipment: the Lyra grenade is an ABILITY
 *                       (GA_Grenade + B_Grenade projectile, AbilitySet-granted), NOT an ItemDefinition --
 *                       so a grenade reskin rides the ability-grant path, a genuinely different mechanism
 *                       from both the part path AND the weapon-equipment path (Pulse/Beam = ID_/WID_).
 *  - WeaponAccessory -> a genuine weapon-attachment EQUIPMENT cosmetic (ID_/WID_/equipment-manager), IF/
 *                       when those exist. NOT the EMP (the EMP is an ability, see AbilityCosmetic). Kept
 *                       as a distinct, honest bucket rather than overloading it with ability cosmetics.
 *  - Beam            -> a beam-VFX variant. Team/Character -> the either/or identity types (GrantedFree).
 */
UENUM(BlueprintType)
enum class EAFLCosmeticType : uint8
{
	SkinColor_Edge    UMETA(DisplayName = "Skin Color - Edge"),    // AFL.Edge.<Color>    -> UAFLSkinColorAsset (Axis==Edge)
	SkinColor_Body    UMETA(DisplayName = "Skin Color - Body"),    // AFL.Body.<Color>    -> UAFLSkinColorAsset (Axis==Body)
	Helmet            UMETA(DisplayName = "Helmet"),               // AFL.Helmet.<Name>   -> CharacterPart-style part (part path)
	AbilityCosmetic   UMETA(DisplayName = "Ability Cosmetic"),     // AFL.Ability.<Name>  -> a GameplayAbility-grant cosmetic (EMP)
	WeaponAccessory   UMETA(DisplayName = "Weapon Accessory"),     // AFL.Weapon.<Name>   -> weapon-attachment EQUIPMENT cosmetic (ID_/WID_)
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

	// --- Display (S-ECON-STORE / IRONICS Digital Market — the skill's display-row fields the tile + details
	//     panel render WITHOUT loading the full asset; soft refs only, per the mobile-memory discipline) ---

	/** Two-line marketing blurb shown in the details panel (e.g. "Cut through the darkness.\nLeave only
	 *  the edge."). MultiLine so the editor gives a multi-row box. Localizable; marketing owns it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Display", meta = (MultiLine = true))
	FText Description;

	/** The collection/series the details panel prints under the name ("IRONICS SERIES"). Localizable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Display")
	FText SeriesName;

	/** Shop thumbnail for the product card -- SOFT ref (hard refs blow up mobile memory; the grid soft-loads
	 *  on display and releases off-screen). Author as T_<Item>_Thumb, low-res 256-512. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Display")
	TSoftObjectPtr<UTexture2D> ShopThumbnail;

	/** Rarity as a GAMEPLAY TAG (the skill's canonical: rarity tag drives the shop CARD FRAME color, e.g.
	 *  Cosmetic.Rarity.Legendary -> gold frame). Lives ALONGSIDE the EAFLCosmeticRarity enum (which the
	 *  skin-color-asset path still consumes) -- additive, NOT a migration, so existing enum reads are intact.
	 *  The store frame-color logic keys off THIS; ResolveRarityTag() falls back to the enum if this is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Display", meta = (Categories = "Cosmetic.Rarity"))
	FGameplayTag RarityTag;

	/** Stat-meter value (1-5) shown as the VISUAL INTENSITY bar in the details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Display", meta = (ClampMin = "0", ClampMax = "5"))
	int32 VisualIntensity = 0;

	/** Stat-meter value (1-5) shown as the GLOW IMPACT bar in the details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Display", meta = (ClampMin = "0", ClampMax = "5"))
	int32 GlowImpact = 0;

	/** Compatibility line in the details panel ("All Classes"). Localizable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Display")
	FText Compatibility;

	/** The product's COLOR IDENTITY tag (Cosmetic.Identity.<Name>, e.g. Cosmetic.Identity.NeonEdge). Points at
	 *  a row in the UAFLColorIdentityRegistry which DECLARES the Primary + Accent neon colors. The card frame /
	 *  showroom / equipped look all RESOLVE this tag -> the registry -> the colors (uniform, never derived).
	 *  SEPARATE axis from RarityTag (which drives ONLY the rarity badge). Replaces the retired ColorTheme FName
	 *  (which fed a per-card derivation that misfired on pooled list tiles). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Display", meta = (Categories = "Cosmetic.Identity"))
	FGameplayTag ColorIdentityTag;
};
