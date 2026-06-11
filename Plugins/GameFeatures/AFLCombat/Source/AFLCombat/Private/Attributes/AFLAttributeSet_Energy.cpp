// Copyright C12 AI Gaming. All Rights Reserved.

#include "Attributes/AFLAttributeSet_Energy.h"

#include "AFLCombat.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffectExtension.h"
#include "Messages/LyraVerbMessage.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLAttributeSet_Energy)

// Native tag (per-file Unity-safe suffix). Broadcast when CarriedEnergy crosses
// OverdriveThreshold UPWARD; the Overdrive GA/buff consumes it in cycle 2 -- today the
// harness + HUD listeners are the consumers.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Energy_ThresholdReached_EnergySet, "Event.Energy.ThresholdReached");


UAFLAttributeSet_Energy::UAFLAttributeSet_Energy()
{
	InitCarriedEnergy(0.0f);
	InitMaxEnergy(100.0f);
	InitOverdriveThreshold(80.0f);
}

void UAFLAttributeSet_Energy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Energy, CarriedEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Energy, MaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAFLAttributeSet_Energy, OverdriveThreshold, COND_None, REPNOTIFY_Always);
}

void UAFLAttributeSet_Energy::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetCarriedEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
}

void UAFLAttributeSet_Energy::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetCarriedEnergyAttribute())
	{
		// Re-clamp post-execute (PreAttributeChange covers most paths; this is the Combat set's
		// belt-and-braces shape) and detect the UPWARD threshold crossing. Old value reconstructed
		// from the applied magnitude -- exact for additive mods, the only writer today.
		const float NewValue = FMath::Clamp(GetCarriedEnergy(), 0.0f, GetMaxEnergy());
		SetCarriedEnergy(NewValue);
		const float OldValue = NewValue - Data.EvaluatedData.Magnitude;
		const float Threshold = GetOverdriveThreshold();

		if (OldValue < Threshold && NewValue >= Threshold)
		{
			// The Overkill broadcast precedent (server-side; PostGameplayEffectExecute runs on the
			// authority). FLyraVerbMessage: Instigator = the owner whose meter crossed; Magnitude =
			// the new energy value.
			AActor* OwnerActor = GetOwningActor();
			if (UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr)
			{
				FLyraVerbMessage Message;
				Message.Verb = TAG_Event_Energy_ThresholdReached_EnergySet;
				Message.Instigator = OwnerActor;
				Message.Magnitude = NewValue;
				UGameplayMessageSubsystem::Get(World).BroadcastMessage(Message.Verb, Message);

				UE_LOG(LogAFLCombat, Log,
					TEXT("AFL_ENERGY: %s crossed OverdriveThreshold upward (%.1f -> %.1f, threshold %.1f) -> Event.Energy.ThresholdReached."),
					*GetNameSafe(OwnerActor), OldValue, NewValue, Threshold);
			}
		}
	}
}

void UAFLAttributeSet_Energy::OnRep_CarriedEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Energy, CarriedEnergy, OldValue);
}

void UAFLAttributeSet_Energy::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Energy, MaxEnergy, OldValue);
}

void UAFLAttributeSet_Energy::OnRep_OverdriveThreshold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAFLAttributeSet_Energy, OverdriveThreshold, OldValue);
}
