// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "AFLSkinColorComponent.generated.h"

class UAFLSkinColorAsset;
class UMaterialInstanceConstant;

// Skin-color race diagnostics (cvar-gated `afl.SkinDiag`, OFF by default). Shared by the part actor,
// the pawn component, and the controller component so all log lines share one category + format. Defined
// in AFLSkinColorComponent.cpp. This is a PERMANENT diagnostic, not temp scaffolding -- it is inert (zero
// log output) unless `afl.SkinDiag 1` is set, so it does not affect shipping or normal PIE.
AFLCOMBAT_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLSkinDiag, Log, All);

namespace AFLSkinDiag
{
	/** True when `afl.SkinDiag` > 0 (game thread). Gate every diagnostic block on this. */
	AFLCOMBAT_API bool IsOn();

	/** Shared line prefix: "[SkinDiag][SRV|CLI][f=<GFrameCounter>] " resolved from WorldContext's net mode. */
	AFLCOMBAT_API FString Prefix(const UObject* WorldContext);
}

/**
 * Pawn-side replicated COLOR selection for the robot skin (L5, Option F composition).
 *
 * Holds the replicated SkinColor and applies it to the pawn's AAFLCharacterPartActor body parts via
 * direct engine material calls (no unexported Lyra symbol, no reflection). Standalone UActorComponent --
 * we do NOT subclass the module-private ULyraPawnComponent_CharacterParts; we observe its result (the
 * spawned part actors) by class-filtered iteration.
 *
 * Race-safety (two channels: the parts FastArray on the stock component vs this SkinColor UPROPERTY):
 *  - PATH 1 (part-arrives-second): each AAFLCharacterPartActor self-colors on its BeginPlay (reads GetSkinColor()).
 *  - PATH 2 (color-arrives-second): OnRep_SkinColor -> ReapplyColorToAllParts() pushes to already-spawned parts
 *    that read null at their BeginPlay. LOAD-BEARING -- without it, color-after-part desyncs on the wire.
 *  Both idempotent (the part's owned-MID create-once) -> safe to fire redundantly when both land close together.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLSkinColorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLSkinColorComponent();

	/** AUTHORITY-ONLY: set the active color on the server. Replicates to all clients. */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "AFL|Cosmetics")
	void SetSkinColor(UAFLSkinColorAsset* NewColor);

	/** Read by the part actor on its BeginPlay (PATH 1). */
	UAFLSkinColorAsset* GetSkinColor() const { return SkinColor; }

	/** AUTHORITY-ONLY: set the active BODY finish (TeamColor axis) on the server. Replicates PARALLEL to
	 *  SkinColor (the edge axis) -- same two-path race-safe spine. The body Finish drives the TeamColor; the
	 *  edge (SkinColor) overlays its emissive on top (composition: body first, edge wins the shared keys). */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "AFL|Cosmetics")
	void SetBodyColor(UAFLSkinColorAsset* NewColor);

	/** Read by the part actor on its BeginPlay (PATH 1) -- the body finish (TeamColor axis). nullptr = none. */
	UAFLSkinColorAsset* GetBodyColor() const { return BodyColor; }

	/** AUTHORITY-ONLY: set the equipped facemask MIC on the server. Replicates to all clients (mirrors
	 *  SetSkinColor exactly). The facemask is a slot-1 base-MATERIAL swap (the proven MI_AFL_FaceMask_Pink
	 *  path), DISTINCT from the SkinColor param-push: this swaps the base material on slot 1 of each body
	 *  part; SkinColor then layers its color params on top (the part's owned-MID re-apply re-MIDs the swapped
	 *  material). NewMaterial==nullptr clears the facemask (slot 1 falls back to the part's authored material).
	 *  A UMaterialInstanceConstant (content asset) -> replication-safe by pointer (never a transient MID). */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "AFL|Cosmetics")
	void SetFacemask(UMaterialInstanceConstant* NewMaterial);

	/** Read by the part actor on its BeginPlay (PATH 1) so a part arriving AFTER the facemask value still
	 *  picks it up. nullptr = no facemask equipped. */
	UMaterialInstanceConstant* GetFacemask() const { return Facemask; }

	/** AUTHORITY-ONLY: set the equipped weapon's skin MI on the server. Replicates to all clients (MIRRORS
	 *  SetFacemask exactly, for the weapon axis). Applied to the equipped weapon mesh's material slots (the
	 *  48-color NeonCamo MIs off the locked master). nullptr = no override (the weapon keeps its baked original).
	 *  Driven by the INDEPENDENT WeaponSkinId axis (RefreshWeaponSkinForPawn) -- a skin applies to ANY weapon. */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "AFL|Cosmetics")
	void SetWeaponSkin(UMaterialInstanceConstant* NewMaterial);

	/** Read by the re-apply paths; nullptr = no weapon-skin override. */
	UMaterialInstanceConstant* GetWeaponSkin() const { return WeaponSkinMaterial; }

	/** AUTHORITY-ONLY: set the equipped weapon's BEAM color asset on the server. Replicates to all clients
	 *  (MIRRORS SetWeaponSkin, for the INDEPENDENT BeamId axis -- FAFLCosmeticSelection.BeamId, DECOUPLED from
	 *  the weapon-skin selection). A beam is its OWN owned item (choose-2-from-catalog, IRONICS_PLAYER_FLOW):
	 *  the selected beam applies to ANY equipped weapon, OVERRIDING its default beam, EXCEPT a weapon whose
	 *  bLockedSignatureBeam is set (special guns keep their signature beam). nullptr = no beam override. */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "AFL|Cosmetics")
	void SetBeamColor(UAFLSkinColorAsset* NewBeamColor);

	/** Read by the re-apply paths; nullptr = no beam override (weapon keeps its default beam). */
	UAFLSkinColorAsset* GetBeamColor() const { return BeamColorAsset; }

