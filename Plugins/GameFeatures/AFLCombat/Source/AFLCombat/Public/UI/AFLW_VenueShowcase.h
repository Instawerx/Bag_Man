// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
// Not forward-declared: UAFLW_VenueTile DERIVES from UCommonButtonBase at the bottom of this file, and a
// base class has to be complete. Everything else here stays a forward declaration.
#include "CommonButtonBase.h"
#include "Online/AFLLobbyTypes.h"

#include "AFLW_VenueShowcase.generated.h"

class UAFLW_VenueTile;
class UCommonTextBlock;
class UImage;
class UPanelWidget;

/**
 * One venue. A VENUE IS A LEVEL, NOT A PLAYLIST -- and that distinction is the surface.
 *
 * ⚠ ARCANEON has four playlists on one map (5v5/8v8 x Haywire/ProMod, `MAP_DISPLAY_NAME_REGISTRY.md`).
 * Listing playlists here would put those four on screen as four things to look at, and a list of
 * configurations is a config picker no matter what it is called -- which is the venue-picker failure
 * `ui-frontend.md` §8 exists to prevent, arriving by a side door. One row per LEVEL.
 */
USTRUCT(BlueprintType)
struct AFLCOMBAT_API FAFLVenueEntry
{
	GENERATED_BODY()

	/** `NANOWATT`, `ARCANEON`, `INFINEON`. Authored, never derived from the level name -- the registry is
	 *  explicit that inferring this is what jumbled NANOWATT/L_Arena_01 against INFINEON/L_Expanse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Venues")
	FText DisplayName;

	/** One line of identity. NOT a mode or a size -- those belong to a queue, and this surface has none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Venues")
	FText Blurb;

	/** R97. ARENA = purpose-built, symmetric, one contained space. MAP = district-scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Venues")
	EAFLVenueClass VenueClass = EAFLVenueClass::Arena;

	/** The level, for provenance and de-duplication. NOT a travel destination from this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Venues")
	FName MapId;

	/** Key art. Soft: the showcase is the most image-heavy surface in the front end and the least visited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Venues")
	TSoftObjectPtr<UTexture2D> KeyArt;

	bool IsValid() const { return !DisplayName.IsEmpty() && !MapId.IsNone(); }
};

/**
 * UAFLW_VenueShowcase -- S8. Browse the maps. NO QUEUE.
 *
 * ══ THE ABSENCE IS THE FEATURE ════════════════════════════════════════════════════════════════════════
 *
 * `ui-frontend.md` §8 is one of the few sections written mostly in the negative, and it is worth reading
 * as a specification of what must NOT be here:
 *
 *   "A venue browser attached to a queue becomes a venue picker, no matter how it is labelled. If a
 *    player can see maps AND queue on the same surface, the natural reading is that the maps are choices
 *    -- and either the matchmaker honours that (fragmenting the pool) or the surface disappoints."
 *
 * So this screen has no CTA that enters a queue, no venue that persists into matchmaking, and no filter
 * it can hand to the lobby. Detached, the same content is pure upside: the maps are among the most
 * expensive things the project makes and a player who has seen them recognises them in play. **The
 * problem was never showing maps; it was showing maps next to a queue button.**
 *
 * ══ THE ONE PERMITTED EXIT, AND ITS ONE CONDITION ═════════════════════════════════════════════════════
 *
 * §8 allows a deep link *to* the lobby -- "but only to the lobby as it is, never pre-filtered by venue,
 * because a pre-filter is a venue choice wearing a different name." `BuildLobbyDeepLink` is that rule as
 * a static predicate a test can hold, in the same spirit as `IsStakeLegalForDoor`: the selected venue is
 * deliberately not an input to it, and it cannot be made one without changing a signature.
 *
 * ⚠ IT ROUTES TO THE HOME SCREEN, NOT TO A LOBBY. Which door a player wants is the R98 first decision and
 * it is not this surface's to make -- sending them straight into LEAGUE would be a product choice smuggled
 * in as a convenience, and sending them into STAKED would be worse.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_VenueShowcase : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** The venues, authored. Deduplicated by level, per the note on FAFLVenueEntry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Venues")
	TArray<FAFLVenueEntry> Venues;

	/** The tile class the list is built from. Empty = the WBP authored its own tiles. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Venues")
	TSubclassOf<UAFLW_VenueTile> TileClass;

	/** Show a venue in the detail region. Index-addressed so the console probe can drive it. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Venues")
	void SelectVenue(int32 Index);

	UFUNCTION(BlueprintPure, Category = "AFL|Venues")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	/**
	 * ⚠ THE §8 INVARIANT, AS A PREDICATE A TEST CAN HOLD.
	 *
	 * True when a route out of the showcase carries NO venue. The showcase may hand the player to the
	 * front end; it may not hand the front end a venue, because a venue that survives this screen is a
	 * venue choice, and a venue choice fragments the matchmaking pool or disappoints the player who made
	 * it. There is no third outcome, which is why this is a rule and not a preference.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Venues")
	static bool IsExitLegal(FName CarriedVenue, int64 CarriedStake);

	/**
	 * The one permitted exit. Returns NAME_None BY CONSTRUCTION -- the selected venue is not an argument,
	 * so no future edit can leak it without changing this signature and failing the test that guards it.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Venues")
	static FName BuildLobbyDeepLink();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Blueprint hook for the §6-style reveal when a venue is selected. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Venues", meta = (DisplayName = "On Venue Selected"))
	void BP_OnVenueSelected(const FAFLVenueEntry& Venue);

	/** REQUIRED: a showcase with nowhere to put tiles is not a showcase. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Venues")
	TObjectPtr<UPanelWidget> VenueList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Venues") TObjectPtr<UCommonTextBlock> VenueName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Venues") TObjectPtr<UCommonTextBlock> VenueBlurb;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Venues") TObjectPtr<UCommonTextBlock> VenueClassLabel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Venues") TObjectPtr<UImage> VenueArt;

	/**
	 * ⚠ THERE IS NO QUEUE BUTTON HERE AND THERE MUST NOT BE ONE. `BackButton` leaves; `LobbyLink` is §8's
	 * permitted deep link and carries nothing. If a future edit wants a "PLAY THIS MAP" control, that is a
	 * change to §8, not to this file.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Venues") TObjectPtr<UCommonButtonBase> BackButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Venues") TObjectPtr<UCommonButtonBase> LobbyLink;

private:
	void RebuildTiles();
	void ApplyDetail();

	int32 SelectedIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAFLW_VenueTile>> Tiles;
};

/** One tile in the list. Presentation only -- it selects, it never queues. */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_VenueTile : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AFL|Venues")
	void SetVenue(const FAFLVenueEntry& InVenue);

	/** ARENA / MAP as a label. Static so the detail panel and the tile cannot word it differently. */
	UFUNCTION(BlueprintPure, Category = "AFL|Venues")
	static FText FormatVenueClass(EAFLVenueClass InClass);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Venues") TObjectPtr<UCommonTextBlock> TileName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Venues") TObjectPtr<UCommonTextBlock> TileClassLabel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Venues") TObjectPtr<UImage> TileArt;
};
