// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"

#include "AFLColorIdentityRegistry.generated.h"

/**
 * FAFLSkinFinish -- the full skin-body color look (multi-tone finish) for a color identity.
 *
 * Per AFL_ECONOMY_ARCHITECTURE_ADR Decision 10 + the lyra-skin-builder-marketplace skill, a color is a FULL
 * coherent finish: body TeamColor + a 3-tier emissive ramp (EmissiveColor1/2/3) + an EdgeGlow rim -- NOT a
 * flat hue. The skin pillar's ApplySkinColor resolves a preset's ColorIdentityTag -> this -> the MID writes.
 * Values are transcribed VERBATIM from the proven baked presets (Edge_<color> emissive/edge + Finish_<color>
 * TeamColor) -> lossless by construction. SEPARATE from PrimaryColor/AccentColor (the cross-surface 2-color
 * scheme weapons + beams already ship reading) -- purely additive; those are untouched. Scalars + textures
 * stay in UAFLSkinColorAsset (SHAPE, not color); the registry carries color only.
 */
USTRUCT(BlueprintType)
struct FAFLSkinFinish
{
	GENERATED_BODY()

	/** BODY axis: body base shade (Finish presets write this; Edge presets have no TeamColor param).
	 *
	 * THIS IS THE IDENTITY COLOUR. Any census, dedup or fold across entries must key on THIS band,
	 * never on EdgeGlowColor. EdgeGlow is the BRIGHTEST band, not the identifying one, and keying
	 * on it silently merges entries that are not the same colour.
	 *
	 * MEASURED (CC-6.3): keying on EdgeGlowColor gives 42 distinct colours across the 47 entries,
	 * TeamColor gives 41, the full tuple gives 44 -- so BOTH single-band keys under-count. Under the
	 * EdgeGlow key, RIFTONE collided with Magenta and was ruled a duplicate; their TeamColors are
	 * (0.0000,0.5500,0.5000) teal versus (0.8960,0.1000,0.5520) magenta, and the fold would have
	 * replaced one with the other. NeonRed collides with FANATICS the same way -- and FANATICS is a
	 * KEPT, colour-locked sponsor brand, so a dedup on that key could have touched it.
	 *
	 * The doctrine already existed (read the identity colour from the primary band); the harvest
	 * scripts contradicted it. This comment sits here so the next census reads the rule where it
	 * reads the value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity|SkinFinish", meta = (HideAlphaChannel = "false"))
	FLinearColor TeamColor = FLinearColor(0.05f, 0.25f, 0.85f, 1.0f);

	/** EDGE axis: emissive base tone -- maps to the preset param named "EmissiveColor". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity|SkinFinish", meta = (HideAlphaChannel = "false"))
	FLinearColor EmissiveColor1 = FLinearColor(0.0f, 0.42f, 1.0f, 1.0f);

	/** EDGE axis: emissive bright tone -- preset param "EmissiveColor2". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity|SkinFinish", meta = (HideAlphaChannel = "false"))
	FLinearColor EmissiveColor2 = FLinearColor(0.0f, 0.896f, 1.0f, 1.0f);

	/** EDGE axis: emissive mid tone -- preset param "EmissiveColor3". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity|SkinFinish", meta = (HideAlphaChannel = "false"))
	FLinearColor EmissiveColor3 = FLinearColor(0.0f, 0.723f, 1.0f, 1.0f);

	/** EDGE axis: rim glow -- preset param "EdgeGlowColor". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity|SkinFinish", meta = (HideAlphaChannel = "false"))
	FLinearColor EdgeGlowColor = FLinearColor(0.0f, 0.42f, 1.0f, 1.0f);

	/** Map a skin preset's ColorParameters KEY -> the matching tone, so ApplySkinColor keeps the preset's param
	 *  SHAPE (which params it writes) and only swaps the VALUE source (baked -> registry). nullptr on an unknown
	 *  key -> caller keeps that param's baked value. */
	const FLinearColor* FindToneForParam(const FName& ParamName) const
	{
		static const FName NEmissive1(TEXT("EmissiveColor"));
		static const FName NEmissive2(TEXT("EmissiveColor2"));
		static const FName NEmissive3(TEXT("EmissiveColor3"));
		static const FName NEdgeGlow(TEXT("EdgeGlowColor"));
		static const FName NTeam(TEXT("TeamColor"));
		// UNIFIED COLOR AXIS (X-line): the chest EMBLEM decal (M_AFL_Branding_Decal) names its tint
		// "NeonColor", and the VISOR MIs name theirs the same. Both are BRAND-GLOW surfaces, so both
		// resolve to the SAME tone as the edge -> one identity choice tints body + edge + visor + emblem
		// together instead of the emblem/visor drifting to a baked value. This is the ONLY place that
		// mapping is declared; add a key here rather than renaming params on shipped materials.
		static const FName NNeon(TEXT("NeonColor"));
		if (ParamName == NEmissive1) { return &EmissiveColor1; }
		if (ParamName == NEmissive2) { return &EmissiveColor2; }
		if (ParamName == NEmissive3) { return &EmissiveColor3; }
		if (ParamName == NEdgeGlow)  { return &EdgeGlowColor; }
		if (ParamName == NNeon)      { return &EdgeGlowColor; }
		if (ParamName == NTeam)      { return &TeamColor; }
		return nullptr;
	}
};

