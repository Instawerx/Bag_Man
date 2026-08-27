// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Templates/SubclassOf.h"
#include "AFLCosmeticCoreTypes.h"    // EAFLCosmeticType + FAFLCatalogEntry (by-value out-param)
#include "Cosmetics/AFLCosmeticSelectionTypes.h" // FAFLCosmeticSelection (display-pawn change tracking)
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
class AAFLLoadoutDisplayPawn;
class UAFLCharacterPartMap;
class UAFLSkinColorControllerComponent;
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

	/** Store-card treatment on the tiles (rarity frame + neon-pipe EQUIP button, store parity). Default OFF so the
	 *  in-match locker stays plain; the FRONT-END locker WBP (WBP_AFL_Loadout) sets this TRUE. Flip the in-match
	 *  WBP only after the operator watches the front-end. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout")
	bool bStoreCardStyle = false;

	/**
	 * CC-5 ENTRY POINT: the creator widget this loadout opens.
	 *
	 * A SOFT CLASS POINTER, NOT A RUNTIME LoadClass PATH. A hardcoded `/Game/...` string resolves fine
	 * in PIE and is DROPPED BY THE COOK -- measured on this project, and it never reproduces in the
	 * editor, which is what makes it dangerous. A UPROPERTY soft reference is a real dependency the
	 * cooker follows. Defaulted in the constructor so it works out of the box; a WBP may override it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Creator")
	TSoftClassPtr<class UAFLW_Creator> CreatorWidgetClass;

	/**
	 * Open the creator on this loadout. THE ONLY SHIPPING PATH INTO IT -- until now every reference
	 * lived in AFLCombatCheats.cpp.
	 *
	 * Entry is from the loadout deliberately: UAFLW_Creator::InitializeCreator takes a
	 * UAFLW_LoadoutBase*, so the coupling is designed in -- the loadout owns the preview pawn the
	 * creator rotates, and a main-menu entry would have to synthesise a context the loadout already
	 * holds.
	 *
	 * Returns the pushed creator, or null with a REASON logged. Never fails silently: an entry point
	 * that quietly does nothing is indistinguishable from a button that is not wired.
	 */
	/**
	 * Open the creator ON A BUILD. THE MAIN DOOR.
	 *
	 * @param BuildIndex   which saved build to edit; INDEX_NONE starts a NEW one (the append case, and
	 *                     the only case the slot cap gates).
	 * @param FocusAxis    UNSET means no axis is focused -- the creator lands on the build as a whole.
	 *                     Set only by the Sticker/Accessory SHORTCUTS, which are secondary doors.
	 *
	 * THE OLD DEFAULT WAS `= EAFLLoadoutAxis::BodyColor`, so there had never been a no-axis state and
	 * every open silently focused a channel nobody chose. That default was the inverted hierarchy in
	 * miniature: the creator is where a robot is built, and an axis is one thing inside it.
	 *
	 * AN INDEX, NOT A POINTER: builds are addressed by index everywhere else (EditingIndex,
	 * ServerSaveBuild, the lapse rule's index-order locking), and a pointer into a replicated array
	 * dangles the moment the set replicates.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout|Creator")
	// -1 rather than INDEX_NONE: UHT cannot parse a macro as a UFUNCTION default parameter.
	class UAFLW_Creator* OpenCreator(int32 BuildIndex = -1);

	/** The SHORTCUT door: open focused on one axis. Kept, and secondary, per the ruling. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout|Creator")
	class UAFLW_Creator* OpenCreatorOnAxis(EAFLLoadoutAxis FocusAxis);

	// ===== CC-5 step 3: ACTIVE-AXIS MODEL (region B/C tabs -> D rail+detail -> E commit) ============
	//
	// The screen shows ONE axis at a time. That is the whole point: with a rail per axis the screen's
	// height grows with the axis count, and it has already outgrown a 720-tall window. Here height is
	// constant no matter how many axes exist.

	/** The axis the rail and detail panel are showing. Never an arrangement axis -- selecting one of
	 *  those opens the creator instead, so it can never become the thing the rail is displaying. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Loadout|Active")
	EAFLLoadoutAxis ActiveAxis = EAFLLoadoutAxis::Weapon;

	/** The row highlighted in the rail. DISTINCT FROM EQUIPPED, deliberately: browsing must not equip,
	 *  or every arrow-key press on a controller fires a server RPC. Commit is a separate act. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Loadout|Active")
	FName SelectedId;

	/** Switch the shown axis. An ARRANGEMENT axis opens the creator focused there and leaves ActiveAxis
	 *  alone -- a nine-zone sticker arrangement is not something a one-id rail can display. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout|Active")
	void SetActiveAxis(EAFLLoadoutAxis Axis);

	/** Highlight a row. Refreshes the detail panel and the commit button; equips nothing. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout|Active")
	void SelectItem(FName CosmeticId);

	/** Region E. Equips SelectedId on ActiveAxis. No-ops with a logged reason when there is nothing to
	 *  commit, rather than dispatching an RPC the server would reject. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout|Active")
	void CommitEquip();

	/** The widget class used for BOTH axis tabs and rail rows. One class, two bindings -- the region
	 *  that spawns it decides which handler it routes to. Falls back to TileClass when unset. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Active")
	TSubclassOf<UAFLW_LoadoutTileBase> AxisTabClass;

protected:
	//~UCommonActivatableWidget / UUserWidget
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~End

	/** WEAPON-axis tile grid.
	 *
	 *  CC-5 step 3: WAS a REQUIRED BindWidget. The active-axis design has no per-axis rails at all -- one
	 *  RailContainer shows whichever axis is active -- so a mandatory weapon-only grid would force every
	 *  WBP to carry a panel the layout does not have, or fail to compile. Optional like the other seven.
	 *  Kept rather than deleted: removing public API is a separate decision from changing a layout, and
	 *  a per-axis WBP may still want it. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
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

	/** EMBLEM-axis tile grid. A selection axis like the six above it -- one id, N owned rows. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> EmblemTileContainer;

	/** CC-5 STICKER rail. Holds exactly ONE tile -- the "open the creator on this axis" affordance -- because
	 *  a sticker loadout is an ARRANGEMENT (nine zones), which no single tile can express. Same container
	 *  shape and same TileClass as every axis above, so the WBP places it where the sticker rail belongs. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StickerTileContainer;

	/** CC-5 ACCESSORY rail. Same as StickerTileContainer -- an arrangement over fixed hardpoints. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> AccessoryTileContainer;

	// ===== CC-5 step 3: the design-system regions ==================================================
	// ALL OPTIONAL. A WBP binds EITHER these or the per-axis containers above; the in-match locker keeps
	// the old ones and is untouched by this pass.

	// ===== CC-5 item 6: OWNED-ONLY SURFACE ========================================================

	/** Does this axis appear at all?
	 *
	 *  Asks what the PLAYER CAN SEE, not what the enum declares: a selection axis needs owned rows, an
	 *  arrangement axis needs catalog rows (it is a door to the creator, not a list). WeaponSkin has
	 *  zero of either since the retirement and Accessory has never had any, so both vanish -- and both
	 *  return by themselves the day they have content, with no code change.
	 *
	 *  THE ENUM MEMBERS STAY. Removing WeaponSkin would renumber every axis after it, and stored
	 *  selections are raw ints. */
	UFUNCTION(BlueprintPure, Category = "AFL|Loadout|Owned")
	bool ShouldAxisAppear(EAFLLoadoutAxis Axis) const;

	/** Saved builds, rebuilt from the replicated set. A build is not a cosmetic and is not an axis. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> BuildsContainer;

	/** First-class New Build action. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> NewBuildButton;

	/** THE NEW-PLAYER STATE, which is the common first experience rather than an edge case: six
	 *  identities, no builds, near-empty axes. Says what is true and where to go. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> EmptyStateText;

	UFUNCTION(BlueprintCallable, Category = "AFL|Loadout|Build")
	void RebuildBuilds();

	/** REGION B/C -- the axis tab row. One tab per axis, one active. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> AxisTabContainer;

	/** REGION D left -- rows for the active axis only. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RailContainer;

	/** REGION D right -- the detail panel's fields. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> DetailNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> DetailMetaText;

	/** REGION E -- the single commit. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UButton> EquipButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> EquipLabelText;

	/** Optional close button -> DeactivateWidget (pops the locker off the Menu layer). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** Center-stage 3D preview -- a UImage showing a live SceneCapture of the REAL local pawn (Approach A:
	 *  the preview IS the pawn the authoritative/replicated apply path already updates, so it cannot lie). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> PreviewImage;

	/** Front-3/4 framing: (forward, right, up) offset of the capture camera from the pawn, pawn-local. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	FVector PreviewCamOffset = FVector(180.f, 40.f, 47.f);

	/** The capture camera looks at this pawn-local point (roughly the chest). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	FVector PreviewFocusOffset = FVector(0.f, 0.f, 21.f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	float PreviewFOV = 82.f;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	FIntPoint PreviewResolution = FIntPoint(512, 768);

	/** The reusable kiosk-pod diorama actor staged around the previewed hero (Increment C). Null -> the C++
	 *  AAFLLoadoutPod placeholder; override with a branded BP child in the WBP for the SM_AFL_LoadoutPod swap. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	TSubclassOf<AAFLLoadoutPod> PodClass;

	/** The ASC-less DISPLAY pawn spawned for the preview (shows cosmetics without the combat stack). Null ->
	 *  the C++ AAFLLoadoutDisplayPawn; override with a BP child to configure the driving mesh / idle AnimBP. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	TSubclassOf<AAFLLoadoutDisplayPawn> DisplayPawnClass;

	/** The robot body the display pawn wears when no identity resolves -- the IRONICS free-grant default
	 *  (B_AFL_Robot_IRONICS). The IRONICS fallback is thus free-by-construction for a fresh/unset display pawn. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	TSoftClassPtr<AActor> DisplayFallbackRobotClass;

	/** Identity id (AFL.Team.* / AFL.Character.*) -> robot body class, for the display pawn's IDENTITY axis.
	 *  Set to DA_AFL_CharacterPartMap (the same map the selector uses). Null -> only the IRONICS fallback. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Loadout|Preview")
	TObjectPtr<UAFLCharacterPartMap> DisplayPartMap;

	/** A tile was clicked: equip its cosmetic on Axis, then refresh the grids (EQUIPPED badge). */
	UFUNCTION()
	void HandleTileClicked(EAFLLoadoutAxis Axis, FName CosmeticId);

	/** CC-5 step 3. Two handlers, not one: a tab and a row are the same widget class, and the ONLY
	 *  honest way to tell them apart is which delegate the spawning region bound. */
	UFUNCTION()
	void HandleAxisTabClicked(EAFLLoadoutAxis Axis, FName CosmeticId);

	UFUNCTION()
	void HandleRailItemClicked(EAFLLoadoutAxis Axis, FName CosmeticId);

	UFUNCTION()
	void HandleEquipButtonClicked();

	UFUNCTION()
	void HandleNewBuildClicked();

	UFUNCTION()
	void HandleBuildTileClicked(int32 InBuildIndex);

	void RebuildAxisTabs();
	void RebuildRail();
	void RefreshDetail();

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

	/** The local player's current pawn (the REAL gameplay pawn; used for the display-pawn spawn location). */
	APawn* GetLocalPawn() const;

	/** The pawn the preview CAPTURES: the ASC-less display pawn (spawned on first call + given the IRONICS body).
	 *  Replaces GetLocalPawn() as the capture target so the pod shows a display robot, not the gameplay pawn --
	 *  which is what lets the preview work with no live pawn (the front-end fix under B). */
