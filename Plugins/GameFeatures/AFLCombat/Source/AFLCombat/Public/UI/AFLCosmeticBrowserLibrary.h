// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/AFLW_LoadoutTileBase.h" // EAFLLoadoutAxis

#include "AFLCosmeticBrowserLibrary.generated.h"

struct FAFLCatalogEntry;
class UAFLCosmeticLoadoutComponent;
class APlayerState;
class AController;
class APawn;

/**
 * UAFLCosmeticBrowserLibrary -- the shared cosmetic-browsing SERVICE (Digital-Market unification, #7/B Phase 1).
 *
 * A pure static service (NOT a base class -> ZERO inheritance coupling). The NEW front-end market
 * (UAFLW_FrontEndMarket) calls these for its Store + Loadout modes; the IN-MATCH UAFLW_LoadoutBase stays
 * UNTOUCHED and keeps its own private copies of this logic. That is the operator's ruled seam: no shared
 * base, no reparent of the in-match loadout -- the front-end and in-match share only this service + the
 * catalog/wallet/loadout DATA spine, never a widget-class ancestor or a conditional branch.
 *
 * The axis-feed / owned-filter / equip bodies here MIRROR UAFLW_LoadoutBase's proven private logic. That is
 * a deliberate, tracked duplication: keeping the in-match loadout literally untouched is worth one copy of a
 * ~40-line feed. A later refactor MAY point UAFLW_LoadoutBase at this library, but only once "in-match
 * untouched" is explicitly lifted.
 */
UCLASS()
class AFLCOMBAT_API UAFLCosmeticBrowserLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** STORE feed: every PURCHASABLE catalog entry (Acquisition != GrantedFree). Wraps the catalog subsystem. */
	static void GetPurchasableEntries(const UObject* WorldContext, TArray<FAFLCatalogEntry>& OutEntries);

	/** LOADOUT feed: the player's OWNED entries for one axis (GrantedFree OR wallet-entitled), namespace-filtered.
	 *  Mirrors UAFLW_LoadoutBase::GetOwnedEntriesForAxis exactly (Identity queries Team+Character). */
	static void GetOwnedEntriesForAxis(const UObject* WorldContext, const APlayerState* PS, EAFLLoadoutAxis Axis, TArray<FAFLCatalogEntry>& OutOwned);

	/** The currently-equipped id for an axis (reads the replicated FAFLCosmeticSelection). NAME_None if none/absent. */
	static FName GetEquippedIdForAxis(const UAFLCosmeticLoadoutComponent* Loadout, EAFLLoadoutAxis Axis);

	/** EQUIP: copy the selection, seed the free IRONICS identity if none (else the RPC's _Validate drops it),
	 *  set the ONE axis field, dispatch ServerSetCosmeticSelection (BlueprintAuthorityOnly -> must be C++). */
	static void EquipForAxis(UAFLCosmeticLoadoutComponent* Loadout, EAFLLoadoutAxis Axis, FName CosmeticId);

	/** Fan-out: push the committed selection onto a target pawn via the Controller's UAFLSkinColorControllerComponent
	 *  in the proven composition order (Facemask -> Skin -> Weapon -> WeaponSkin -> Beam). For the FRONT-END this
	 *  targets the ARMORY display pawn (NOT the gameplay pawn). Identity/body (SetRobotBody) is the widget's concern. */
	static void ApplySelectionToPawn(AController* Controller, APawn* Pawn);
};
