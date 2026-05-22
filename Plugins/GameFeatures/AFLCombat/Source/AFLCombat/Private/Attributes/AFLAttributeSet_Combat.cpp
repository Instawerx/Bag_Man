// Copyright C12 AI Gaming. All Rights Reserved.

#include "Attributes/AFLAttributeSet_Combat.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "Effects/GE_AFL_Heat_VentingComplete.h"
#include "GameplayEffectExtension.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAttributeSet_Combat)

// Heat-system native tag consumed inside PostGameplayEffectExecute. Same
// CDO-vs-ini-scan rationale as the rest of AFLCombat — UE_DEFINE_GAMEPLAY_TAG_STATIC
// at file scope registers the tag at module init, strictly before any CDO of
// a class in this module is constructed (per the 2026-05-20 crash post-mortem).
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Overheated_AttrSet, "State.Overheated");


UAFLAttributeSet_Combat::UAFLAttributeSet_Combat()
	: Health(100.0f)
	, MaxHealth(100.0f)
	, Shield(0.0f)
	, MaxShield(0.0f)
	, Armor(0.0f)
	, OverkillThreshold(50.0f)
	, Heat(0.0f)
	, MaxHeat(100.0f)
	, HeatDecayRate(20.0f)
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
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, Heat,              COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, MaxHeat,           COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Combat, HeatDecayRate,     COND_OwnerOnly, REPNOTIFY_Always);
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
	else if (Attribute == GetHeatAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHeat());
	}
	else if (Attribute == GetMaxHealthAttribute()
		|| Attribute == GetMaxShieldAttribute()
		|| Attribute == GetArmorAttribute()
		|| Attribute == GetOverkillThresholdAttribute()
		|| Attribute == GetMaxHeatAttribute()
		|| Attribute == GetHeatDecayRateAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	// Damage / HeatPerBeamTick: no clamping; consumed in PostGameplayEffectExecute.
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
	else if (Data.EvaluatedData.Attribute == GetHeatPerBeamTickAttribute())
	{
		// Same transit-meta pattern as Damage. GE_AFL_Heat_BeamTick writes
		// HeatPerBeamTick (Override +4); here we fold it into the persistent
		// Heat attribute, clamp, grant State.Overheated when Heat hits MaxHeat,
		// then zero the meta so it can't accumulate across ticks.
		const float Delta = GetHeatPerBeamTick();
		SetHeatPerBeamTick(0.0f);

		if (Delta != 0.0f)
		{
			const float NewHeat = FMath::Clamp(GetHeat() + Delta, 0.0f, GetMaxHeat());
			SetHeat(NewHeat);

			if (NewHeat >= GetMaxHeat() && GetMaxHeat() > 0.0f)
			{
				UAbilitySystemComponent* TargetASC = &Data.Target;
				if (!TargetASC->HasMatchingGameplayTag(TAG_State_Overheated_AttrSet))
				{
					// Loose-tag grant: the Overheated tag has to persist past
					// this GE instance (BeamTick is Instant) but is owned by
					// the heat state machine in this AttributeSet, not by an
					// active GE. Replicate so client-side ActivationBlockedTags
					// checks on the beam ability also see it.
					TargetASC->AddLooseGameplayTag(TAG_State_Overheated_AttrSet);
					TargetASC->SetReplicatedLooseGameplayTagCount(TAG_State_Overheated_AttrSet, 1);

					// Acceptance log line — orchestrator log-stream scrape looks
					// for `AFL_LOG: heat_overheat` at the overheat boundary.
					UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOG: heat_overheat"));
				}
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHeatAttribute())
	{
		// Decay GE writes negative deltas into Heat. When the cooled value
		// drops below MaxHeat * 0.3 and the target still carries
		// State.Overheated, clear the tag and apply the venting-complete
		// marker GE — the GE grants Event.Combat.HeatVentingComplete on
		// application so listeners (HUD pulse, audio cue, telemetry) get the
		// boundary without coupling to Heat directly.
		UAbilitySystemComponent* TargetASC = &Data.Target;
		if (TargetASC->HasMatchingGameplayTag(TAG_State_Overheated_AttrSet)
			&& GetHeat() <= GetMaxHeat() * 0.3f)
		{
			TargetASC->SetReplicatedLooseGameplayTagCount(TAG_State_Overheated_AttrSet, 0);
			TargetASC->RemoveLooseGameplayTag(TAG_State_Overheated_AttrSet);

			FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
			Context.AddInstigator(TargetASC->GetOwnerActor(), TargetASC->GetAvatarActor());
			FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
				UGE_AFL_Heat_VentingComplete::StaticClass(), /*Level=*/1.0f, Context);
			if (SpecHandle.IsValid())
			{
				TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}

			// Acceptance log line — orchestrator log-stream scrape looks for
			// `AFL_LOG: heat_vented` at the vent-complete boundary.
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOG: heat_vented"));
		}
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

void UAFLAttributeSet_Combat::OnRep_Heat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, Heat, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_MaxHeat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, MaxHeat, OldValue);
}

void UAFLAttributeSet_Combat::OnRep_HeatDecayRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Combat, HeatDecayRate, OldValue);
}
