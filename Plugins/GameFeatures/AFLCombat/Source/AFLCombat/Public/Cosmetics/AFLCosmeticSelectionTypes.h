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
 * CC-7.2 -- THE NINE STICKER ZONES. Fixed, ruled, and ordered so the enum's own order is the
 * replication order and the UI's row order.
 *
 * WHY FIXED AND WHY NINE. A fixed-size set is DIRECTLY REPLICABLE -- nine optional placements ride in
 * one struct with no compact-reference indirection, no array delta, and no ordering ambiguity between
 * client and server. A variable-length sticker list would need all three, and every one of them is a
 * place for the client's view and the server's to disagree about which sticker is where.
 */
UENUM(BlueprintType)
enum class EAFLStickerZone : uint8
{
	ChestLeft    UMETA(DisplayName = "Chest (left)"),
	ChestRight   UMETA(DisplayName = "Chest (right)"),
	Stomach      UMETA(DisplayName = "Stomach"),
	LegFrontLeft UMETA(DisplayName = "Front leg (left)"),
	LegFrontRight UMETA(DisplayName = "Front leg (right)"),
	LegBackLeft  UMETA(DisplayName = "Back leg (left)"),
	LegBackRight UMETA(DisplayName = "Back leg (right)"),
	Back         UMETA(DisplayName = "Back"),
	Face         UMETA(DisplayName = "Face"),

	/** Count sentinel. LAST, always -- anything appended before it renumbers stored values. */
	MAX          UMETA(Hidden)
};

/**
 * CC-7.2 -- ONE STICKER IN ONE ZONE.
 *
 * The placement is a POSITION WITHIN THE ZONE'S OWN UV RECT, expressed in normalised zone space
 * [0,1]x[0,1], NOT in mesh UV space. That is deliberate: the zone rect is the seam boundary, so a
 * placement that cannot leave [0,1] cannot cross a seam by construction. Clamping in mesh-UV space
 * would require every caller to know where the seams are, and the first caller that did not would
 * put a sticker across one.
 */
USTRUCT(BlueprintType)
struct FAFLStickerPlacement
{
	GENERATED_BODY()

	/** The catalog row (AFL.Sticker.*). NAME_None = this zone is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Sticker")
	FName StickerId = NAME_None;

	/** Centre, in NORMALISED ZONE SPACE. Clamped to [0,1] so a sticker cannot cross the zone seam. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Sticker")
	FVector2D Position = FVector2D(0.5, 0.5);

	/** Degrees. Free -- rotation cannot move a sticker out of its rect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Sticker")
	float RotationDegrees = 0.0f;

	/** Fraction of the zone rect. Clamped: below the floor a sticker is invisible and reads as a bug;
	 *  at 1.0 it fills the rect exactly and any larger would spill across the seam. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Sticker")
	float Scale = 0.5f;

	bool IsSet() const { return !StickerId.IsNone(); }

	bool operator==(const FAFLStickerPlacement& O) const
	{
		return StickerId == O.StickerId
			&& Position.Equals(O.Position, 0.0001)
			&& FMath::IsNearlyEqual(RotationDegrees, O.RotationDegrees, 0.01f)
			&& FMath::IsNearlyEqual(Scale, O.Scale, 0.0001f);
	}
	bool operator!=(const FAFLStickerPlacement& O) const { return !(*this == O); }
};

/**
 * CC-7.2 -- THE SHARED PLACEMENT CLAMP.
 *
 * ONE CLAMP, SHARED, exactly as AFLCreatorGamut is for colour. The server clamps on commit and the UI
 * clamps on drag, and BOTH call these -- because two implementations of one rule drift silently, and
 * the player is shown one placement and given another. THE CLIENT NEVER DECIDES A FINAL POSITION.
 */
namespace AFLStickerBounds
{
	/** Below this a sticker is a speck the player cannot see, which reads as a bug rather than a
	 *  choice. Above 1.0 it would spill past the zone rect and cross a seam. */
	static constexpr float MinScale = 0.05f;
	static constexpr float MaxScale = 1.00f;

