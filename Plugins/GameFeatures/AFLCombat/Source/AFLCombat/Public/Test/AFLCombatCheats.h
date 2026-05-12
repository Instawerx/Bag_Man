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

private:

	UAbilitySystemComponent* GetPlayerASC() const;
};
