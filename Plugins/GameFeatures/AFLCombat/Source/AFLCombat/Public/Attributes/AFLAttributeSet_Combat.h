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
};