	inline FVector2D ClampPosition(const FVector2D& In)
	{
		return FVector2D(FMath::Clamp(In.X, 0.0, 1.0), FMath::Clamp(In.Y, 0.0, 1.0));
	}

	inline float ClampScale(const float In)
	{
		return FMath::Clamp(In, MinScale, MaxScale);
	}

	/** Wrapped, not clamped: 370 degrees is 10 degrees, and refusing it would be surprising. */
	inline float NormaliseRotation(const float Degrees)
	{
		const float W = FMath::Fmod(Degrees, 360.0f);
		return W < 0.0f ? W + 360.0f : W;
	}

	/** The whole placement, through one door. */
	inline FAFLStickerPlacement Clamp(const FAFLStickerPlacement& In)
	{
		FAFLStickerPlacement Out = In;
		Out.Position        = ClampPosition(In.Position);
		Out.Scale           = ClampScale(In.Scale);
		Out.RotationDegrees = NormaliseRotation(In.RotationDegrees);
		return Out;
	}
}

/**
 * CC-7.2 -- ALL NINE ZONES, FIXED SIZE, DIRECTLY REPLICABLE.
 *
 * A fixed TStaticArray-shaped set rather than a TArray: nine entries always exist, indexed by
 * EAFLStickerZone, so there is no add/remove ordering to reconcile and an empty zone is a placement
 * with StickerId == None rather than an absent element. Absent-vs-empty ambiguity is the exact trap
 * this programme has paid for repeatedly.
 */
USTRUCT(BlueprintType)
struct FAFLStickerSet
{
	GENERATED_BODY()

	/** Indexed by EAFLStickerZone. Always ZoneCount long -- see EnsureSized. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Sticker")
	TArray<FAFLStickerPlacement> Zones;

	static constexpr int32 ZoneCount = static_cast<int32>(EAFLStickerZone::MAX);

	/** Fixed size is an INVARIANT, restored rather than assumed: a set deserialised from an older
	 *  save could be short, and a short array read by zone index would silently read the wrong zone. */
	void EnsureSized()
	{
		if (Zones.Num() != ZoneCount) { Zones.SetNum(ZoneCount); }
	}

	const FAFLStickerPlacement* Find(const EAFLStickerZone Zone) const
	{
		const int32 I = static_cast<int32>(Zone);
		return Zones.IsValidIndex(I) ? &Zones[I] : nullptr;
	}

	/** Writes ALWAYS go through the shared clamp. There is no unclamped setter, deliberately. */
	void Set(const EAFLStickerZone Zone, const FAFLStickerPlacement& P)
	{
		EnsureSized();
		const int32 I = static_cast<int32>(Zone);
		if (Zones.IsValidIndex(I)) { Zones[I] = AFLStickerBounds::Clamp(P); }
	}

	void ClearZone(const EAFLStickerZone Zone)
	{
		EnsureSized();
		const int32 I = static_cast<int32>(Zone);
		if (Zones.IsValidIndex(I)) { Zones[I] = FAFLStickerPlacement(); }
	}

	int32 NumSet() const
	{
		int32 N = 0;
		for (const FAFLStickerPlacement& P : Zones) { if (P.IsSet()) { ++N; } }
		return N;
	}

	bool operator==(const FAFLStickerSet& O) const
	{
		if (Zones.Num() != O.Zones.Num()) { return false; }
		for (int32 i = 0; i < Zones.Num(); ++i) { if (Zones[i] != O.Zones[i]) { return false; } }
		return true;
	}
	bool operator!=(const FAFLStickerSet& O) const { return !(*this == O); }
};

/**
 * CC-8 -- ACCESSORY HARDPOINTS. Fixed set, same shape and for the same reasons as the sticker zones.
 *
 * A slot names WHERE an accessory hangs, not what it is. The socket each slot maps to is authored on
 * SK_Mannequin -- ONE skeleton, shared by the X line and the Original line alike (measured:
 * SKM_IRONICS_Blank, SKM_Manny and SKM_Quinn all resolve to
 * /Game/Characters/Heroes/Mannequin/Meshes/SK_Mannequin, and ABP_ProMod_FBIK_PP targets that same
 * asset -- FBIK is layered ON it, not a separate skeleton). So slots are authored once and serve both.
 */
UENUM(BlueprintType)
enum class EAFLAccessorySlot : uint8
{
	Head       UMETA(DisplayName = "Head"),
	ShoulderL  UMETA(DisplayName = "Shoulder (left)"),
	ShoulderR  UMETA(DisplayName = "Shoulder (right)"),