public:
	/** The display pawn, spawning one if needed.
	 *
	 *  PUBLIC as of 2026-08-27: the creator reads the chassis's WORN channel colours off this pawn's
	 *  materials to seed the rail, which is a product path, not a test one. It was private with a
	 *  public GetPreviewPawnForTest() twin -- and reaching for a "ForTest" accessor from shipping UI
	 *  would misname the dependency for every later reader. */
	APawn* GetPreviewPawn();

	/** The identity->body map, READ ONLY.
	 *
	 *  A const getter rather than making DisplayPartMap public: the creator needs to ASK whether a
	 *  chassis line can resolve to a body, not to hold or replace the map. Widening the property
	 *  itself would hand every caller a settable pointer to answer a question. */
	const class UAFLCharacterPartMap* GetDisplayPartMap() const { return DisplayPartMap; }

private:

	/** Apply the player's CURRENT selection to the display pawn via the PROVEN 3-tier fan-out: resolve the
	 *  identity -> robot body (IRONICS fallback; re-spawned only on change) + drive the controller's
	 *  Refresh*ForPawn(DisplayPawn) for skin/body/edge/facemask/weapon/beam. The display pawn is PS-less
	 *  (resolve falls back to the controller PS) + HasAuthority (setters apply) -- both verified. */
	void ApplySelectionToDisplayPawn();

	/** Spawn/attach the SceneCapture rig framing the pawn + route its render target into PreviewImage. */
	void SetupPreviewCapture();

	/** Isolate the robot: set the capture's ShowOnlyList to the pawn + its attached actors (the equipped
	 *  weapon changes as you pick) so it renders on the clean backdrop, not the arena. Cheap; called per-tick. */
	void RefreshPreviewShowList();

	/** Re-position the capture from the afl.Loadout.Preview* cvars each tick -> LIVE framing tuning (no rebuild). */
	void RepositionPreviewCamera();

	/** Re-position the pod under the pawn from afl.Loadout.PodGroundZ each tick -> LIVE grounding (raise the
	 *  hero relative to the capsule so the feet clear the base + glue the floor disc under the feet). */
	void RepositionPreviewPod();

	/** Destroy the SceneCapture rig (on deactivate). */

