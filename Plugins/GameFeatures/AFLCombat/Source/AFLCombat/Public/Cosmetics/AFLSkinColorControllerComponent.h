// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "AFLSkinColorControllerComponent.generated.h"

class APawn;
class UAFLSkinColorAsset;
class UAFLBrandEdgeMap;
class ULyraEquipmentInstance;

/**
 * Controller-side PERSISTENT home for the robot-skin color selection (L5, Option F).
 *
 * Lives on the Controller -> survives pawn death (unlike the pawn's UAFLSkinColorComponent, which dies
 * with the pawn). On each possession it re-pushes PersistentSkinColor onto the new pawn's
 * UAFLSkinColorComponent (authority) -> sets the replicated SkinColor -> all clients converge. This is
 * what makes the skin survive respawn. Standalone subclass of the engine UControllerComponent (exported);
 * we do NOT subclass the module-private ULyraControllerComponent_CharacterParts.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLSkinColorControllerComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UAFLSkinColorControllerComponent(const FObjectInitializer& ObjectInitializer);

	/** AUTHORITY-ONLY: set the persistent color (survives respawn) + push to the current pawn now. */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "AFL|Cosmetics")
	void SetPersistentSkinColor(UAFLSkinColorAsset* NewColor);

	UFUNCTION(BlueprintPure, Category = "AFL|Cosmetics")
	UAFLSkinColorAsset* GetPersistentSkinColor() const { return PersistentSkinColor; }

	/** AUTHORITY: re-resolve this pawn's brand edge from its CURRENT parts and push it onto the pawn's
	 *  UAFLSkinColorComponent. The single resolve+push body -- the possess/set paths AND the part-arrival
	 *  hook (AAFLCharacterPartActor::BeginPlay on a runtime robot swap) both call THIS, so a swap re-reads
	 *  the new robot's brand tag without waiting for re-possession. Propagation is unchanged (SetSkinColor
	 *  only). Safe to call redundantly (idempotent). #38a. */
	void RefreshSkinForPawn(APawn* Pawn) const;

	/** AUTHORITY: resolve the player's equipped FacemaskId (from the PlayerState loadout selection, catalog
	 *  resolveVia) to its mask MIC and SWAP the possessed robot's slot-1 base material to it -- the proven
	 *  facemask path (MI_AFL_FaceMask_Pink is a slot-1 base-MI cosmetic, NOT a param spray). Server-auth: the
	 *  swap is applied via the pawn component's replicated facemask value so all clients converge (mirrors the
	 *  SkinColor replicate-then-apply spine). MUST run BEFORE RefreshSkinForPawn at each possession so the
	 *  finish param-push re-MIDs the SWAPPED material and lands its color params on top (composition order:
	 *  material swap, then param push -- the swap never strands the finish). NAME_None facemask -> no-op (the
	 *  robot keeps its BP-default slot-1). Idempotent. Mirrors RefreshSkinForPawn's resolve+apply shape. */
	void RefreshFacemaskForPawn(APawn* Pawn) const;

	/**
	 * AUTHORITY (server-only): resolve the player's selected WeaponId (PlayerState loadout -> catalog -> a
	 * UAFLWeaponCosmeticAsset carrier -> its EquipmentDefinition) and EQUIP it on the pawn's
	 * ULyraEquipmentManagerComponent, REPLACING the current primary (D2: own the instance, no stacking). Lyra's
	 * FLyraEquipmentList fast-array replicates the equip to every client -- no client push here (mirrors the
	 * SetSkinColor spine's server-auth-then-replicate shape). Idempotent: a re-run for the same WeaponId + a live
	 * instance is a no-op, so the dual spine (possession + OnRep + nudge) never stacks. NAME_None -> our selection
	 * is removed. #43 WeaponId consumer -- closes the "replicates but nothing consumes" axis; completes own->
	 * select->equip->fire. NOT const (it tracks the equipped instance).
	 *
	 * D3 DEBT (tracked, not now): this component now consumes skin + facemask + weapon -> it is really a
	 * UAFLCosmeticControllerComponent; the "SkinColor" name lags. Rename deferred (churn), logged as debt.
	 * D5 HORIZON: direct EquipItem mirrors AFLHeroComponent (proven). Weapon-switching (primary/secondary/pickups
	 * = the QuickBar path) is genre-core for an extraction shooter -- NEAR-horizon. The equip/replace logic is
	 * isolated in RefreshWeaponForPawn so the QuickBar transition swaps THIS body, not the spine.
	 */
	void RefreshWeaponForPawn(APawn* Pawn);

	/** AUTHORITY-spine (const; NOT authority-gated in the body -- SetWeaponSkin's HasAuthority guard + OnRep
	 *  handle clients, exactly like RefreshFacemaskForPawn): resolve the selected WeaponId's ".<Color>" suffix
	 *  -> the NeonCamo color MI (/Game/Weapons/AFL/Skins/MI_AFL_WeaponSkin_NeonCamo_<Color>) and push it to the
	 *  pawn's UAFLSkinColorComponent replicated WeaponSkin (all clients converge via OnRep_WeaponSkin ->
	 *  ApplyWeaponSkinToEquipped). MIRRORS RefreshFacemaskForPawn for the weapon COLOR axis. MUST run AFTER
	 *  RefreshWeaponForPawn on the spine (the weapon must be equipped before the color applies). No suffix / no
	 *  MI -> null push (the weapon keeps its baked default). Completes the #43 WeaponId generalization (the
	 *  equip-half is RefreshWeaponForPawn; this is the color-half). Idempotent. */
	void RefreshWeaponSkinForPawn(APawn* Pawn) const;

