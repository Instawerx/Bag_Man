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
	Character         UMETA(DisplayName = "Character Identity"),   // AFL.Character.<Name>-> character identity (GrantedFree base)
	// ADR Decision 5 (composable address scheme) -- additive types (no existing value renamed/reordered):
	Finish            UMETA(DisplayName = "Finish"),               // AFL.Finish.<Color>  -> UAFLSkinColorAsset (Axis==Finish); full base finish, SOLE color source
	Weapon            UMETA(DisplayName = "Weapon"),               // AFL.Weapon.<Name>   -> full-weapon SKU; TSoftClassPtr to the Lyra equipment item-def (owning grants the equipment)
	Accessory         UMETA(DisplayName = "Accessory"),            // AFL.Accessory.<Name>-> per-identity attachment (composes via AddCharacterPart); distinct from WeaponAccessory
	Bundle            UMETA(DisplayName = "Bundle"),               // AFL.Bundle.<Name>   -> grants a SET of child CosmeticIds on the same ownership spine (buy-once -> grant-N)
	// Facemask axis (dedicated type, replaces the SkinColor_Body interim the 31 mask rows used). A facemask is a
	// FULL slot-1 MATERIAL cosmetic (UAFLSkinColorAsset wrapping the mask MIC, proven path MI_AFL_FaceMask_Pink):
	// the visual is the slot-1 base-MI swap (HUD-faceplate richness, animated/reactive future), applied at runtime
	// via the replicated FacemaskId selection (mirrors the EdgeId axis + the Lyra CharacterParts replicate-then-
	// apply model). DISTINCT from SkinColor_* (param-push) and Helmet (the retired part path). Address AFL.Facemask.<Name>.
	Facemask          UMETA(DisplayName = "Facemask"),             // AFL.Facemask.<Name> -> slot-1 material reskin (the proven facemask path), runtime-equipped + replicated
	// EMBLEM axis (X-line three-layer economy: Edge colour / Facemask visor / EMBLEM logo). The chest brand mark
	// is a UDecalComponent projection driven by a per-brand MI off M_AFL_Branding_Decal (BrandMaskTex = the brand
	// mask texture, NeonColor = the identity tint) -- see AAFLCharacterPartActor's ChestEmblemDecal, bone-attached
	// to spine_04. DISTINCT from Facemask (slot-1 material) and SkinColor_*/Finish (param push): the emblem owns no
	// material slot, it projects onto the body. Catalogued independently so the logo layer is a-la-carte sellable.
	Emblem            UMETA(DisplayName = "Emblem"),               // AFL.Emblem.<Brand>  -> chest brand decal (per-brand MI + mask texture)

	// APPENDED 2026-08-19 (CC-X17). A row that is never explicitly typed must be DETECTABLE.
	// Appended at the END so no existing enumerator's numeric value shifts. This value is a NAME to
	// check against and to author future rows against; whether it becomes the struct default is a
	// separate decision with separate risk -- see FAFLCatalogEntry::Type.
	Invalid           UMETA(DisplayName = "Invalid / unset"),   // never a valid catalog row

	// CC-6.1 CREATOR SLOT / ROBOT PACK. NOT a cosmetic: it grants a COUNTED entitlement
	// (FAFLCatalogEntry -> CountedEntitlements, CC-3.3), not an item on a cosmetic axis. Bundle was the
	// near-miss and is deliberately NOT reused -- Bundle grants a SET of child CosmeticIds, a different
	// shape, and reusing it would make the type-vs-id-prefix lint meaningless for these rows.
	//
	// APPENDED AFTER Invalid ON PURPOSE, despite reading oddly. Inserting an enumerator anywhere earlier
	// renumbers every value after it, and stored .uasset values are raw ints. Inserting Unclassified at
	// position 0 of EAFLColorEntryKind cost a full no-regression run today (cc-6-4-done); this is that
	// lesson applied. Ugly ordering, stable data.
	//
	// ONE MECHANISM for robots AND slots (PRICING_SSOT 5.4): a $3 purchase increments the counted slot
	// entitlement and grants creator rights to that slot; the x3 and x8 packs increment by 3 and 8.
	// Address AFL.CreatorSlot.<N>.
	CreatorSlot       UMETA(DisplayName = "Creator Slot / Robot Pack")

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
 * Content tier (ADR Decision 6 / Ruling 2) -- an EXPLICIT management fact stamped on each catalog entry,
 * NOT inferred from Acquisition+CollectionId. Descriptive/filterable metadata (merchandising), DISTINCT
 * from the composable address (never in CosmeticId) and from the paid-ladder EAFLCosmeticTier:
 *  - Base      = the free base layer (the 7 AFL.Finish.* + free base identities; Acquisition GrantedFree).
 *  - Premium   = the paid branded content (the 30-name identity backlog; Acquisition Direct + a Tier rung).
 *  - Event     = limited-time event content.
 *  - Seasonal  = season-track / battle-pass content.
 */