public:
	// ─── CC-5.3 · CREATOR PREVIEW INTERFACE ──────────────────────────────────────────────────────
	// THE INTERFACE THE WIDGET CALLS. The widget itself is the UI lane's; this is the behaviour it
	// binds to, so nothing has to be authored blind. Three verbs: set a channel, apply, rotate --
	// plus reads for state.
	//
	// NO SEPARATE PREVIEW COLOUR PATH, AND THAT IS THE POINT. CreatorApplyPreview pushes a preview
	// SELECTION through UAFLSkinColorControllerComponent::SetPreviewSelection, and every
	// Refresh*ForPawn reads it via GetEffectiveSelection. RefreshSkinForPawn then builds the overlay
	// with UAFLCosmeticLoadoutComponent::BuildColorOverride and calls SetColorOverride -- the SAME
	// functions the gameplay pawn goes through on possession. A preview that resolved colour its own
	// way would be a bait-and-switch waiting to happen (CREATOR_SSOT 5.3: "the preview is the
	// product"), so the creator drives the shipping path rather than a parallel one.

	/** Set one creator channel on the working selection. Clamped to the neon gamut immediately, so what
	 *  the preview shows is what the server would commit -- the clamp is shared, not re-implemented. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Creator|Preview")
	void CreatorSetChannel(EAFLCreatorChannel Channel, FLinearColor Colour);

	/** Push the working selection to the display pawn through the shipping resolve path. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Creator|Preview")
	void CreatorApplyPreview();

	/** Spin the model. Rotates the MESH, not the actor: the scene capture is ATTACHED to the actor, so
	 *  rotating the actor would carry the camera around with it and the view would never change -- a
	 *  rotate control that looks wired and does nothing. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Creator|Preview")
	void CreatorRotatePreview(float DeltaYawDegrees);

	/** Current preview yaw in degrees. Read state for the widget, and the far-side check for a proof. */
	UFUNCTION(BlueprintPure, Category = "AFL|Creator|Preview")
	float CreatorGetPreviewYaw() const;

	/** The working selection the preview is showing. Not committed -- CC-3 save is a separate act. */
	UFUNCTION(BlueprintPure, Category = "AFL|Creator|Preview")
	FAFLCosmeticSelection CreatorGetWorkingSelection() const { return CreatorWorking; }

	/** Which channels are real on the bound chassis, and why each is not. Straight from the measured
	 *  schema so the widget disables rather than hides (CC-X24). */
	UFUNCTION(BlueprintPure, Category = "AFL|Creator|Preview")
	FAFLCreatorChannelSchema CreatorGetSchema() const;

	/** Channel link state. Defaults fully unlinked; the pairing is unruled (CC-X24). */
	UPROPERTY(BlueprintReadWrite, Category = "AFL|Creator|Preview")
	FAFLCreatorChannelLinks CreatorLinks;