/**
 * FAFLColorIdentity — one DECLARED color identity (S-ECON / cosmetic-identity foundation).
 *
 * A named product/collection identity (NOT an abstract color): "Neon Edge", "EMP", "Ironics Visor".
 * Each identity DECLARES its two-color scheme once; every surface (store card, Slice-2 showroom pedestal,
 * the equipped character later) RESOLVES the same IdentityTag -> the same colors. This is the registry
 * pattern (Fortnite/Valorant/CS2) and the skill's TeamColorPalette pattern: colors are declared + tag-keyed
 * + uniformly resolved, NEVER derived per-card (derivation is the bug class that misfired on pooled tiles).
 *
 * Primary = the dominant neon (frame/border glow, pedestal key). Accent = the contrast/secondary neon
 * (edge accent, inner detail). Rarity is a SEPARATE axis (FAFLCatalogEntry.RarityTag) and never shares a
 * slot with these.
 */
/**
 * CC-6.4 -- WHAT KIND OF ENTRY THIS IS.
 *
 * MEASURED DEFECT THIS FIXES: the registry holds 47 entries doing FOUR different jobs -- 28 brand
 * identities, named palette colours, weapon/FX tints, and visor/edge entries -- and NOTHING in the
 * asset says which is which. The only way to tell was to cross-reference an ENTIRELY DIFFERENT asset
 * (the catalog's AFL.Character.* / AFL.Team.* rows) and match on a normalised tag leaf.
 *
 * That is not a theoretical risk. Matching by NAME classified `Cosmetic.Identity.IronicsVisor` as the
 * IRONICS identity, because the string contains it. Under the roster cut that one mis-classification
 * is a visor entry preserved as an identity, or an identity deleted as a visor -- and the error only
 * becomes visible after the deletion that caused it.
 *
 * ADDITIVE BY CONSTRUCTION: no consumer enumerates Identities. All four read paths
 * (AFLCharacterPartActor, AFLCosmeticCatalogSubsystem, AFLW_LoadoutBase, AFLCombatCheats) narrow to
 * FindIdentity(tag) and then read SkinFinish, so adding a field changes nothing they do.
 *
 * ON THE DEFAULT (CORRECTED): an earlier revision of this comment argued Identity was a safe default
 * because "the cut's own cross-reference supplies every value explicitly". That makes correctness
 * depend on CC-6.3 being exhaustive, and any entry it misses reads Identity silently -- which is the
 * FAFLCatalogEntry::Type trap exactly, reproduced inside the field written to prevent it. 19 of the
 * 47 entries are NOT identities, so that default was wrong for all of them, and "never typed" could
 * not be told apart from "typed as Identity" by any read.
 *
 * The default is now Unclassified. That classifies nothing; it makes the ABSENCE of a classification
 * answerable by a read rather than by an assumption. It also fails in the safe direction: until the
 * entries are typed, GetEntriesOfKind(Identity) returns NOTHING, so a roster cut sees an empty keep
 * list and stops -- instead of seeing all 47 and preserving weapon tints as brands.
 */
