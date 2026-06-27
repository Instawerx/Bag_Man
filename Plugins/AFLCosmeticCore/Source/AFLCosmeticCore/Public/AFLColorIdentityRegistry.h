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

	/** BODY axis: body base shade (Finish presets write this; Edge presets have no TeamColor param). */
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
		if (ParamName == NEmissive1) { return &EmissiveColor1; }
		if (ParamName == NEmissive2) { return &EmissiveColor2; }
		if (ParamName == NEmissive3) { return &EmissiveColor3; }
		if (ParamName == NEdgeGlow)  { return &EdgeGlowColor; }
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
USTRUCT(BlueprintType)
struct FAFLColorIdentity
{
	GENERATED_BODY()

	/** The identity key. Format Cosmetic.Identity.<Name> (e.g. Cosmetic.Identity.NeonEdge). Items point at
	 *  this via FAFLCatalogEntry.ColorIdentityTag; the registry resolves it to the colors below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|ColorIdentity", meta = (Categories = "Cosmetic.Identity"))
	FGameplayTag IdentityTag;

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
};