#if !UE_BUILD_SHIPPING
	/** TEST ONLY (CC-6.5). The display pawn, so a probe can read its RESOLVED colour override and
	 *  compare it against the gameplay pawn's. That comparison IS the done definition -- "the robot in
	 *  match is the robot in the preview" -- and it can only be made by reading BOTH pawns through the
	 *  same accessor, or the difference measured would be a difference in method. Null before the rig
	 *  has spawned one.
	 *
	 *  BODY LIVES IN THE .CPP. DisplayPawn is a forward-declared type here, and an inline Cast<> on an
	 *  incomplete type compiles ONLY in translation units that happen to include AFLLoadoutDisplayPawn.h
	 *  first -- so this header built or failed depending on who included it, which is how it survived
	 *  until another UI file included it in a different order. */
	APawn* GetPreviewPawnForTest() const;

	/** TEST ONLY (CC-5 step 2). The two rails the entry proof clicks. Returned as the BOUND pointer, so a
	 *  null here is readable as "BindWidgetOptional did not bind" -- which is otherwise invisible: a null
	 *  container is skipped inside RebuildAxisTiles without a word, and the missing rail would be blamed
	 *  on the code that fills it. */
	UPanelWidget* GetStickerRailForTest() const { return StickerTileContainer; }
	UPanelWidget* GetFacemaskRailForTest() const { return FacemaskTileContainer; }