protected:
	virtual void BeginPlay() override;

	/** Authority-side selection state. NOT replicated -- the pawn component replicates the active value.
	 *  EditDefaultsOnly so a per-component-class default color can be authored in the details panel (the
	 *  GameFeatureAction-added component carries this default; the gate uses it for ARIA = Green). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Cosmetics")
	TObjectPtr<UAFLSkinColorAsset> PersistentSkinColor = nullptr;

	/** Brand -> default-edge map (#38a). When set, RefreshSkinForPawn resolves the possessed robot's
	 *  Cosmetic.Brand.* tag through this map to pick the brand's factory-default edge; on miss (no tag /
	 *  unmapped / asset unset) it falls back to PersistentSkinColor. Set in the details panel (step 2).
	 *  EditDefaultsOnly: the GameFeatureAction-added component carries this as a per-class default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Cosmetics")
	TObjectPtr<UAFLBrandEdgeMap> BrandEdgeMap = nullptr;

	/** Base facemask equipped when the player has NO facemask selection (FacemaskId == None). Mirrors
	 *  BrandEdgeMap's role for the finish -- a configured DATA ASSET resolved DIRECTLY (not by CosmeticId), so
	 *  the base visor shows regardless of the facemask catalog's id state. Defaults (ctor) to the IRONICS HUD
	 *  visor (DA_AFL_Facemask_IroVisor / T_AFL_Visor_Ironics) per SSOT player-flow 9.2. EditDefaultsOnly
	 *  (overridable in the details, like BrandEdgeMap); soft so it isn't force-loaded until a fallback fires.
	 *  Empty -> the old un-equip (bare-head) behavior. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Cosmetics")
	TSoftObjectPtr<UAFLSkinColorAsset> BaseFacemask;

	/** Bound (authority) to the controller's public OnPossessedPawnChanged; re-pushes color to the new pawn. */
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

private:
	/** #38a: read the possessed robot's Cosmetic.Brand.* tag off its AAFLCharacterPartActor parts (via the
	 *  existing IGameplayTagAssetInterface). Invalid tag if the pawn has no brand-tagged body part. */
	FGameplayTag ResolveBrandTag(APawn* Pawn) const;

	// --- #43 WeaponId consumer state (D2: own the selected instance, replace the primary, no stacking) ---
	// Server-only, NOT replicated (the equip itself replicates via Lyra's FLyraEquipmentList fast-array). Tracked
	// per-pawn: the instance dies with its pawn, so WeaponTrackedPawn guards a stale cross-pawn read -- a respawn
	// resets tracking and the new pawn re-equips clean.
	TWeakObjectPtr<APawn> WeaponTrackedPawn;
	TWeakObjectPtr<ULyraEquipmentInstance> SelectedWeaponInstance;
	/** The WeaponId currently realized on WeaponTrackedPawn (idempotency key; NAME_None = none). */
	FName EquippedWeaponId = NAME_None;
};