protected:
	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of UActorComponent interface

	UPROPERTY(ReplicatedUsing = OnRep_SkinColor)
	TObjectPtr<UAFLSkinColorAsset> SkinColor = nullptr;

	UFUNCTION()
	void OnRep_SkinColor();

	/** PATH 2: push the current color to all already-spawned AAFLCharacterPartActor body parts. Idempotent. */
	void ReapplyColorToAllParts();

	/** The BODY finish -- replicated PARALLEL to SkinColor (the edge axis), same race-safe two-path spine.
	 *  Drives the TeamColor axis. A content asset (UAFLSkinColorAsset) -> safe to replicate by pointer. nullptr = none. */
	UPROPERTY(ReplicatedUsing = OnRep_BodyColor)
	TObjectPtr<UAFLSkinColorAsset> BodyColor = nullptr;

	UFUNCTION()
	void OnRep_BodyColor();

	/** PATH 2 (body): re-apply BodyColor (Finish: TeamColor + emissive) THEN SkinColor (edge: emissive overlays)
	 *  to all spawned parts -- the composition LAYER order (body first, edge wins shared keys). Idempotent. */
	void ReapplyBodyColorToAllParts();

	/** The equipped facemask MIC -- replicated PARALLEL to SkinColor, same race-safe two-path spine. A content
	 *  asset (UMaterialInstanceConstant) -> safe to replicate by pointer. nullptr = none equipped. */
	UPROPERTY(ReplicatedUsing = OnRep_Facemask)
	TObjectPtr<UMaterialInstanceConstant> Facemask = nullptr;

	UFUNCTION()
	void OnRep_Facemask();

	/** PATH 2 (facemask): swap slot-1 base material to Facemask on all spawned body parts, THEN re-apply the
	 *  current SkinColor so the finish params land on top of the swapped material (composition order). The part
	 *  exposes the swap+recolor via AAFLCharacterPartActor::ApplyFacemask. Idempotent. */
	void ReapplyFacemaskToAllParts();

	/** The equipped weapon's skin MI -- replicated PARALLEL to Facemask, same race-safe two-path spine. A content
	 *  asset (UMaterialInstanceConstant) -> safe to replicate by pointer. nullptr = no override (baked default). */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponSkin)
	TObjectPtr<UMaterialInstanceConstant> WeaponSkinMaterial = nullptr;

	UFUNCTION()
	void OnRep_WeaponSkin();

	/** PATH 2 (weapon-skin): apply WeaponSkinMaterial to the equipped weapon mesh's material slots. MIRRORS
	 *  ReapplyFacemaskToAllParts, but the target is the equipped weapon actor (found via the pawn's
	 *  ULyraEquipmentManagerComponent -> the ranged-weapon instance -> its spawned actor -> SkeletalMesh),
	 *  NOT the pawn's body parts. Null WeaponSkinMaterial = no-op (keep the weapon's baked default). Idempotent. */
	void ApplyWeaponSkinToEquipped();

	/** The equipped weapon's BEAM color -- replicated PARALLEL to WeaponSkinMaterial (same race-safe two-path
	 *  spine), for the INDEPENDENT BeamId axis. A content asset (UAFLSkinColorAsset carrying the beam tint in
	 *  ColorParameters["BeamColor"]) -> replication-safe by pointer, exactly like SkinColor/BodyColor. nullptr =
	 *  no beam override. */
	UPROPERTY(ReplicatedUsing = OnRep_BeamColor)
	TObjectPtr<UAFLSkinColorAsset> BeamColorAsset = nullptr;

	UFUNCTION()
	void OnRep_BeamColor();

	/** PATH 2 (beam-color): reflection-WRITE LaserTintColor = (the asset's ColorParameters["BeamColor"], A=1)
	 *  onto the equipped weapon INSTANCE(s) -- the ability/cue re-reads LaserTintColor on the next fire (no
	 *  live-beam surgery; the exact seam AFLLaserVisualStatics::ReadLaserTint reflects). MIRRORS
	 *  ApplyWeaponSkinToEquipped's reach (the pawn's ULyraEquipmentManagerComponent -> ranged-weapon instances)
	 *  but the target is the INSTANCE's LaserTintColor (the beam seam), NOT the actor mesh. A weapon whose
	 *  bLockedSignatureBeam=true is SKIPPED (its signature beam is locked). Null asset = no-op. Idempotent. */
	void ApplyBeamColorToEquipped();
};