UENUM(BlueprintType)
enum class EAFLContentTier : uint8
{
	Base       UMETA(DisplayName = "Base (free)"),
	Premium    UMETA(DisplayName = "Premium"),
	Event      UMETA(DisplayName = "Event"),
	Seasonal   UMETA(DisplayName = "Seasonal")
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

/**
 * CC-4.2 -- EFFECTIVE SLOT CAP. Every input is a PARAMETER; there are no numbers in this function.
 *
 * PRICING_SSOT 5.2 states the rule and then says "All four values are data. No number is hardcoded."
 * Writing 2, 5 or 10 in here would make the ladder a code change -- and the ladder is a product lever
 * that moves with subscription tiers and promotions.
 *
 *   MaxUpgrade ? HardCap : clamp(Baseline + Purchased, Baseline, TierCeiling)
 *
 * Baseline is CONDITIONAL (derived from sub state: 2 free, 5 League), Purchased is COUNTED (permanent,
 * $3 each), MaxUpgrade is BOOLEAN OWNED (permanent, $10). Keeping the three shapes separate is what
 * makes lapse tractable -- a lapsed subscriber keeps what they BOUGHT and loses only the baseline.
 *
 * Purchased is clamped at zero: a negative count is meaningless and would silently reduce a cap below
 * the baseline the player is entitled to regardless.
 */
inline int32 AFLResolveEffectiveSlotCap(const int32 Baseline, const int32 Purchased,
	const int32 TierCeiling, const bool bMaxUpgradeOwned, const int32 HardCap)
{
	if (bMaxUpgradeOwned)
	{
		return HardCap;
	}
	const int32 Safe = FMath::Max(0, Purchased);
	return FMath::Clamp(Baseline + Safe, Baseline, TierCeiling);
}

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
	// CC-X17 2026-08-19: was SkinColor_Edge -- a REAL, WRONG value, so any row authored without
	// setting Type was silently absorbed into the Edge axis. It caught two separate batches of 27:
	// the AFL.Facemask.* rows (retyped, cc-x16-done) and the AFL.Character.*_X rows (CC-X18, left
	// as-is because the roster cut retires them). Now Invalid, so an untyped row is DETECTABLE
	// rather than plausible.
	//
	// MEASURED SAFE, not assumed. The risk was that UE delta-serializes struct fields against this
	// default, which would mean rows equal to the OLD default were never written to disk and would
	// load as Invalid under this change -- silently retyping all 67, including the live Edge axis.
	// Tested by flipping the default, rebuilding, and censusing a fresh load WITHOUT saving:
	// SKIN_COLOR_EDGE stayed 67, INVALID was 0, all 581 rows matched the pre-change census exactly.
	// The values are explicitly serialized; the hypothesis was wrong and the flip moves nothing.
	EAFLCosmeticType Type = EAFLCosmeticType::Invalid;