	/** Count sentinel. LAST, always. */
	MAX        UMETA(Hidden)
};

/**
 * CC-8 -- ONE ACCESSORY IN ONE SLOT.
 *
 * NO TRANSFORM HERE, DELIBERATELY. The part attaches with SetupAttachment(component, SocketName) and
 * inherits the socket's transform, so offset/rotation belong to the SOCKET, authored once on the
 * skeleton, rather than to per-player data that every accessory would have to get right. Putting a
 * transform here would also invite a bone-transform READ to place it -- and a read has an ordering
 * against the FBIK post-process ABP, which attachment does not.
 */
USTRUCT(BlueprintType)
struct FAFLAccessoryPlacement
{
	GENERATED_BODY()

	/** Catalog row (AFL.Accessory.*). NAME_None = slot empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Accessory")
	FName AccessoryId = NAME_None;

	bool IsSet() const { return !AccessoryId.IsNone(); }
	bool operator==(const FAFLAccessoryPlacement& O) const { return AccessoryId == O.AccessoryId; }
	bool operator!=(const FAFLAccessoryPlacement& O) const { return !(*this == O); }
};

/** CC-8 -- all slots, fixed size, directly replicable. Empty slot = placement with None, never absent. */
USTRUCT(BlueprintType)
struct FAFLAccessorySet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Accessory")
	TArray<FAFLAccessoryPlacement> Slots;

	static constexpr int32 SlotCount = static_cast<int32>(EAFLAccessorySlot::MAX);

	/** Invariant RESTORED, not assumed -- an older save could deserialise short, and a slot-indexed
	 *  read on a short array returns the WRONG SLOT rather than failing. */
	void EnsureSized() { if (Slots.Num() != SlotCount) { Slots.SetNum(SlotCount); } }

	const FAFLAccessoryPlacement* Find(const EAFLAccessorySlot Slot) const
	{
		const int32 I = static_cast<int32>(Slot);
		return Slots.IsValidIndex(I) ? &Slots[I] : nullptr;
	}

	void Set(const EAFLAccessorySlot Slot, const FAFLAccessoryPlacement& P)
	{
		EnsureSized();
		const int32 I = static_cast<int32>(Slot);
		if (Slots.IsValidIndex(I)) { Slots[I] = P; }
	}

	void ClearSlot(const EAFLAccessorySlot Slot)
	{
		EnsureSized();
		const int32 I = static_cast<int32>(Slot);
		if (Slots.IsValidIndex(I)) { Slots[I] = FAFLAccessoryPlacement(); }
	}

	int32 NumSet() const
	{
		int32 N = 0;
		for (const FAFLAccessoryPlacement& P : Slots) { if (P.IsSet()) { ++N; } }
		return N;
	}

	bool operator==(const FAFLAccessorySet& O) const
	{
		if (Slots.Num() != O.Slots.Num()) { return false; }
		for (int32 i = 0; i < Slots.Num(); ++i) { if (Slots[i] != O.Slots[i]) { return false; } }
		return true;
	}
	bool operator!=(const FAFLAccessorySet& O) const { return !(*this == O); }
};

/**
 * CC-8 -- SLOT -> SOCKET. The one place the mapping lives.
 *
 * NAMED, not derived from the enum, because a socket name is content the skeleton must actually carry
 * and an enum member is not. ResolveSocket returns NAME_None for an unmapped slot so a caller FAILS
 * CLOSED -- attaching to NAME_None would silently parent to the component root, putting an accessory
 * at the pawn's feet rather than refusing.
 *
 * NOT YET AUTHORED ON THE SKELETON: see CC-X34. SkeletalMeshSocket::SocketName is read-only from
 * Python and the Sockets array is protected, so these three need the Skeleton editor or a C++ path.
 * The names are fixed here so the code and the eventual sockets cannot disagree.
 */
namespace AFLAccessorySockets
{
	inline FName ResolveSocket(const EAFLAccessorySlot Slot)
	{
		switch (Slot)
		{
			case EAFLAccessorySlot::Head:      return FName(TEXT("accessory_head"));
			case EAFLAccessorySlot::ShoulderL: return FName(TEXT("accessory_clavicle_l"));
			case EAFLAccessorySlot::ShoulderR: return FName(TEXT("accessory_clavicle_r"));
			default: break;
		}
		return NAME_None;   // fails closed
	}
}

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

