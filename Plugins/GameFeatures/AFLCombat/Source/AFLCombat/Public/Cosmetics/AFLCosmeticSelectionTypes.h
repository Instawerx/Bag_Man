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

/**
 * CC-5.1 -- THE CREATOR CHANNEL SCHEMA.
 *
 * ONE SHELL, TWO SCHEMAS. The creator shows a control per channel; which channels a chassis actually
 * HAS depends on the material master its slot-1 visor resolves to. Two screens would double the
 * maintenance for one differing column, so the difference is DATA, not a second UI.
 *
 * WHY THIS TYPE EXISTS AT ALL, and it is not cosmetic tidiness: coverage is master-dependent and was
 * measured, not assumed (SSOT 3.4.1). M_AFL_Visor_Clean and M_AFL_FaceMask_Visor expose BaseTint +
 * EmissiveColor. M_Mannequin exposes TeamColor + EmissiveColor and NEITHER BaseTint NOR EdgeGlowColor,
 * so for the 32 facemask presets that bind it the creator's EDGE channel is silently inert.
 *
 * A UI that offered an Edge control there would be LYING TO THE PLAYER -- they would drag a slider,
 * see nothing change, and have no way to learn why. SetVectorParameterValue on an absent parameter is
 * ignored with no error, so nothing downstream can catch it either. Presenting only the channels that
 * render is the difference between a creator that is honest about its own limits and one that is not.
 *
 * WHAT THIS DOES NOT DO. It does not decide layout, colour, or control style -- those are visual design
 * and belong to IRONICS_UI_STYLE_SSOT.md. It answers exactly one question: for this chassis, which
 * channels are real?
 */
USTRUCT(BlueprintType)
struct FAFLCreatorChannelSchema
{
	GENERATED_BODY()

	/** Body colour reaches this chassis (via BaseTint on a visor master, or TeamColor on M_Mannequin). */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	bool bBodyAvailable = false;

	/** Edge colour reaches this chassis. FALSE on M_Mannequin -- it has no EdgeGlowColor parameter. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	bool bEdgeAvailable = false;

	/** Glow/emissive reaches this chassis. Every master measured so far exposes EmissiveColor. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	bool bGlowAvailable = false;

	/** The master this schema was derived FROM. Carried so a UI can say WHY a channel is missing
	 *  instead of just hiding it -- an unexplained absent control is indistinguishable from a bug. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	FName ResolvedFromMaster = NAME_None;

	int32 AvailableCount() const
	{
		return (bBodyAvailable ? 1 : 0) + (bEdgeAvailable ? 1 : 0) + (bGlowAvailable ? 1 : 0);
	}

	/**
	 * DERIVE FROM THE MATERIAL, NOT FROM A HARDCODED LIST OF MASTER NAMES.
	 *
	 * Asking the material whether it has the parameter means a master that is rewired, renamed, or added
	 * later is handled without touching this code -- and a name-keyed table would silently mis-answer for
	 * any master not in it, which is the same class of defect as the catalog Type default.
	 *
	 * Absence and presence are distinguished by the ENGINE's own parameter lookup rather than by reading
	 * a value: a parameter that is present-but-black and one that is absent both read (0,0,0), and that
	 * ambiguity has already cost this programme twice.
	 */
	static FAFLCreatorChannelSchema DeriveFromMaterial(const UMaterialInterface* Slot1Master)
	{
		FAFLCreatorChannelSchema Out;
		if (!Slot1Master)
		{
			return Out; // nothing bound -> no channels claimed. Fails closed.
		}
		Out.ResolvedFromMaster = FName(*Slot1Master->GetName());

		auto HasVector = [Slot1Master](const TCHAR* ParamName)
		{
			FLinearColor Unused;
			return Slot1Master->GetVectorParameterValue(
				FMaterialParameterInfo(FName(ParamName)), Unused);
		};

		// Body reaches the chassis EITHER way a master can carry it -- BaseTint on the visor masters or
		// TeamColor on M_Mannequin. One channel, two parameter names, because the creator's body colour
		// is written to both (CC-2.2) and only one of them exists on any given master.
		Out.bBodyAvailable = HasVector(TEXT("BaseTint")) || HasVector(TEXT("TeamColor"));
		Out.bEdgeAvailable = HasVector(TEXT("EdgeGlowColor"));
		Out.bGlowAvailable = HasVector(TEXT("EmissiveColor"));
		return Out;
	}
};

