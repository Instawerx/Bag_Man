// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Templates/SubclassOf.h"
#include "AFLCosmeticCoreTypes.h"   // EAFLCosmeticType + FAFLCatalogEntry (by-value out-param)

#include "AFLW_LoadoutBase.generated.h"

class UAFLCosmeticLoadoutComponent;
class UAFLCosmeticCatalogSubsystem;
class ALyraPlayerState;
class UPanelWidget;
class UButton;
class UAFLW_LoadoutTileBase;
struct FUIInputConfig;

/**
 * UAFLW_LoadoutBase -- the C++ base for the IRONICS LOADOUT / LOCKER (#7).
 *
 * A UCommonActivatableWidget pushed full-screen onto UI.Layer.Menu (mirrors UAFLW_MatchScoreboard --
 * ULyraActivatableWidget is LyraGame-private, so we subclass UCommonActivatableWidget directly and drive
 * Menu input via GetDesiredInputConfig). C++ owns the cosmetic-selection BINDINGS; the WBP child owns the
 * paper-doll layout + AAA styling (the proven AFL split).
 *
 * The keystone: it makes OWNED cosmetics player-selectable in gameplay, retiring cheat-driven selection.
 * It reuses the PROVEN own->apply loop with ZERO new backend --
 *   GetEntriesByType(axis) -> filter IsEntitled (OWNED-only) -> ServerSetCosmeticSelection(copy) ->
 *   OnRep -> Refresh*ForPawn.
 * ServerSetCosmeticSelection is BlueprintAuthorityOnly, so the client MUST dispatch it from C++ (here); a
 * BP node would self-gate on authority. Axis-parameterized (the shared engine the AxisPicker drives):
 * Increment 1 = Weapon. (WeaponSkin has no EAFLCosmeticType enum -> its namespace path lands in Inc 2.)
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_LoadoutBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** OWNED-ONLY feed for one axis: every catalog entry of Axis the local player is entitled to
	 *  (GrantedFree auto-owns; paid requires the owned-set). The AxisPicker renders these as tiles. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout")
	void GetOwnedEntriesForAxis(EAFLCosmeticType Axis, TArray<FAFLCatalogEntry>& OutOwned) const;

	/** The currently-selected CosmeticId for Axis (reads the replicated selection). NAME_None if unset. */
	UFUNCTION(BlueprintPure, Category = "AFL|Loadout")
	FName GetEquippedIdForAxis(EAFLCosmeticType Axis) const;

	/** Equip: copy the current selection, set Axis's field to CosmeticId, dispatch the ONE server RPC.
	 *  Client-safe (dispatches server-side from C++, past ServerSetCosmeticSelection's BP authority gate).
	 *  The server re-validates entitlement, so an unentitled id is a server-side no-op. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout")
	void EquipForAxis(EAFLCosmeticType Axis, FName CosmeticId);

	/** Rebuild the OWNED grid for ActiveAxis: clear TileContainer, spawn a tile per owned entry, mark equipped.
	 *  C++ owns the spawn+bind (the WBP carries zero graph); called on activate + after each equip. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout")
	void RebuildTiles();

	/** The axis this locker screen drives. Increment 1 = Weapon; the WBP sets it per-axis in Inc 2-4. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout")
	EAFLCosmeticType ActiveAxis = EAFLCosmeticType::Weapon;

	/** The tile widget spawned per owned entry (a WBP child of UAFLW_LoadoutTileBase). Set on the WBP. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout")
	TSubclassOf<UAFLW_LoadoutTileBase> TileClass;

protected:
	//~UCommonActivatableWidget / UUserWidget
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End

	/** The panel the tiles spawn into -- the WBP provides it (a ScrollBox/WrapBox/VerticalBox named TileContainer). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> TileContainer;

	/** Optional close button -> DeactivateWidget (pops the locker off the Menu layer). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** A tile was clicked: equip its cosmetic on ActiveAxis, then refresh the grid (EQUIPPED badge). */
	UFUNCTION()
	void HandleTileClicked(FName CosmeticId);

	UFUNCTION()
	void HandleCloseClicked();

	/** The local player's cosmetic loadout component (off the owning PlayerState). Null before the PS exists. */
	UAFLCosmeticLoadoutComponent* GetLoadoutComponent() const;

	/** The local player's PlayerState as ALyraPlayerState (the entitlement-source key). */
	const ALyraPlayerState* GetLyraPlayerState() const;

	/** The GameInstance-scoped cosmetic catalog subsystem. */
	UAFLCosmeticCatalogSubsystem* GetCatalog() const;
};