	/** CC-7.2 -- the nine sticker zones. FIXED SIZE, so it replicates directly: no compact-reference
	 *  indirection, no array delta, no ordering to reconcile between client and server. An empty zone
	 *  is a placement with StickerId == None, NOT an absent element -- absent-vs-empty is the
	 *  ambiguity this programme has paid for more than once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FAFLStickerSet StickerSet;

	/** CC-8 -- accessory hardpoints. Fixed size, same replication shape as StickerSet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Axes")
	FAFLAccessorySet AccessorySet;

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

	/** CC-6.4 -> "BaseTint" (visor base). THE SPLIT DEFERRED AT CC-2.2: mask design and visor colour
	 *  are separate choices, so BaseTint stops tracking body colour and becomes its own channel.
	 *  MIGRATION IS ADDITIVE. While bVisorColorSet is FALSE the visor mirrors CreatorBodyColor exactly
	 *  as before; the split takes effect only when a player actually picks a visor colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Creator")
	FLinearColor CreatorVisorColor = FLinearColor::White;

	/** FALSE = visor mirrors body (pre-CC-6.4 rendering). TRUE = the player chose a visor colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Cosmetic|Creator")
	uint8 bVisorColorSet : 1;

	/** Zeroes the creator bitfield (a UPROPERTY bitfield cannot carry an inline initializer). All other members
	 *  keep their default-member-initializers, so the struct's default is byte-identical to before + overlay OFF. */
	FAFLCosmeticSelection() : bUseCreatorColors(0), bVisorColorSet(0) {}

	/**
	 * THE VISOR COLOUR THAT ACTUALLY RENDERS -- always call this, never read CreatorVisorColor raw.
	 *
	 * MEASURED DEFECT THIS FIXES. CC-6.4 first implemented the mirror inside ResolveInto() and again
	 * inside ServerSetCosmeticSelection() -- once per path that builds a selection. The pull-only
	 * migration proof then measured a THIRD path nobody had mirrored: AFLCosmeticLoadoutComponent.cpp
	 * :136 assigns `Self->Selection = Loaded` straight from persistence and never calls ResolveInto,
	 * so a selection saved before this field existed deserialised at the struct default WHITE. The
	 * probe read (1.0000,1.0000,1.0000) on exactly that world while the authority worlds read correctly.
	 *
	 * A per-path rule is only as good as the enumeration of paths, and that enumeration was wrong on
	 * the first attempt. So the rule lives on the TYPE: CreatorVisorColor means "the explicit choice,
	 * meaningful only while bVisorColorSet", and this accessor is the single place the fallback is
	 * decided. Any future path that materialises a selection inherits the migration for free.
	 */
	FLinearColor EffectiveVisorColor() const
	{
		return bVisorColorSet ? CreatorVisorColor : CreatorBodyColor;
	}

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
 * EmissiveColor. M_Mannequin exposes SEVEN vectors including EdgeGlowColor and TeamColor, but NOT
 * BaseTint -- so for the 32 facemask presets that bind it the creator's VISOR channel is the one that
 * cannot land. (CORRECTED 2026-08-19: an earlier revision of this comment claimed EDGE was inert
 * there. It is not. SchemaProbe measured EdgeGlowColor found=1, BaseTint found=0 on that master.)
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
/**
 * CC-5.2 -- THE NEON GAMUT, AND THE HUE ARC BUILT ON IT.
 *
 * MOVED HERE FROM AFLCosmeticLoadoutComponent.cpp, where it was a private static. The server clamps
 * every committed colour with it; a creator UI must preview with the SAME clamp or it shows the player
 * one colour and commits another. Two implementations of one rule drift, and the drift is silent --
 * the identical failure shape as a seed price disagreeing with a catalog price. ONE definition, used
 * by both sides. The constants and the maths are unchanged from the server's original.
 *
 * WHY A HUE ARC AND NOT RGB SLIDERS. The clamp PRESERVES hue and floors saturation/value
 * (S >= 0.55, V in [0.45, 1.0]). So hue is the only fully free axis: an RGB picker would let a player
 * choose a muddy or near-black colour, show it, and then hand back something visibly different after
 * the server clamped it. An arc over hue offers exactly the freedom that survives the clamp, which is
 * why the roadmap specifies "clamped, not RGB sliders" -- it is honesty about the gamut, not a style
 * preference. The clamp itself is a gameplay requirement (CREATOR_SSOT 6.3): near-black and near-white
 * builds degrade match readability for every other player.
 */
namespace AFLCreatorGamut
{
	// CC-2.1 neon gamut bounds -- the SINGLE source (tune here, never scatter magic numbers at call
	// sites). The X-body master is emissive-heavy, so a low-saturation / low-value pick reads as muddy
	// "no colour"; these floors keep a creator choice legibly neon.
	static constexpr float MinSaturation = 0.55f;
	static constexpr float MinValue      = 0.45f;
	static constexpr float MaxValue      = 1.00f;

