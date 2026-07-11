// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Templates/SubclassOf.h"
#include "AFLCosmeticCoreTypes.h"    // EAFLCosmeticType + FAFLCatalogEntry (by-value out-param)
#include "UI/AFLW_LoadoutTileBase.h" // EAFLLoadoutAxis + UAFLW_LoadoutTileBase

#include "AFLW_LoadoutBase.generated.h"

class UAFLCosmeticLoadoutComponent;
class UAFLCosmeticCatalogSubsystem;
class ALyraPlayerState;
class UPanelWidget;
class UButton;
class UImage;
class UTextureRenderTarget2D;
class ASceneCapture2D;
class APawn;
class AAFLLoadoutPod;
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
	 *  (GrantedFree auto-owns; paid requires the owned-set). Weapon/WeaponSkin both query Type==Weapon and
	 *  split by the AFL.Weapon. / AFL.WeaponSkin. namespace; Beam queries Type==Beam. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout")
	void GetOwnedEntriesForAxis(EAFLLoadoutAxis Axis, TArray<FAFLCatalogEntry>& OutOwned) const;

	/** The currently-selected CosmeticId for Axis (reads the replicated selection). NAME_None if unset. */
	UFUNCTION(BlueprintPure, Category = "AFL|Loadout")
	FName GetEquippedIdForAxis(EAFLLoadoutAxis Axis) const;

	/** Equip: copy the selection, seed the free IRONICS identity if none (else _Validate drops the RPC), set
	 *  Axis's field to CosmeticId, dispatch ServerSetCosmeticSelection from C++ (past the BlueprintAuthorityOnly
	 *  gate). The server re-validates entitlement, so an unentitled id is a server-side no-op. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout")
	void EquipForAxis(EAFLLoadoutAxis Axis, FName CosmeticId);

	/** Rebuild EVERY axis grid (weapon + skin + beam) from the current owned-set + selection. C++ owns the
	 *  spawn+bind (the WBP carries zero graph); called on activate + after each equip. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout")
	void RebuildTiles();

	/** The tile widget spawned per owned entry (a WBP child of UAFLW_LoadoutTileBase). Set on the WBP. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout")
	TSubclassOf<UAFLW_LoadoutTileBase> TileClass;

protected:
	//~UCommonActivatableWidget / UUserWidget
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End

	/** WEAPON-axis tile grid (BindWidget: the WBP names its ScrollBox TileContainer). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> TileContainer;

	/** Weapon-SKIN tile grid (Increment 2). Optional so the Inc-1 WBP still binds. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SkinTileContainer;

	/** BEAM tile grid (Increment 2). Optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> BeamTileContainer;

	/** IDENTITY / BODY / EDGE / FACEMASK tile grids (Increment 3). All optional. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> IdentityTileContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> BodyColorTileContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> EdgeColorTileContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> FacemaskTileContainer;

	/** Optional close button -> DeactivateWidget (pops the locker off the Menu layer). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** Center-stage 3D preview -- a UImage showing a live SceneCapture of the REAL local pawn (Approach A:
	 *  the preview IS the pawn the authoritative/replicated apply path already updates, so it cannot lie). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PreviewImage;

	/** Front-3/4 framing: (forward, right, up) offset of the capture camera from the pawn, pawn-local. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	FVector PreviewCamOffset = FVector(300.f, 120.f, 55.f);

	/** The capture camera looks at this pawn-local point (roughly the chest). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	FVector PreviewFocusOffset = FVector(0.f, 0.f, 40.f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	float PreviewFOV = 38.f;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	FIntPoint PreviewResolution = FIntPoint(512, 768);

	/** The reusable kiosk-pod diorama actor staged around the previewed hero (Increment C). Null -> the C++
	 *  AAFLLoadoutPod placeholder; override with a branded BP child in the WBP for the SM_AFL_LoadoutPod swap. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	TSubclassOf<AAFLLoadoutPod> PodClass;

	/** A tile was clicked: equip its cosmetic on Axis, then refresh the grids (EQUIPPED badge). */
	UFUNCTION()
	void HandleTileClicked(EAFLLoadoutAxis Axis, FName CosmeticId);

	UFUNCTION()
	void HandleCloseClicked();

	/** The local player's cosmetic loadout component (off the owning PlayerState). Null before the PS exists. */
	UAFLCosmeticLoadoutComponent* GetLoadoutComponent() const;

	/** The local player's PlayerState as ALyraPlayerState (the entitlement-source key). */
	const ALyraPlayerState* GetLyraPlayerState() const;

	/** The GameInstance-scoped cosmetic catalog subsystem. */
	UAFLCosmeticCatalogSubsystem* GetCatalog() const;

private:
	/** Spawn the OWNED tiles for one axis into its container (the parameterized engine, called per-axis). */
	void RebuildAxisTiles(EAFLLoadoutAxis Axis, UPanelWidget* Container);

	/** The local player's current pawn (the REAL pawn the preview captures). */
	APawn* GetLocalPawn() const;

	/** Spawn/attach the SceneCapture rig framing the pawn + route its render target into PreviewImage. */
	void SetupPreviewCapture();

	/** Isolate the robot: set the capture's ShowOnlyList to the pawn + its attached actors (the equipped
	 *  weapon changes as you pick) so it renders on the clean backdrop, not the arena. Cheap; called per-tick. */
	void RefreshPreviewShowList();

	/** Re-position the capture from the afl.Loadout.Preview* cvars each tick -> LIVE framing tuning (no rebuild). */
	void RepositionPreviewCamera();

	/** Destroy the SceneCapture rig (on deactivate). */
	void TeardownPreviewCapture();

	/** The scene-capture actor framing the pawn (attached to it; captures every frame -> live). */
	TWeakObjectPtr<ASceneCapture2D> PreviewCapture;

	/** The kiosk-pod diorama actor spawned attached to the pawn -> rendered INSIDE the preview via the
	 *  ShowOnlyList (Increment C). Destroyed with the capture on deactivate. */
	TWeakObjectPtr<AAFLLoadoutPod> PreviewPod;

	/** The render target the capture writes + PreviewImage displays (runtime, transient). */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PreviewRT;
};
