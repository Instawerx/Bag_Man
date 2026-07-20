// Copyright C12 AI Gaming. AFL / BAG MAN.
#pragma once

#include "Weapons/LyraWeaponSpawner.h"
#include "GameplayTagContainer.h"

#include "AFLWeaponSpawner.generated.h"

class UAFLWeaponSpawnRegistry;
class ULyraWeaponPickupDefinition;

/**
 * AFL weapon-spawner base -- Path A of the GameFeature-injected pickup system (Option C hierarchy).
 *
 * HIERARCHY (deliberate stock touch -- same class as the deliberate stock re-brands): this class is
 * inserted BETWEEN stock B_WeaponSpawner and ALyraWeaponSpawner -- B_WeaponSpawner is reparented to
 * extend it. So EVERY spawner in the game (stock B_WeaponSpawner, B_AbilitySpawner, and AFL markers)
 * descends from it, which is what makes an AFL marker a first-class B_WeaponSpawner: visible to every
 * Cast<B_WeaponSpawner> and GetAllActorsOfClass(B_WeaponSpawner) in the codebase (GCN_InteractPickUp's
 * pickup-VFX cast, EQS_Context_WeaponCheck's bot weapon-seeking, B_ShooterGameScoring_Base's pad-reset).
 * WeaponIntent is OPT-IN: an empty tag => byte-identical stock behaviour (BeginPlay early-outs to Super,
 * no registry, no logging); a tag set => the AFL GameFeature resolves it to a real pickup at runtime.
 *
 * THE PROBLEM THIS SOLVES: a weapon spawner placed in a stock map (ShooterMaps) cannot legally hard-
 * reference an AFL weapon, because that weapon is GameFeature content -- the base map loads BEFORE the
 * GameFeature activates, so a baked ref resolves null and the pickup silently does nothing (the exact
 * null-ref no-op we chased). Instead the designer expresses intent with an ENGINE-LEVEL FGameplayTag (no
 * asset reference, so it is legal on any actor in any map), and the AFL GameFeature resolves that tag to a
 * real ULyraWeaponPickupDefinition at RUNTIME, once it is active.
 *
 * WHY IT LIVES HERE: AFLGameCore is always-loaded and AFL-content-AGNOSTIC, so this class is placeable in
 * stock maps exactly like the stock ALyraWeaponSpawner it extends. It names NO AFL content -- it holds a
 * tag and asks UAFLWeaponSpawnRegistry (also AFLGameCore) to resolve it. The AFL GameFeature
 * (AFLCombat's UAFLGFA_WeaponSpawns) feeds that registry the tag->def table on activation.
 *
 * MULTIPLAYER: resolution is deterministic-LOCAL on server and every client (the tag is baked into level
 * data, the table is present on both), which is why the stock non-replicated WeaponDefinition is fine --
 * each side resolves it identically (clients for the pad mesh, the server for the authoritative grant,
 * both inherited unchanged from ALyraWeaponSpawner).
 *
 * WORLD PARTITION: resolution is marker-PULL in BeginPlay (which fires when the cell streams in), so cells
 * that stream in AFTER the GameFeature activated resolve immediately, and markers that pre-date activation
 * resolve on the registry's OnReady broadcast. No world scan, so late-loading cells are never missed.
 *
 * NOTE (grant): ALyraWeaponSpawner::GiveWeapon is a BlueprintImplementableEvent, so the actual inventory
 * grant is authored on a Blueprint child (placeable from /Game, referencing this native parent). This
 * class supplies only the resolution + pad refresh; it does not reimplement the grant.
 */
UCLASS()
class AFLGAMECORE_API AAFLWeaponSpawner : public ALyraWeaponSpawner
{
	GENERATED_BODY()

public:
	AAFLWeaponSpawner();

protected:
	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor interface

	/** DIAGNOSTIC: log the pickup guard state (authority/available/def/itemDef) then run the stock grant path. */
	virtual void AttemptPickUpWeapon_Implementation(APawn* Pawn) override;

	/** The designer's per-spawner choice, resolved to a ULyraWeaponPickupDefinition by the AFL GameFeature. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Spawner", meta = (Categories = "AFL.Spawner"))
	FGameplayTag WeaponIntent;

private:
	/** Look up WeaponIntent in the world registry; on success apply the def, on failure WARN and stay inert. */
	void ResolveAndApply();

	/** The single place the resolved def is applied: set the inherited WeaponDefinition + refresh the pad. */
	void ApplyDefinition(ULyraWeaponPickupDefinition* Def);

	/** Clear the def + hide the weapon mesh -- the spawner goes dark (GameFeature deactivated). */
	void RevertToInert();

	/** Re-drive the pad's weapon mesh from the current WeaponDefinition (mirror of stock OnConstruction). */
	void RefreshPad();

	/** The world's registry, or null very early / in non-game worlds. */
	UAFLWeaponSpawnRegistry* GetRegistry() const;

	//~ Registry delegate handlers
	void HandleRegistryReady();
	void HandleRegistryCleared();

	/** True once we have bound the registry delegates, so EndPlay unbinds exactly once. */
	bool bBoundToRegistry = false;
};