	/** Server-authoritative clamp into the neon gamut. Hue preserved; S/V floored and ceilinged. */
	inline FLinearColor ClampToNeon(const FLinearColor& In)
	{
		FLinearColor HSV = In.LinearRGBToHSV();
		HSV.G = FMath::Max(HSV.G, MinSaturation);
		HSV.B = FMath::Clamp(HSV.B, MinValue, MaxValue);
		FLinearColor Out = HSV.HSVToLinearRGB();
		Out.A = 1.0f;
		return Out;
	}

	/** Hue of a colour, in DEGREES [0,360). The arc's read direction: put an existing pick on the arc. */
	inline float HueOf(const FLinearColor& In)
	{
		return In.LinearRGBToHSV().R;
	}

	/**
	 * Re-hue a colour, keeping ITS saturation and value, then clamp. The arc's write direction.
	 *
	 * Deliberately NOT "build a fresh colour at full saturation": a player who has a valid in-gamut
	 * pick and drags the arc expects the hue to move and nothing else. Rebuilding would silently
	 * discard the S/V they already had, which reads as the control fighting them.
	 */
	inline FLinearColor WithHue(const FLinearColor& In, const float HueDegrees)
	{
		FLinearColor HSV = In.LinearRGBToHSV();
		HSV.R = FMath::Fmod(FMath::Fmod(HueDegrees, 360.0f) + 360.0f, 360.0f);
		// CLAMP IN HSV, BEFORE THE ROUND TRIP. The first revision set the hue and then handed the
		// colour to ClampToNeon, which clamps in RGB. For an ACHROMATIC input (S == 0: grey, black,
		// white) hue carries no information through HSV->RGB, so the requested hue was discarded and
		// the saturation floor then produced hue 0 -- RED -- no matter where the player dragged.
		// Measured by afl.Creator.ArcProbe: 12 of 24 cases, every achromatic input, hueOk=0.
		// Applying the floor here keeps S > 0 while the hue is still present, so it survives.
		HSV.G = FMath::Max(HSV.G, MinSaturation);
		HSV.B = FMath::Clamp(HSV.B, MinValue, MaxValue);
		FLinearColor Out = HSV.HSVToLinearRGB();
		Out.A = 1.0f;
		return Out;
	}

