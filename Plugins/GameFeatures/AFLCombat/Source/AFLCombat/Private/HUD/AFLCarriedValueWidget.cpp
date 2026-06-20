// Copyright C12 AI Gaming. All Rights Reserved.

#include "HUD/AFLCarriedValueWidget.h"

#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Loot/AFLLootCarryComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCarriedValueWidget)

void UAFLCarriedValueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// State A (carrying) ships active; the extraction State B is sub-pass 2B.
	if (StateSwitcher)
	{
		StateSwitcher->SetActiveWidgetIndex(0);
	}

	// Re-bind on respawn: the carry component lives on the EPHEMERAL pawn (the at-risk pool resets on death), so a
	// fresh pawn = a fresh component. Bind the controller's possess delegate first, then bind the current pawn (it
	// may already be possessed when the HUD spawns; OnPossessedPawnChanged covers every later respawn).
	if (APlayerController* PC = GetOwningPlayer())
	{
		BoundController = PC;
		PC->OnPossessedPawnChanged.AddDynamic(this, &UAFLCarriedValueWidget::HandlePossessedPawnChanged);
	}
	BindToCarryComponent(GetOwningPlayerPawn());
}

void UAFLCarriedValueWidget::NativeDestruct()
{
	if (CarryComp.IsValid())
	{
		CarryComp->OnCarriedValueChanged.RemoveDynamic(this, &UAFLCarriedValueWidget::HandleCarriedValueChanged);
		CarryComp->OnCarriedPartsChanged.RemoveDynamic(this, &UAFLCarriedValueWidget::HandleCarriedPartsChanged);
	}
	if (BoundController.IsValid())
	{
		BoundController->OnPossessedPawnChanged.RemoveDynamic(this, &UAFLCarriedValueWidget::HandlePossessedPawnChanged);
	}
	Super::NativeDestruct();
}

void UAFLCarriedValueWidget::BindToCarryComponent(APawn* Pawn)
{
	// Drop the old binding (respawn / pawn change) before re-binding -- no double-fire, no dangling delegate.
	if (CarryComp.IsValid())
	{
		CarryComp->OnCarriedValueChanged.RemoveDynamic(this, &UAFLCarriedValueWidget::HandleCarriedValueChanged);
		CarryComp->OnCarriedPartsChanged.RemoveDynamic(this, &UAFLCarriedValueWidget::HandleCarriedPartsChanged);
	}

	CarryComp = Pawn ? Pawn->FindComponentByClass<UAFLLootCarryComponent>() : nullptr;
	if (CarryComp.IsValid())
	{
		// EVENT-DRIVEN, no tick: the proven rail + parts aggregate (sub-pass 1) broadcast on every change.
		CarryComp->OnCarriedValueChanged.AddDynamic(this, &UAFLCarriedValueWidget::HandleCarriedValueChanged);
		CarryComp->OnCarriedPartsChanged.AddDynamic(this, &UAFLCarriedValueWidget::HandleCarriedPartsChanged);
	}
	RefreshCarried();
}

void UAFLCarriedValueWidget::HandlePossessedPawnChanged(APawn* /*OldPawn*/, APawn* NewPawn)
{
	BindToCarryComponent(NewPawn);   // fresh pawn (respawn) -> re-bind to its carry component (no blank HUD)
}

void UAFLCarriedValueWidget::HandleCarriedValueChanged(int32 /*NewValue*/)
{
	RefreshCarried();
}

void UAFLCarriedValueWidget::HandleCarriedPartsChanged(int32 /*PartsValue*/, int32 /*PartsCount*/)
{
	RefreshCarried();
}

void UAFLCarriedValueWidget::RefreshCarried()
{
	const int32 Total = CarryComp.IsValid() ? CarryComp->GetCarriedTotal() : 0;
	const int32 Count = CarryComp.IsValid() ? CarryComp->GetCarriedPartsCount() : 0;

	if (TotalText)
	{
		TotalText->SetText(FText::AsNumber(Total));
	}
	if (PartsText)
	{
		PartsText->SetText(FText::AsNumber(Count));
	}
	OnCarriedRefreshed(Total, Count);   // BP drives the glow / MIs (amber pulse scaling with the total)
}