UENUM(BlueprintType)
enum class EAFLColorEntryKind : uint8
{
	/** NOT YET CLASSIFIED -- the zero value and the default, so an untyped entry reads as untyped
	 *  instead of masquerading as a brand. Every entry starts here and must be moved deliberately. */
	Unclassified UMETA(DisplayName = "Unclassified"),
	/** A brand identity with a matching AFL.Character.* or AFL.Team.* catalog row. 28 of 47. */
	Identity   UMETA(DisplayName = "Brand identity"),
	/** A named palette colour. 11 of 47, MEASURED: NeonBlue/Green/Pink/Purple/Red/Yellow, Magenta,
	 *  Crimson, Indigo, Lime, Solar. Ownable-discrete colour, NOT a brand -- the creator's
	 *  named-colour vocabulary. Evidence: the entry carries a colour axis (Body/Edge/Finish).
	 *  (An earlier revision of this list named 9 and omitted NeonYellow and Solar.) */
	Palette    UMETA(DisplayName = "Palette colour"),
	/** A weapon or FX tint. 6 of 47, MEASURED: DRAGONSOUL, EMP, FUTUREWARRIOR, JAGUARNEON,
	 *  RUNITBACK, SIMULARENT. Not a selectable finish. Evidence: sold ONLY on Weapon/Ability --
	 *  RUNITBACK and SIMULARENT resolve to AFL.Weapon.HandCannon.* rows, EMP to AFL.Ability.*.
	 *  (An earlier revision named 4 and omitted RUNITBACK and SIMULARENT.) */
	WeaponFX   UMETA(DisplayName = "Weapon / FX tint"),
	/** A visor or edge entry. 2 of 47: IronicsVisor, NeonEdge. The enumerator whose ABSENCE caused
	 *  the IronicsVisor mis-classification. Evidence: these two have NO catalog row on ANY axis --
	 *  they are internal, not sold, which is what distinguishes them from a palette colour. */
	VisorEdge  UMETA(DisplayName = "Visor / edge")
};

USTRUCT(BlueprintType)
struct FAFLColorIdentity
{
	GENERATED_BODY()

