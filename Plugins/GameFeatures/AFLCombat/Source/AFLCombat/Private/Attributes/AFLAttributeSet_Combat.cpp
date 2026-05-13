// Copyright C12 AI Gaming. All Rights Reserved.

#include "Attributes/AFLAttributeSet_Combat.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAttributeSet_Combat)


UAFLAttributeSet_Combat::UAFLAttributeSet_Combat()
	: Health(100.0f)
	, MaxHealth(100.0f)
	, Shield(0.0f)
	, MaxShield(0.0f)
	, Armor(0.0f)
	, OverkillThreshold(50.0f)
{
}

void UAFLAttributeSet_Combat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, Health,            COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, MaxHealth,         COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, Shield,            COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, MaxShield,         COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, Armor,             COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, OverkillThreshold, COND_OwnerOnly, REPNOTIFY_Always);
}

void UAFLAttributeSet_Combat::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxShield());
	}
	else if (Attribute == GetMaxHealthAttribute()
		|| Attribute == GetMaxShieldAttribute()
		|| Attribute == GetArmorAttribute()
		|| Attribute == GetOverkillThresholdAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	// Damage: no clamping; the ExecCalc validates inputs.
}

void UAFLAttributeSet_Combat::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// The ExecCalc writes Shield/Health output modifiers directly. Damage is a
	// transit meta-attribute used only inside the ExecCalc; if any GE somehow
	// routes through Damage as the evaluated attribute, zero it here so it
	// can't accumulate across calls.
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		SetDamage(0.0f);
	}
}

void UAFLAttributeSet_Combat::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, Health, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, MaxHealth, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, Shield, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, MaxShield, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, Armor, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_OverkillThreshold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, OverkillThreshold, OldValue);
}
