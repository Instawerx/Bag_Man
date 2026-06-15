// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/CheatManager.h"

#include "AFLCombatCheats.generated.h"

class UAbilitySystemComponent;


/**
 * UAFLCombatCheats
 *
 * Editor + non-shipping console cheats for the Sprint 1 AFL-0106 damage-pipeline
 * acceptance matrix. Self-registers with the cheat manager on game start when
 * UE_WITH_CHEAT_MANAGER is defined. Mirrors ULyraCosmeticCheats registration.
 *
 * NOT for production gameplay. Exposes raw Override writes to combat attributes
 * and a direct damage-flow trigger that bypasses the granted-ability path so
 * the matrix can vary BaseDamage / multipliers per call without touching the
 * BP_GA_AFL_Damage_Test CDO.
 */
UCLASS(NotBlueprintable)
class AFLCOMBAT_API UAFLCombatCheats final : public UCheatManagerExtension
{
	GENERATED_BODY()

public:

	UAFLCombatCheats();

	/** Run the §8.3 damage pipeline self-target with explicit per-call inputs. Server-only. */
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void TestDamage(float Base = 25.0f, float Headshot = 1.0f, float Weakpoint = 1.0f, float Distance = 1.0f);

	/** Override an AFLAttributeSet_Combat attribute by short name. Server-only.
	 *  Valid names (case-insensitive): Health, MaxHealth, Shield, MaxShield, Armor, OverkillThreshold, Damage. */
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void SetCombatAttribute(const FString& Name, float Value);

	/** Log a snapshot of the player's combat attributes to LogAFLCombat. */
	UFUNCTION(Exec)
	void DumpCombatAttributes();

	/**
	 * #43 selection-seam harness. PURE CLIENT-ISSUED CALLER of the real RPC: builds an
	 * FAFLCosmeticSelection from the typed edge id and hands it to
	 * UAFLCosmeticLoadoutComponent::ServerSetCosmeticSelection. It does NOTHING else -- it does not
	 * write the replicated selection, does not touch the controller resolve, does not gate. The whole
	 * point is to exercise the genuine seam end to end: this client exec -> the component's Server RPC
	 * -> server validation + entitlement + change-timing gate -> replicated commit -> OnRep -> the
	 * controller's spawn-read/refresh -> the proven SetSkinColor push.
	 *
	 * Deliberately NOT BlueprintAuthorityOnly: it must run on the OWNING CLIENT so the Server RPC makes
	 * the real client->server hop (an authority-only exec would run server-side and skip that hop,
	 * proving nothing about the wire).
	 *
	 * EdgeColorId examples (must be present in DA_AFL_BrandEdgeMap to resolve via the #43 stopgap):
	 *   NeonPurple / NeonPink / NeonBlue / NeonGreen  (NOT NeonRed -- absent from the map -> would miss).
	 * Accepts either the short color ("NeonPink") or the full CosmeticId ("AFL.Edge.NeonPink").
	 */
	UFUNCTION(Exec)
	void SetCosmeticEdge(const FString& EdgeColorId);

	/**
	 * Set the player's CHARACTER identity (IdentityType=Character) so the body selector resolves an
	 * AFL.Character.* robot (e.g. Big Sixx). Cheat-manager-extension method -> targets the OWNING CLIENT's
	 * PlayerState via GetPlayerController() (the window you typed in), so two PIE windows can be driven
	 * INDEPENDENTLY -- unlike the world-global afl.Cosmetic.SetCharacter console command which resolves to
	 * GetFirstPlayerController() and collapses to one player. NOT BlueprintAuthorityOnly (must run on the
	 * owning client so the Server RPC makes the real client->server hop).
	 * Accepts "BigSixx" or the full "AFL.Character.BigSixx".
	 */
	UFUNCTION(Exec)
	void SetCosmeticCharacter(const FString& CharacterId);

	/**
	 * Set the player's TEAM identity (IdentityType=Team). The per-window sibling of SetCosmeticCharacter for
	 * the Team axis (the proven roster). Targets the owning client's PlayerState (GetPlayerController()).
	 * Accepts "IRONICS" or the full "AFL.Team.IRONICS".
	 */
	UFUNCTION(Exec)
	void SetCosmeticTeam(const FString& TeamId);

	/**
	 * Kill THIS window's pawn (set its Health to 0 via the owning client's ASC) to force a death->respawn->
	 * re-possession, which is what makes the body selector re-resolve the current identity. Per-window
	 * (GetPlayerController()'s ASC), so each PIE window respawns ITS OWN pawn -- the matching half of the
	 * per-window identity cheats (without it, a world-global kill only respawns one player). Test-only.
	 */
	UFUNCTION(Exec)
	void SuicidePawn();

private:

	UAbilitySystemComponent* GetPlayerASC() const;

	/** #43: resolve the owning client's PlayerState loadout component (the RPC target). Null if not ready. */
	class UAFLCosmeticLoadoutComponent* GetLoadoutComponent() const;
};
