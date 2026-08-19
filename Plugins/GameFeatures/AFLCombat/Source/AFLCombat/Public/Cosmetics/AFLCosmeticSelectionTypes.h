// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLCosmeticSelectionTypes.generated.h"

/**
 * CC-3.1 -- A CREATOR CHANNEL VALUE: an id OR a continuum value, with its PROVENANCE recorded.
 *
 * The creator and the store express colour two different ways and the server must validate them
 * differently. A discrete SKU is an ID: it is validated by catalog lookup plus an entitlement check
 * -- does this row exist, and does the player own it. A continuum pick is a VALUE: there is no row
 * to own, so it is validated by clamping into the neon gamut (AFLCreatorGamut::ClampToNeon) exactly
 * as the CC-2 overlay already is. Collapsing the two into one field would force the server to guess
 * which rule applies, and guessing is how an unowned SKU gets equipped.
 *
 * WHY PROVENANCE IS A FIELD AND NOT AN INFERENCE. Given only a colour you cannot tell whether it was
 * bought or picked -- the same ambiguity that has cost this programme repeatedly (a material
 * parameter reading (0,0,0) could not say "absent"; a Type reading SkinColor_Edge could not say
 * "authored"). Source makes the question answerable by reading one field instead of reconstructing
 * history. Never infer Source from whether CosmeticId is set.
 *
 * WHY Resolved IS ALWAYS POPULATED. It is the colour that RENDERS, cached at the moment of choice
 * for BOTH sources. A saved build must render identically after a subscription lapses, after the
 * catalog reprices, and after a row is retired -- with no recomputation and no catalog round-trip.
 * This is the data-level form of the CC-4.2 lapse rule: freeze, never mutate. Re-resolving a build
 * at load time would make a lapsed player's saved robot change appearance underneath them, which is
 * precisely what that rule forbids.
 */
UENUM(BlueprintType)
enum class EAFLChannelSource : uint8
{
	/** Nothing chosen on this channel. Resolved is meaningless; consumers must fall through to preset/baked. */
	Unset       UMETA(DisplayName = "Unset"),
	/** A discrete catalog SKU. CosmeticId is authoritative; server validates existence + entitlement. */
	CatalogId   UMETA(DisplayName = "Catalog SKU"),
	/** A creator continuum pick. There is no row to own; server validates by gamut clamp only. */
	Continuum   UMETA(DisplayName = "Creator continuum")
};

USTRUCT(BlueprintType)
struct FAFLChannelValue
{
	GENERATED_BODY()

	/** WHERE this value came from. The discriminator the server switches its validation rule on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Channel")
	EAFLChannelSource Source = EAFLChannelSource::Unset;

	/** Valid IFF Source == CatalogId. The immutable catalog key, e.g. AFL.Finish.Crimson. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Channel")
	FName CosmeticId = NAME_None;

	/** The colour that RENDERS, for BOTH sources. Cached at choice time; never recomputed on load. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Channel")
	FLinearColor Resolved = FLinearColor::White;

	bool IsSet() const { return Source != EAFLChannelSource::Unset; }

	/** True iff this channel needs an ownership check. A continuum pick owns nothing by construction. */
	bool RequiresEntitlement() const { return Source == EAFLChannelSource::CatalogId; }

	static FAFLChannelValue MakeCatalog(FName InId, const FLinearColor& InResolved)
	{
		FAFLChannelValue V;
		V.Source = EAFLChannelSource::CatalogId;
		V.CosmeticId = InId;
		V.Resolved = InResolved;
		return V;
	}

	static FAFLChannelValue MakeContinuum(const FLinearColor& InResolved)
	{
		FAFLChannelValue V;
		V.Source = EAFLChannelSource::Continuum;
		V.Resolved = InResolved;
		return V;
	}

	bool operator==(const FAFLChannelValue& O) const
	{
		return Source == O.Source && CosmeticId == O.CosmeticId && Resolved.Equals(O.Resolved, 0.0f);
	}
	bool operator!=(const FAFLChannelValue& O) const { return !(*this == O); }
};

/**
 * The identity slot is EITHER/OR (D5/D5b): a player is a Team or a Character, never both, never
 * combined. This discriminator is resolved by TYPE at spawn -- the matching id field is the only
 * one read. (#43)
 */
UENUM(BlueprintType)
enum class EAFLIdentityType : uint8
{
	Team        UMETA(DisplayName = "Team"),
	Character   UMETA(DisplayName = "Character")
};

/**
 * FAFLCosmeticSelection -- the server-authoritative cosmetic selection for one player (#43).
 *
 * A PLAIN replicated USTRUCT (replicated as a single ReplicatedUsing UPROPERTY on
 * UAFLCosmeticLoadoutComponent). DELIBERATELY NOT a FGameplayAbilityTargetData subclass: it is
 * persistent player state, not ability payload. It never serializes its TYPE through
 * FNetSerializeScriptStructCache, so the late-GameFeature-load desync that forces hitscan target
 * data into AFLNetTypes DOES NOT APPLY here -- this struct stays in the AFLCombat GameFeature.
 *
 * Every cosmetic is referenced by its immutable FName CosmeticId (UAFLSkinColorAsset::CosmeticId,
 * format AFL.<Axis>.<Color>). The selection stores KEYS, not asset pointers: the payload is tiny
 * (FNames) and a player can hold a key whose asset isn't currently loaded. The catalog (S-ECON-CAT)
 * resolves key -> asset. An unset axis is NAME_None.
 *
 * SCOPED AXIS BOUNDARY (#43): all five axis fields exist + replicate + are settable now (forward-
 * compatible), but only EdgeId is wired through to the proven SetSkinColor push initially -- Edge is
 * the one axis with a proven propagation path (#38a). Body/Helmet/Weapon/Beam consumers land as each
 * propagation path is proven downstream. This is honest scoping, not a stub: the data is real and
 * replicated; only the per-axis spawn-read consumer is staged.
 */