#endif

protected:
	/** The uncommitted selection the creator is editing. Seeded from the committed one on first touch. */
	UPROPERTY()
	FAFLCosmeticSelection CreatorWorking;

	/** Whether CreatorWorking has been seeded yet -- absent-vs-default made readable rather than
	 *  inferred from whether the struct "looks empty". */
	UPROPERTY()
	bool bCreatorWorkingSeeded = false;

private:
	void TeardownPreviewCapture();

	/** The scene-capture actor framing the pawn (attached to it; captures every frame -> live). */
	TWeakObjectPtr<ASceneCapture2D> PreviewCapture;

	/** The kiosk-pod diorama actor spawned attached to the pawn -> rendered INSIDE the preview via the
	 *  ShowOnlyList (Increment C). Destroyed with the capture on deactivate. */
	TWeakObjectPtr<AAFLLoadoutPod> PreviewPod;

	/** The ASC-less display pawn captured in the pod (spawned on GetPreviewPawn, destroyed on teardown). */
	TWeakObjectPtr<AAFLLoadoutDisplayPawn> DisplayPawn;

	/** Last selection applied to the display pawn -- the NativeTick change-poll re-drives only on a delta
	 *  (an equip lands via OnRep async, so a poll is more robust than a post-equip call). */
	FAFLCosmeticSelection LastAppliedDisplaySelection;

	/** Last identity applied to the display body -- SetRobotBody (remove+add) fires ONLY on identity change,
	 *  never on a color/weapon pick, so the robot doesn't thrash. */
	FName LastAppliedBodyIdentity = NAME_None;
	bool bDisplayBodyApplied = false;

	/** The previewed pawn's capsule half-height (feet offset), cached for RepositionPreviewPod grounding. */
	float PreviewFeetDrop = 90.f;

	/** The render target the capture writes + PreviewImage displays (runtime, transient). */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PreviewRT;
};