	/** A colour at this hue from nothing -- for a channel with no prior pick. Mid-gamut, not extreme. */
	inline FLinearColor FromHue(const float HueDegrees)
	{
		const float H = FMath::Fmod(FMath::Fmod(HueDegrees, 360.0f) + 360.0f, 360.0f);
		return ClampToNeon(FLinearColor(H, 1.0f, 1.0f).HSVToLinearRGB());
	}
}

/** CC-5.2: the creator's four colour channels, as an addressable enum for the link model. */
UENUM(BlueprintType)
enum class EAFLCreatorChannel : uint8
{
	Body  UMETA(DisplayName = "Body"),
	Edge  UMETA(DisplayName = "Edge"),
	Glow  UMETA(DisplayName = "Glow"),
	Visor UMETA(DisplayName = "Visor")
};

/**
 * CC-5.2 -- CHANNEL LINKING. Generic, and DEFAULTED OFF.
 *
 * The roadmap specifies "Neon and Edge linked by default with an unlink toggle". That pairing no
 * longer maps: measurement (CC-X24) established that NeonColor is not a creator channel at all -- it
 * is connected-but-silent behind AlbedoRecolor at 0.0 -- and body colour is DISABLED on the X-line
 * chassis, which leaves Edge and Glow as the two available there.
 *
 * SO THE MECHANISM IS BUILT AND THE PAIRING IS NOT INVENTED. Which channels should move together is a
 * design question the roadmap answers for a channel set that does not exist; guessing a substitute
 * pairing would bake a decision nobody made into shipped behaviour. Default is UNLINKED: every channel
 * independent, no hidden coupling. When the pairing is ruled, it is a default value change here and
 * nothing else.
 */
USTRUCT(BlueprintType)
struct FAFLCreatorChannelLinks
{
	GENERATED_BODY()

	/** Bitmask over EAFLCreatorChannel. 0 = fully unlinked, which is the default and the shipped state
	 *  until a pairing is ruled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Link")
	uint8 LinkedMask = 0;

	static uint8 Bit(const EAFLCreatorChannel Ch) { return static_cast<uint8>(1u << static_cast<uint8>(Ch)); }

	bool IsLinked(const EAFLCreatorChannel Ch) const { return (LinkedMask & Bit(Ch)) != 0; }

	void SetLinked(const EAFLCreatorChannel Ch, const bool bLinked)
	{
		if (bLinked) { LinkedMask |= Bit(Ch); }
		else         { LinkedMask &= static_cast<uint8>(~Bit(Ch)); }
	}

	/** How many channels currently move together. 0 or 1 means dragging one moves only itself. */
	int32 LinkedCount() const
	{
		return (IsLinked(EAFLCreatorChannel::Body)  ? 1 : 0) + (IsLinked(EAFLCreatorChannel::Edge)  ? 1 : 0)
		     + (IsLinked(EAFLCreatorChannel::Glow)  ? 1 : 0) + (IsLinked(EAFLCreatorChannel::Visor) ? 1 : 0);
	}
};

/**
 * CC-5.2 -- IS A CHANNEL REAL ON THIS CHASSIS? THREE ANSWERS, NOT TWO.
 *
 * MEASURED DEFECT THIS FIXES. FAFLCreatorChannelSchema decided availability by asking the master
 * whether the parameter EXISTS. That catches absence, and CC-6.4 proved it (BaseTint absent on
 * M_Mannequin -> visor correctly unavailable). It does NOT catch a parameter that exists and drives
 * nothing: on M_AFL_Character -- the X-line flagship chassis -- TeamColor is present and INERT, with
 * zero downstream consumers. A body control built on "exists" would be offered there and would write
 * to a dead parameter. Same "control that writes nowhere" failure, arriving by the other route.
 *
 * Absent and PresentButInert must stay DISTINCT because the reason is what the UI shows. Collapsing
 * them into "unavailable" loses it, and an unexplained missing control is indistinguishable from a bug
 * -- to the player and to the next developer.
 */
UENUM(BlueprintType)
enum class EAFLChannelAvailability : uint8
{
	/** The master has no such parameter. Measured by the ENGINE's own lookup, so it is correct for any
	 *  master, audited or not. Zero value: an unresolved schema fails closed. */
	Absent           UMETA(DisplayName = "Absent (no such parameter)"),
	/** The parameter EXISTS but is measured to have zero downstream consumers on THIS master. Show the
	 *  control DISABLED with the reason -- never hide it, never let it write. */
	PresentButInert  UMETA(DisplayName = "Present but inert on this master"),
	/** Present and driving visible output. */
	Connected        UMETA(DisplayName = "Connected")
};

/**
 * CC-5.2 -- MEASURED MATERIAL CONNECTIVITY.
 *
 * THIS IS A NAME-KEYED TABLE, AND THE SCHEMA COMMENT BELOW WARNS AGAINST EXACTLY THAT. The warning is
 * right and is not waived: a name-keyed table silently mis-answers for any master not in it. So this
 * table is bounded to the one question the engine CANNOT answer at runtime -- graph connectivity, which
 * is editor-only data -- and its coverage is reported alongside its answers via bMasterAudited. A master
 * that has never been audited does not get to look identical to one measured as fully connected.
 *
 * KEYED ON THE (MASTER, PARAMETER) PAIR, never on the parameter alone. TeamColor is INERT on
 * M_AFL_Character and LIVE on M_Mannequin -- it is the colour axis there. Inertness is a property of the
 * pairing, and a parameter-only table would be wrong for one of those two masters no matter which way
 * it was written.
 *
 * PROVENANCE -- measured, not asserted. Source: the TASK 0 graph-connectivity audit (2026-08-19),
 * recorded in IRONICS_PRICING_SSOT section 7. METHOD, stated so a NEW master is audited the same way
 * rather than guessed at:
 *   1. export the master to T3D;
 *   2. for each vector-parameter expression, follow its output pins transitively;
 *   3. a parameter with no path terminating at a material output is INERT on that master.
 * Re-run that on any master added here, and add it to GAuditedMasters even when nothing is inert --
 * "audited, all connected" and "never audited" are different facts.
 */
namespace AFLMaterialConnectivity
{
	struct FInertPair { const TCHAR* Master; const TCHAR* Parameter; };