	/** The identity key. Format Cosmetic.Identity.<Name> (e.g. Cosmetic.Identity.NeonEdge). Items point at
	 *  this via FAFLCatalogEntry.ColorIdentityTag; the registry resolves it to the colors below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity", meta = (Categories = "Cosmetic.Identity"))
	FGameplayTag IdentityTag;

	/** CC-6.4: which of the four jobs this entry does. Typed from evidence, never from the tag name --
	 *  name-matching classified IronicsVisor as the IRONICS identity. Read this; do not infer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity")
	EAFLColorEntryKind EntryKind = EAFLColorEntryKind::Unclassified;

	/** Player-facing identity name (e.g. "Neon Edge"). Editor/debug + a possible future "collection" header. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity")
	FText DisplayName;

	/** The dominant neon: card frame/border glow, showroom pedestal key light, equipped TeamColorPrimary. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity", meta = (HideAlphaChannel = "false"))
	FLinearColor PrimaryColor = FLinearColor(0.0f, 0.42f, 1.0f, 1.0f);

	/** The contrast/secondary neon: card edge accent + inner detail, showroom rim. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity", meta = (HideAlphaChannel = "false"))
	FLinearColor AccentColor = FLinearColor(0.0f, 0.941f, 1.0f, 1.0f);

	/** The full skin-body finish (multi-tone) for this identity: TeamColor + emissive ramp + edge. The skin
	 *  pillar's ApplySkinColor resolves a preset tag -> here -> the MID writes. ADDITIVE: PrimaryColor +
	 *  AccentColor above are unchanged (weapons read Primary->AccentColor, beams read Primary->User.Color --
	 *  both unaffected by this new field). Populated in STEP 2 (data), verbatim from the baked presets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity")
	FAFLSkinFinish SkinFinish;

	/** SPONSOR LOCK (two identity classes). TRUE = a SPONSOR identity (FANATICS, IRONICS, free brand lines):
	 *  the brand color IS the product, so it is COLOR-LOCKED -- a player color selection must NOT repaint it.
	 *  FALSE (default) = a STANDARD identity (paid/non-sponsor): COLOR-NEUTRAL, the player's chosen color owns
	 *  every surface. Lives HERE (one flag per registry row, resolved by tag) rather than on the character BP,
	 *  so the flag travels with the identity to every consumer -- store card, showroom, equipped pawn -- and the
	 *  31-identity batch stamps exactly one boolean per row. The character BP declares only WHICH identity it
	 *  belongs to (AAFLCharacterPartActor::BrandColorIdentityTag); the POLICY is this flag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity")
	bool bColorLocked = false;
};

/**
 * UAFLColorIdentityRegistry — the DECLARED color-identity table (the palette).
 *
 * One asset (DA_AFL_ColorIdentityRegistry) lists every color identity. Loaded via AssetManager primary-asset
 * type "AFLColorIdentityRegistry" at the engine-startup scan (same shape as DA_AFL_CosmeticCatalog) so it is
 * resolvable everywhere from frame 0. Collection-level by default (the 5 Neon Edge skins share ONE NeonEdge
 * identity); per-product entries only where a product needs its own (EMP, Visor). Grows IN BULK as products
 * land (per the tracker's asset-batch sequencing) -- the registry expands WITH the catalog.
 *
 * Lives in AFLCosmeticCore (always-loaded) alongside FAFLCatalogEntry + the catalog subsystem -- one home
 * for the cosmetic-identity SSOT, reachable by the store card, the showroom, and the equipped character.
 */
UCLASS(BlueprintType)
class AFLCOSMETICCORE_API UAFLColorIdentityRegistry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Every declared identity. Authored in the editor; resolved by tag at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity", meta = (TitleProperty = "IdentityTag"))
	TArray<FAFLColorIdentity> Identities;

	/** Primary-asset id so AssetManager can find the one registry by type (matches the .ini scan entry). */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("AFLColorIdentityRegistry")), GetFName());
	}

	/** Find the identity for a tag (linear scan -- the table is tiny). Returns nullptr on miss. */
	const FAFLColorIdentity* FindIdentity(const FGameplayTag& IdentityTag) const
	{
		if (!IdentityTag.IsValid())
		{
			return nullptr;
		}
		for (const FAFLColorIdentity& Id : Identities)
		{
			if (Id.IdentityTag == IdentityTag)
			{
				return &Id;
			}
		}
		return nullptr;
	}

	/** CC-6.4: every entry of one kind. Exists so callers filter by DATA rather than repeating the
	 *  catalog cross-reference -- the roster cut needs "the identities" and the creator needs "the
	 *  palette", and both previously had to derive that from a different asset. */
	void GetEntriesOfKind(EAFLColorEntryKind Kind, TArray<const FAFLColorIdentity*>& Out) const
	{
		Out.Reset();
		for (const FAFLColorIdentity& Id : Identities)
		{
			if (Id.EntryKind == Kind) { Out.Add(&Id); }
		}
	}
};
