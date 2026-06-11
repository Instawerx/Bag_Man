// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraAttributeSet.h"

#include "AFLAttributeSet_Energy.generated.h"

class UObject;
struct FGameplayEffectModCallbackData;

/**
 * UAFLAttributeSet_Energy  (AFL-0701 landed -- energy drops cycle 1)
 *
 * The carried-energy economy: CarriedEnergy (the loop currency), MaxEnergy (clamp ceiling),
 * OverdriveThreshold (crossing it upward broadcasts Event.Energy.ThresholdReached; the Overdrive
 * GA/buff that CONSUMES the signal is cycle 2). Mirrors UAFLAttributeSet_Combat's shape exactly
 * (ULyraAttributeSet base, REPNOTIFY replication, PreAttributeChange clamping, PostGameplayEffect
 * threshold broadcast -- the Overkill precedent). Granted beside the Combat set via
 * DA_AFL_Combat_AbilitySet GrantedAttributes (the LyraAbilitySet first-class path); writes go
 * through UGE_AFL_EnergyGain_Small (SetByCaller Data.Energy.Gain) -- never direct (the rail).
 */
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLAttributeSet_Energy : public ULyraAttributeSet
{
	GENERATED_BODY()

public:

	UAFLAttributeSet_Energy();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Energy, CarriedEnergy);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Energy, MaxEnergy);
	ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Energy, OverdriveThreshold);

protected:

	UFUNCTION()
	void OnRep_CarriedEnergy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxEnergy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_OverdriveThreshold(const FGameplayAttributeData& OldValue);

private:

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CarriedEnergy, Category="AFL|Energy", Meta=(HideFromModifiers, AllowPrivateAccess=true))
	FGameplayAttributeData CarriedEnergy;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxEnergy, Category="AFL|Energy", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxEnergy;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_OverdriveThreshold, Category="AFL|Energy", Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData OverdriveThreshold;
};