/**
 * CC-5.4 -- BUILD NAME MODERATION STATE.
 *
 * The requirement is a VISIBILITY GATE, not a word list: a name must not reach another player until it
 * has been cleared. So the state is what matters, and it is three-valued for the same reason
 * EAFLConditionState is -- "not yet checked" must be distinguishable from "checked and rejected".
 *
 * FAILS CLOSED FOR EXPOSURE. Pending is treated as NOT SHOWABLE. That is the opposite asymmetry to
 * CC-4.1, deliberately: there, withholding a perk from a paying subscriber was the worse error, so
 * penalties failed open. Here the worse error is showing an unvetted name to a stranger, so exposure
 * fails closed. The owner always sees their own name regardless -- withholding it from the person who
 * typed it protects nobody.
 *
 * WHAT THIS DOES NOT DECIDE. Which WORDS are disallowed is product policy, not engineering, and no
 * word list is hardcoded here. This provides structural validation (length, control characters,
 * whitespace, uniqueness) plus the state machine and the report path that a policy filter plugs into.
 * A build whose name has not been through a policy check stays Pending, which is safe by construction.
 */
UENUM(BlueprintType)
enum class EAFLNameState : uint8
{
	/** Structurally valid, not policy-checked. NOT showable to other players. */
	Pending   UMETA(DisplayName = "Pending review"),
	/** Cleared for cross-player display. */
	Approved  UMETA(DisplayName = "Approved"),
	/** Rejected by policy or by a report. Owner still sees it; nobody else does. */
	Rejected  UMETA(DisplayName = "Rejected")
};

/** Structural verdicts, separate from policy. Returned by the validator so a caller can tell the
 *  player WHY a name was refused rather than silently dropping it. */
UENUM(BlueprintType)
enum class EAFLNameVerdict : uint8
{
	Ok              UMETA(DisplayName = "Ok"),
	TooShort        UMETA(DisplayName = "Too short"),
	TooLong         UMETA(DisplayName = "Too long"),
	IllegalCharacter UMETA(DisplayName = "Illegal character"),
	Duplicate       UMETA(DisplayName = "Duplicate name")
};

/**
 * CC-3.2 -- A SAVED CREATOR BUILD.
 *
 * THE INVARIANT THIS EXISTS TO PROTECT: the gameplay spawn path keeps reading ONE
 * FAFLCosmeticSelection, at the same read site, with the same shape. A build is not a second thing
 * gameplay learns to read -- it RESOLVES INTO the selection before any gameplay read happens
 * (ResolveInto below). Nothing downstream of UAFLCosmeticLoadoutComponent::GetSelection() changes,
 * so every proof from CC-1 and CC-2 keeps its meaning.
 *
 * A build is the player-facing unit -- "my robot" -- and it is what a save slot holds. The identity
 * and non-colour axes stay FNames, exactly as the selection stores them, because those are always
 * discrete SKUs. Only the three creator colour channels become FAFLChannelValue, because only they
 * can be either a SKU or a continuum pick.
 */
USTRUCT(BlueprintType)
struct FAFLCreatorBuild
{
	GENERATED_BODY()

	/** Player-authored label. Cosmetic only -- never a key, never resolved against. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Build")
	FString DisplayName;

	/** CC-5.4 moderation state for DisplayName. Pending is NOT showable to other players. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Build")
	EAFLNameState NameState = EAFLNameState::Pending;

	/** True only when this name may be shown to someone who is not its author. */
	bool IsNameShowableToOthers() const { return NameState == EAFLNameState::Approved; }

