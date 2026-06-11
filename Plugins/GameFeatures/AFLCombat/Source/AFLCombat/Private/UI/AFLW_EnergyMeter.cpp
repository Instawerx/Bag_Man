// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_EnergyMeter.h"

#include "AFLCombat.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/AFLAttributeSet_Energy.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_EnergyMeter)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Energy_Overdrive_Meter, "State.Energy.Overdrive");


void UAFLW_EnergyMeter::NativeConstruct()
{
	Super::NativeConstruct();
	TryArm();
}

void UAFLW_EnergyMeter::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmRetryTimer);
	}
	if (ASC.IsValid())
	{
		if (EnergyChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()).Remove(EnergyChangedHandle);
		}
		if (OverdriveTagHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Energy_Overdrive_Meter, EGameplayTagEventType::NewOrRemoved).Remove(OverdriveTagHandle);
		}
	}
	Super::NativeDestruct();
}

void UAFLW_EnergyMeter::TryArm()
{
	// The owning player's PlayerState ASC (Get Owning Player, never the camera manager -- the
	// widget-bridge trap list). Possession/replication can land after construct: bounded poll.
	const APlayerController* PC = GetOwningPlayer();
	const APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAbilitySystemComponent* ResolvedASC = PS
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const_cast<APlayerState*>(PS)) : nullptr;
	if (!ResolvedASC || !ResolvedASC->GetSet<UAFLAttributeSet_Energy>())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(ArmRetryTimer,
				FTimerDelegate::CreateWeakLambda(this, [this] { TryArm(); }), 0.5f, false);
		}
		return;
	}
	ASC = ResolvedASC;

	EnergyChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()).AddUObject(this, &UAFLW_EnergyMeter::HandleEnergyChanged);
	OverdriveTagHandle = ASC->RegisterGameplayTagEvent(TAG_State_Energy_Overdrive_Meter, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLW_EnergyMeter::HandleOverdriveTagChanged);

	// Initial paint.
	Refresh(ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()),
		ASC->HasMatchingGameplayTag(TAG_State_Energy_Overdrive_Meter));
}

void UAFLW_EnergyMeter::HandleEnergyChanged(const FOnAttributeChangeData& Data)
{
	Refresh(Data.NewValue, ASC.IsValid() && ASC->HasMatchingGameplayTag(TAG_State_Energy_Overdrive_Meter));
}

void UAFLW_EnergyMeter::HandleOverdriveTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	const float Energy = ASC.IsValid()
		? ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetCarriedEnergyAttribute()) : 0.0f;
	Refresh(Energy, NewCount > 0);
}

void UAFLW_EnergyMeter::Refresh(float Energy, bool bOverdriven)
{
	const float MaxEnergy = ASC.IsValid()
		? FMath::Max(1.0f, ASC->GetNumericAttribute(UAFLAttributeSet_Energy::GetMaxEnergyAttribute())) : 100.0f;
	if (EnergyBar)
	{
		EnergyBar->SetPercent(Energy / MaxEnergy);
		EnergyBar->SetFillColorAndOpacity(bOverdriven ? OverdriveColor : NormalColor);
	}
	if (EnergyText)
	{
		EnergyText->SetText(FText::AsNumber(FMath::RoundToInt(Energy)));
	}
}
