// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"

#include "AFLW_EnergyMeter.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 * UAFLW_EnergyMeter  (energy cycle 2 -- the first AFL HUD extension widget)
 *
 * Plain UUserWidget (NOT a CommonActivatable -- a passive meter never takes focus; activatables are
 * the focus/input-stack menu shape). The C++ half owns every binding because BP graph authoring is
 * bridge-hostile; the WBP child owns layout only (a BindWidget ProgressBar + optional value text).
 *
 * Bindings (client-side, NativeConstruct with a deferred-arm poll for the possession race):
 *  - CarriedEnergy bar: GetGameplayAttributeValueChangeDelegate(CarriedEnergy) + an initial read.
 *    REPNOTIFY feeds the delegate on clients. NOT the Event.Energy.Collected message -- that
 *    broadcast is SERVER-WORLD-ONLY by design; a client HUD would never hear it (the grounded trap).
 *  - Overdrive restyle: RegisterGameplayTagEvent(State.Energy.Overdrive) flips the bar fill color
 *    (normal cyan -> overdrive orange); the threshold tick mark is the WBP child's static layout at
 *    80% (MaxEnergy/OverdriveThreshold are design constants this cycle).
 *
 * Slotted into the Lyra HUD via the experience's GameFeatureAction_AddWidgets row
 * (HUD.Slot.Equipment for cycle 2; a center-bottom layout fork is the named debt).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_EnergyMeter : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** The bar the WBP child must provide (BindWidget = compile-enforced). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> EnergyBar;

	/** Optional numeric readout (BindWidgetOptional -- the child may omit it). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnergyText;

	/** Bar fill while normal / while overdriven. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Energy")
	FLinearColor NormalColor = FLinearColor(0.0f, 0.94f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Energy")
	FLinearColor OverdriveColor = FLinearColor(1.0f, 0.55f, 0.0f);

private:
	void TryArm();
	void HandleEnergyChanged(const FOnAttributeChangeData& Data);
	void HandleOverdriveTagChanged(const FGameplayTag Tag, int32 NewCount);
	void Refresh(float Energy, bool bOverdriven);

	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle EnergyChangedHandle;
	FDelegateHandle OverdriveTagHandle;
	FTimerHandle ArmRetryTimer;
};
