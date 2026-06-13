// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraAttributeSet.h"

#include "AFLAttributeSet_Combat.generated.h"

class UObject;
struct FGameplayEffectModCallbackData;

/**
 * Fired from PostGameplayEffectExecute. Mirrors ULyraHealthSet's FLyraAttributeEvent
 * signature so consumers (UAFLDeathComponent, hit-react, HUD) bind it the same way
 * Lyra's ULyraHealthComponent binds ULyraHealthSet. Args: the AttributeSet, the GE
 * mod callback data (instigator/causer/spec), the damage magnitude, and old/new values.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FAFLAttributeEvent_Combat,
	AActor* /*EffectInstigator*/, AActor* /*EffectCauser*/, float /*Magnitude*/);


UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLAttributeSet_Combat : public ULyraAttributeSet
{
	GENERATED_BODY()

public:

	UAFLAttributeSet_Combat();

	/**
	 * AFL-native death/damage signals -- the trigger half of the AFL death system. AFL runs
	 * its OWN combat-health economy (this set + UAFLDamageExecCalc), parallel to ULyraHealthSet.
	 * Death therefore must fire off THIS set, not ULyraHealthSet, to be correct for every
	 * combatant (the player drains this Health too). UAFLDeathComponent binds OnOutOfHealth and
	 * drives Lyra's replicated death-state sequence (StartDeath/FinishDeath) from it. These are
	 * the AFL mirror of ULyraHealthSet::OnOutOfHealth / OnHealthChanged.
	 */
	mutable FAFLAttributeEvent_Combat OnOutOfHealth;   // Health crossed >0 -> <=0
	mutable FAFLAttributeEvent_Combat OnHealthChanged;  // any Health change (hit-react / HUD)

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, Health);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, MaxHealth);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, Shield);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, MaxShield);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, Armor);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, OverkillThreshold);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, Damage);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, Heat);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, MaxHeat);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, HeatDecayRate);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, HeatPerBeamTick);

	// S4-INC2: recoil/spread scalar read by UAFLAG_Laser_Pulse at fire time. Baseline
	// 1.0; a dismember consequence GE (arm loss) Multiply-modifies it up (x1.5/arm).
	// Clamped >= 1.0 in PreAttributeChange so a consequence only ever INCREASES recoil.
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, RecoilMultiplier);

	// S4-INC3: per-zone HP -- the OUTERMOST absorber. UAFLDamageExecCalc routes a hit by its
	// bone (AFLCore::BoneToZone) to the matching zone-HP, which drains FIRST; only the overflow
	// past zero continues to shield/health. Zone-HP <= 0 = severed (limb inert dead-zone / head
	// decapitation). Default 0.0 in the ctor; the InitData GE seeds real values in PHASE B.
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, HeadHealth);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, LeftArmHealth);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, RightArmHealth);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, LeftLegHealth);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, RightLegHealth);

protected:

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxShield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_OverkillThreshold(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Heat(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHeat(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_HeatDecayRate(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RecoilMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_HeadHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_LeftArmHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RightArmHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_LeftLegHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RightLegHealth(const FGameplayAttributeData& OldValue);

private:

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="AFL|Combat", Meta=(HideFromModifiers, AllowPrivateAccess=true))
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Shield, Category="AFL|Combat", Meta=(HideFromModifiers, AllowPrivateAccess=true))
	FGameplayAttributeData Shield;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxShield, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxShield;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Armor, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData Armor;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_OverkillThreshold, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData OverkillThreshold;

	// Meta — non-replicated, transit-only. ExecCalc consumes and zeroes per call.
	UPROPERTY(BlueprintReadOnly, Category="AFL|Combat", Meta=(HideFromModifiers, AllowPrivateAccess=true))
	FGameplayAttributeData Damage;

	// Heat economy (AFL-0207). Persistent + replicated like Health/Shield;
	// HeatPerBeamTick is the per-tick transit meta consumed inside
	// PostGameplayEffectExecute (same shape as Damage).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Heat, Category="AFL|Combat", Meta=(HideFromModifiers, AllowPrivateAccess=true))
	FGameplayAttributeData Heat;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHeat, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxHeat;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_HeatDecayRate, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData HeatDecayRate;

	UPROPERTY(BlueprintReadOnly, Category="AFL|Combat", Meta=(HideFromModifiers, AllowPrivateAccess=true))
	FGameplayAttributeData HeatPerBeamTick;

	// S4-INC2: recoil/spread multiplier (baseline 1.0). Persistent + replicated like
	// Health/Heat so the owning client's Pulse fire path reads the live value (incl.
	// any consequence GE modifier). Modifiable by GEs (NO HideFromModifiers).
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_RecoilMultiplier, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData RecoilMultiplier;

	// S4-INC3: per-zone HP. Persistent + replicated; the ExecCalc absorbs into these per the
	// hit bone before shield/health. No HideFromModifiers (the InitData GE + the ExecCalc's
	// Additive drain modifier write them). Default 0.0 (ctor) -> InitData seeds real values.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_HeadHealth, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData HeadHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LeftArmHealth, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData LeftArmHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_RightArmHealth, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData RightArmHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_LeftLegHealth, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData LeftLegHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_RightLegHealth, Category="AFL|Combat", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData RightLegHealth;
};