USTRUCT(BlueprintType)
struct FAFLCosmeticSelection
{
	GENERATED_BODY()

	// --- Identity slot (Team OR Character, type-resolved at spawn) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Identity")
	EAFLIdentityType IdentityType = EAFLIdentityType::Team;

	/** Valid iff IdentityType==Team. Catalog key AFL.Team.<BRAND>. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Identity")
	FName TeamId = NAME_None;

	/** Valid iff IdentityType==Character. Catalog key AFL.Character.<Name>. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Identity")
	FName CharacterId = NAME_None;

	// --- Per-axis cosmetic keys (each an FName CosmeticId; NAME_None = unset) ---

	/** AFL.Edge.<Color>. The one axis wired through to the proven push in #43. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FName EdgeId = NAME_None;

	/** AFL.Body.<Color>. Replicates + settable; consumer lands when the body-color path is proven. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FName BodyId = NAME_None;

	/** AFL.Helmet.<Name>. Axis taxonomy generalizes in S-ECON-CAT. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FName HelmetId = NAME_None;

	/** AFL.Weapon.<Name>. Axis taxonomy generalizes in S-ECON-CAT. RIGHT hand when a dual (arm-worn) pair is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FName WeaponId = NAME_None;

	/** AFL.Weapon.<Name> for the LEFT hand. NAME_None (default) = single-held path (WeaponId is the only weapon,
	 *  every existing gun stays byte-identical). Set ONLY for ARM-WORN Hand-Cannon pairs -> the dual-mount path
	 *  (RefreshHandCannonsForPawn) holds BOTH cannons at once (D2/D3), never unequipping the other hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FName LeftWeaponId = NAME_None;

	/** AFL.WeaponSkin.<Pattern>.<Color>. INDEPENDENT weapon-skin axis (parallel to BeamId): a skin is its OWN
	 *  owned item that applies to ANY equipped weapon, OVERRIDING the weapon's baked original color -- NOT the
	 *  retired per-weapon AFL.Weapon.<W>.<Color> coupling (own one skin, wear it on any gun). Consumer =
	 *  RefreshWeaponSkinForPawn (resolve pattern+color -> the NeonCamo MI -> the weapon-mesh slots). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FName WeaponSkinId = NAME_None;

	/** AFL.Beam.<Name>. Axis taxonomy generalizes in S-ECON-CAT. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FName BeamId = NAME_None;

	/** AFL.Facemask.<Name>. The equipped facemask key. UNLIKE EdgeId/BodyId (param-push via SetSkinColor), a
	 *  facemask is a slot-1 base-MATERIAL swap (the proven MI_AFL_FaceMask_Pink path) -- its consumer is
	 *  UAFLSkinColorControllerComponent::RefreshFacemaskForPawn (resolve CosmeticId -> mask MIC -> swap robot
	 *  slot-1 material), driven on the SAME possession + OnRep + CopyProperties spine as the rest of this
	 *  selection. Composes with the finish param-push (disjoint: finish writes color params into the slot MID,
	 *  facemask swaps the slot's base material -> the controller re-applies the finish AFTER the swap so the
	 *  swap never strands the finish). NAME_None = no facemask equipped (robot's BP-default slot-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FName FacemaskId = NAME_None;

	// --- CREATOR COLOUR OVERLAY (CC-2.1) -----------------------------------------------------------------
	// ADDITIVE, appended after the existing 11 fields. Plain replicated members -> NO custom NetSerialize
	// (this struct stays a plain ReplicatedUsing UPROPERTY). The colours are server-clamped into the neon
	// gamut in ServerSetCosmeticSelection before commit; consumers ignore them unless bUseCreatorColors.

	/** TRUE = the three creator colours below override the resolved body tone. FALSE (default, guaranteed by the
	 *  ctor) = preset/registry tone, byte-identical to before -- the regression guarantee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Creator")
	uint8 bUseCreatorColors : 1;

	/** -> "TeamColor" (body finish base). Ignored unless bUseCreatorColors; server-clamped before commit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Creator")
	FLinearColor CreatorBodyColor = FLinearColor::White;

	/** -> "EdgeGlowColor" (rim glow). Ignored unless bUseCreatorColors; server-clamped before commit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Creator")
	FLinearColor CreatorEdgeColor = FLinearColor::White;

	/** -> "EmissiveColor" (emissive base). Ignored unless bUseCreatorColors; server-clamped before commit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Creator")
	FLinearColor CreatorGlowColor = FLinearColor::White;

	/** Zeroes the creator bitfield (a UPROPERTY bitfield cannot carry an inline initializer). All other members
	 *  keep their default-member-initializers, so the struct's default is byte-identical to before + overlay OFF. */
	FAFLCosmeticSelection() : bUseCreatorColors(0) {}

	/** The active identity key for the current type (TeamId or CharacterId). NAME_None if unset. */
	FName GetActiveIdentityId() const
	{
		return (IdentityType == EAFLIdentityType::Character) ? CharacterId : TeamId;
	}
};