	/** Measured inert. ONLY what the audit actually established -- M_Mannequin is deliberately absent
	 *  from this list except where measured, because recording an unmeasured guess here would be the
	 *  very failure this table exists to prevent. */
	static const FInertPair GInertPairs[] =
	{
		{ TEXT("M_AFL_Character"), TEXT("TeamColor")      },
		{ TEXT("M_AFL_Character"), TEXT("EmissiveColor2") },
		{ TEXT("M_AFL_Character"), TEXT("EmissiveColor3") },
	};

	/** Masters the audit has actually covered. Presence here is what licenses a Connected verdict. */
	static const TCHAR* const GAuditedMasters[] =
	{
		TEXT("M_AFL_Character"),
		TEXT("M_Mannequin"),
	};

	inline bool IsMasterAudited(const FName Master)
	{
		for (const TCHAR* M : GAuditedMasters)
		{
			if (Master == FName(M)) { return true; }
		}
		return false;
	}

	inline bool IsInert(const FName Master, const TCHAR* Parameter)
	{
		for (const FInertPair& Pair : GInertPairs)
		{
			if (Master == FName(Pair.Master) && FCString::Stricmp(Parameter, Pair.Parameter) == 0)
			{
				return true;
			}
		}
		return false;
	}
}

USTRUCT(BlueprintType)
struct FAFLCreatorChannelSchema
{
	GENERATED_BODY()

