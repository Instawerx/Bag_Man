// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"

#include "AFLColorIdentityRegistry.generated.h"

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
