// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AFLCosmeticSelectionTypes.generated.h"

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

	/** AFL.Weapon.<Name>. Axis taxonomy generalizes in S-ECON-CAT. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FName WeaponId = NAME_None;

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

	/** The active identity key for the current type (TeamId or CharacterId). NAME_None if unset. */
	FName GetActiveIdentityId() const
	{
		return (IdentityType == EAFLIdentityType::Character) ? CharacterId : TeamId;
	}
};