	/** Body colour reaches this chassis. CC-6.4: TeamColor ONLY -- BaseTint moved to the visor channel. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	bool bBodyAvailable = false;

	/** Visor base tint reaches this chassis (BaseTint). FALSE on M_Mannequin, which does not expose it,
	 *  so its 32 facemask presets report the visor control UNAVAILABLE rather than offering one that
	 *  writes nowhere. ResolvedFromMaster is what lets a UI say why. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	bool bVisorAvailable = false;

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

	/** WHY each channel is or is not offered. The bools above answer "can I use it"; these answer
	 *  "and if not, why" -- which is what the UI renders next to a disabled control. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	EAFLChannelAvailability BodyState = EAFLChannelAvailability::Absent;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	EAFLChannelAvailability EdgeState = EAFLChannelAvailability::Absent;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	EAFLChannelAvailability GlowState = EAFLChannelAvailability::Absent;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	EAFLChannelAvailability VisorState = EAFLChannelAvailability::Absent;

	/** FALSE = this master has never been through the connectivity audit, so a Connected verdict on it
	 *  means "present, and inertness UNKNOWN" rather than "measured to render". Carried separately so
	 *  the three channel states stay three, while never letting unaudited pass as verified. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Creator|Schema")
	bool bMasterAudited = false;

	int32 AvailableCount() const
	{
		return (bBodyAvailable ? 1 : 0) + (bEdgeAvailable ? 1 : 0) + (bGlowAvailable ? 1 : 0)
			+ (bVisorAvailable ? 1 : 0);
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
		// CC-6.4 SPLIT: body and visor write different parameters, so the schema stops ORing them.
		// Measured: M_Mannequin has TeamColor but NOT BaseTint (SchemaProbe found=1 / found=0), so it
		// reports body available and visor UNAVAILABLE -- the honest answer for its 32 presets.
		// CC-5.2: existence is the ENGINE's answer and is right for any master; inertness is the
		// AUDIT's answer and is only right for masters it has covered. Both are folded in here, and
		// bMasterAudited reports which masters the second answer actually applies to.
		Out.bMasterAudited = AFLMaterialConnectivity::IsMasterAudited(Out.ResolvedFromMaster);

		auto Classify = [&Out, &HasVector](const TCHAR* ParamName)
		{
			if (!HasVector(ParamName)) { return EAFLChannelAvailability::Absent; }
			return AFLMaterialConnectivity::IsInert(Out.ResolvedFromMaster, ParamName)
				? EAFLChannelAvailability::PresentButInert
				: EAFLChannelAvailability::Connected;
		};

		Out.BodyState  = Classify(TEXT("TeamColor"));
		Out.VisorState = Classify(TEXT("BaseTint"));
		Out.EdgeState  = Classify(TEXT("EdgeGlowColor"));
		Out.GlowState  = Classify(TEXT("EmissiveColor"));

		// USABLE means Connected. PresentButInert is deliberately NOT usable: the control is shown,
		// disabled, with BodyState as the reason -- it must never be allowed to write.
		Out.bBodyAvailable  = (Out.BodyState  == EAFLChannelAvailability::Connected);
		Out.bVisorAvailable = (Out.VisorState == EAFLChannelAvailability::Connected);
		Out.bEdgeAvailable  = (Out.EdgeState  == EAFLChannelAvailability::Connected);
		Out.bGlowAvailable  = (Out.GlowState  == EAFLChannelAvailability::Connected);
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

	/** CC-6.4 visor base tint. Unset = mirrors the body channel, preserving pre-split rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Creator|Build")
	FAFLChannelValue VisorChannel;

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
			|| GlowChannel.Source == EAFLChannelSource::Continuum
			|| VisorChannel.Source == EAFLChannelSource::Continuum;
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
		const bool bAnyColour = BodyChannel.IsSet() || EdgeChannel.IsSet() || GlowChannel.IsSet()
			|| VisorChannel.IsSet();
		if (!bAnyColour)
		{
			return Out; // untouched -- the never-opened-the-creator path
		}
		Out.bUseCreatorColors = 1;
		if (BodyChannel.IsSet()) { Out.CreatorBodyColor = BodyChannel.Resolved; }
		if (EdgeChannel.IsSet()) { Out.CreatorEdgeColor = EdgeChannel.Resolved; }
		if (GlowChannel.IsSet()) { Out.CreatorGlowColor = GlowChannel.Resolved; }
		// VISOR: record an EXPLICIT choice only. The mirror is deliberately NOT applied here -- it lives
		// on the type (EffectiveVisorColor) so every path that materialises a selection gets it, including
		// the persistence load that never calls this function. Mirroring in both places would be two
		// mechanisms for one rule, and it was the un-mirrored third path that actually shipped White.
		if (VisorChannel.IsSet())
		{
			Out.CreatorVisorColor = VisorChannel.Resolved;
			Out.bVisorColorSet = 1;
		}
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