	/** The cosmetic asset this id resolves to (SOFT -- resolved/loaded on demand by the registry). The
	 *  concrete type depends on Type (UAFLSkinColorAsset for skin axes, a part/equipment asset otherwise). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Identity", meta = (AllowAbstract = "true"))
	TSoftObjectPtr<UPrimaryDataAsset> Asset;

	/**
	 * FULL-WEAPON payload (Type == Weapon). Owning the SKU grants the equipment, so a weapon row must carry the
	 * Lyra equipment ITEM-DEF CLASS.
	 *
	 * ⚠ WHY THIS FIELD HAD TO EXIST AT ALL: `Asset` above is a TSoftObjectPtr<UPrimaryDataAsset> and therefore
	 * CANNOT hold an item-def -- a LyraInventoryItemDefinition is a Blueprint CLASS, not a data-asset instance.
	 * The EAFLCosmeticType::Weapon enum comment claimed a "TSoftClassPtr to the Lyra equipment item-def" already
	 * existed; it described the intent, not an implemented field. The gap went unnoticed because the 63 pre-existing
	 * Weapon rows are all COSMETIC ones -- they populate `Asset` with a DA_AFL_Weapon_* (UAFLWeaponCosmeticAsset,
	 * a skin) and never needed to grant equipment, so nothing ever asked a Weapon row for an item-def.
	 *
	 * ⚠ WHY TSoftClassPtr<UObject> + MetaClass RATHER THAN THE CONCRETE TYPE: AFLCosmeticCore deliberately does
	 * NOT depend on LyraGame (it is game-agnostic cosmetic core), and ULyraInventoryItemDefinition carries no
	 * LYRAGAME_API export -- naming it here would be both a layering violation and an LNK2019, the same export
	 * trap that shaped the quickbar/weapon-wheel split. MetaClass gives the editor a correctly-filtered class
	 * picker with ZERO link dependency, and stays soft so the grid never loads equipment to draw a tile.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Identity",
		meta = (MetaClass = "/Script/LyraGame.LyraInventoryItemDefinition"))
	TSoftClassPtr<UObject> ItemDefClass;

	/** Player-facing, localizable. Marketing owns this; safe to change (unlike CosmeticId). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Identity")
	FText DisplayName;

	// --- Economy (LOCKED IRONICS values; integer, never float) ---

	/** Paid ladder rung. IGNORED when Acquisition == GrantedFree (identity / free base carry no tier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	EAFLCosmeticTier Tier = EAFLCosmeticTier::SPARK;

	/** Content tier (ADR Decision 6 / Ruling 2) -- EXPLICIT management metadata, NOT inferred. Descriptive
	 *  (Base/Premium/Event/Seasonal) for filtering/merchandising; distinct from the paid Tier ladder and
	 *  never part of CosmeticId. Defaults to Base; the 30-name premium identities stamp Premium. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	EAFLContentTier ContentTier = EAFLContentTier::Base;

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

	/**
	 * CC-4.2 -- WHICH COUNTED ENTITLEMENT THIS SKU FEEDS, and HOW MANY UNITS.
	 *
	 * A robot/slot pack does not grant an ITEM, it increments a COUNT. OwnedCosmeticIds is a boolean set
	 * and cannot express "three of these" -- buying x3 twice must reach six, not stay owned=true.
	 *
	 * THE QUANTITY IS DATA, NOT PARSED FROM THE ID. Reading 3 out of "AFL.CreatorSlot.x3" would be taking
	 * a VALUE from a NAME, which is the provenance error this programme has paid for repeatedly (a
	 * material parameter reading (0,0,0) could not say "absent"; a Type reading SkinColor_Edge could not
	 * say "authored"; matching by name classified IronicsVisor as the IRONICS identity). A renamed SKU
	 * must not silently change what it grants.
	 *
	 * ONE MECHANISM. x1, x3 and x8 all carry the SAME CountedKey, so they accumulate into ONE counter --
	 * PRICING_SSOT 5.4's ruling that a $3 robot and a $3 slot are the same transaction, expressed as data
	 * rather than as three parallel ladders that would double-grant or double-charge.
	 *
	 * FAILS CLOSED. Default NAME_None / 0: a SKU nobody configured grants nothing. The alternative --
	 * defaulting to a real key with quantity 1 -- is the FAFLCatalogEntry::Type trap again, where a
	 * plausible default silently absorbs rows nobody typed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	FName CountedKey = NAME_None;

	/** Units granted per purchase into CountedKey. 0 = grants no counted entitlement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	int32 GrantQuantity = 0;

	/**
	 * CAN A WEAPON CREDIT REDEEM THIS ROW? Default FALSE, and the default is the whole point.
	 *
	 * FAILS CLOSED: an unmarked row is REFUSED, not redeemed. The alternative -- defaulting true and
	 * marking exclusions -- means every row authored from now on is silently redeemable until someone
	 * remembers to exclude it, which is the FAFLCatalogEntry::Type trap that cost 27 invisible facemask
	 * rows (CC-X17). A forgotten mark here costs a player one refused redemption; a forgotten mark the
	 * other way costs a $1.49 set for a third of a credit.
	 *
	 * NOT DERIVED FROM Type OR FROM THE ID. Hand cannons live under AFL.Weapon.* exactly like ordinary
	 * weapons and share Type=Weapon, so neither separates them -- measured: every candidate
	 * discriminator (Type, Tier, ContentTier, Rarity, bTransactable, CollectionId) OVERLAPS between the
	 * two groups. This is an explicit, authored fact, which is why it is a field and not a rule.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	bool bCreditRedeemable = false;

	/** Season / set grouping (e.g. Founders, Season_1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	FName CollectionId;

	/** Cosmetic flavor rarity (orthogonal to Tier/price). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Economy")
	EAFLCosmeticRarity Rarity = EAFLCosmeticRarity::Common;

	// --- Bundle + limited-edition (Build B1: catalog DATA + the inert gate; mint/trade ENFORCEMENT is B2/persistence) ---

	/** BUNDLE composition (Type==Bundle). The DISTINCT child SKU CosmeticIds this bundle grants buy-once->grant-N
	 *  (ADR Decision 4 ChildCosmeticIds / PSS §4.5): { Character-axis · Team-axis · finish · edge · mask · weapon }.
	 *  Empty for non-bundles. A RESERVED-but-unbuilt child id (e.g. the weapons-phase weapon/mask, D2) is listed
	 *  here as its FUTURE id -- the ATOMIC grant that makes it live is B2, not this row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Bundle")
	TArray<FName> ContainedEntitlementIds;

	/** THE INERT GATE (Build B1, load-bearing). FALSE -> the purchase path (ClientRequestPurchase +
	 *  ServerPurchaseCosmetic) HARD-DECLINES with "not yet available (backend-gated)" BEFORE any transaction, so
	 *  an Acquisition-flipped-to-paid SKU CANNOT transact until B2 de-inerts it. Default TRUE (every existing
	 *  purchasable item is byte-unchanged); the 29 bundle rows ship FALSE. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Bundle")
	bool bTransactable = true;

	/** Limited-edition mint cap (the 1-of-N). 0 = unlimited (default; every existing row). The 1-of-1 Singularity
	 *  bundles set 1. FIELD ONLY here -- MintedCount + sold-out/never-reissue ENFORCEMENT is persistence-gated B2 (PSS §5.1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Bundle")
	int32 MintCap = 0;

	/** INTACT-ONLY trade (PSS E1). TRUE -> this bundle's child SKUs are container-locked: the grail trades ONLY
	 *  as one atomic unit (no per-child transfer while bundled). FIELD ONLY -- the trade LOCKING enforcement is B2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Catalog|Bundle")
	bool bIntactOnlyBundle = false;

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