	/** What a stranger sees. NEVER the raw string unless approved -- the gate lives here, at the one
	 *  place that answers "what do others see", so a new call site cannot forget to check the state. */
	FString GetPublicDisplayName() const
	{
		return IsNameShowableToOthers() ? DisplayName : FString(TEXT("Unnamed Robot"));
	}

	/** The identity and non-colour axes. Always discrete SKUs, so always plain ids. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Build")
	FAFLCosmeticSelection BaseSelection;

	/** The three creator colour channels, each id-or-continuum with provenance (CC-3.1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Build")
	FAFLChannelValue BodyChannel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Build")
	FAFLChannelValue EdgeChannel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Build")
	FAFLChannelValue GlowChannel;

	/** CC-4.2 LAPSE RULE, at the data level: a build beyond the effective slot cap goes READ-ONLY.
	 *  It is never deleted and never mutated -- it renders exactly as saved and refuses edits. Set by
	 *  the server when the cap shrinks; cleared when entitlement restores it. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Build")
	bool bReadOnly = false;

	/** True iff any colour channel is a continuum pick -- i.e. this build needed creator rights to
	 *  author. Used by the lapse rule to decide what locks, WITHOUT re-deriving it from colours. */
	bool UsesContinuum() const
	{
		return BodyChannel.Source == EAFLChannelSource::Continuum
			|| EdgeChannel.Source == EAFLChannelSource::Continuum
			|| GlowChannel.Source == EAFLChannelSource::Continuum;
	}

	/**
	 * RESOLVE INTO THE ONE SELECTION GAMEPLAY READS. Additive by construction: a build whose channels
	 * are all Unset produces a selection byte-identical to BaseSelection, with the creator overlay
	 * OFF -- so a player who never opened the creator is unaffected, and the guarantee is "no write
	 * occurs", not "an identical value is written".
	 *
	 * The resolved colours are copied VERBATIM from the channels. They are NOT recomputed, NOT
	 * re-clamped, and NOT re-looked-up in the catalog: the values were validated when they were
	 * chosen, and CC-4.2 requires a saved build to render identically afterwards regardless of what
	 * happened to the player's entitlements or to the catalog since.
	 */
	FAFLCosmeticSelection ResolveInto() const
	{
		FAFLCosmeticSelection Out = BaseSelection;
		const bool bAnyColour = BodyChannel.IsSet() || EdgeChannel.IsSet() || GlowChannel.IsSet();
		if (!bAnyColour)
		{
			return Out; // untouched -- the never-opened-the-creator path
		}
		Out.bUseCreatorColors = 1;
		if (BodyChannel.IsSet()) { Out.CreatorBodyColor = BodyChannel.Resolved; }
		if (EdgeChannel.IsSet()) { Out.CreatorEdgeColor = EdgeChannel.Resolved; }
		if (GlowChannel.IsSet()) { Out.CreatorGlowColor = GlowChannel.Resolved; }
		// A channel sourced from a catalog SKU also carries its id onto the matching axis, so the
		// existing preset/registry path still sees the SKU it expects.
		if (BodyChannel.Source == EAFLChannelSource::CatalogId) { Out.BodyId = BodyChannel.CosmeticId; }
		if (EdgeChannel.Source == EAFLChannelSource::CatalogId) { Out.EdgeId = EdgeChannel.CosmeticId; }
		return Out;
	}
};

/**
 * CC-3.2 -- THE SAVED SET. Builds plus which one is live.
 *
 * ActiveBuildIndex is an INDEX, not a copy: there is exactly one live build and no way for the
 * active build to drift from the saved one. INDEX_NONE means no build is active, in which case the
 * player's plain FAFLCosmeticSelection is used unchanged -- the pre-Stage-B behaviour, preserved.
 */
USTRUCT(BlueprintType)
struct FAFLCreatorBuildSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Build")
	TArray<FAFLCreatorBuild> Builds;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Build")
	int32 ActiveBuildIndex = INDEX_NONE;

	bool HasActive() const { return Builds.IsValidIndex(ActiveBuildIndex); }

	const FAFLCreatorBuild* GetActive() const
	{
		return HasActive() ? &Builds[ActiveBuildIndex] : nullptr;
	}
};
