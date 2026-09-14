// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"

#include "AFLW_PurchaseConfirm.generated.h"

class UTextBlock;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAFLPurchaseConfirmResolved, bool, bConfirmed);

/**
 * UAFLW_PurchaseConfirm -- the ruled confirm step on UI.Layer.Modal (mock 'Flows' artboard).
 * One product line, the wallet delta, CONFIRM / CANCEL. C++-built tree; fires Resolved exactly
 * once and deactivates itself.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLCOMBAT_API UAFLW_PurchaseConfirm : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UAFLW_PurchaseConfirm();

	UPROPERTY(BlueprintAssignable, Category = "AFL|Retail")
	FAFLPurchaseConfirmResolved Resolved;

	UFUNCTION(BlueprintCallable, Category = "AFL|Retail")
	void Configure(const FText& ProductName, const FText& PriceLine);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// POOLED INSTANCE (CommonUI layer stacks recycle activatable widgets): re-arm the one-shot `bFired` latch per
	// activation, or the second purchase dialog of a session ignores CONFIRM, CANCEL and Esc (the same defect
	// that killed the WHERE TO? cards -- see UAFLW_RouteChoice::NativeOnActivated).
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;

	UFUNCTION()
	void HandleConfirm();

	UFUNCTION()
	void HandleCancel();

private:
	void Fire(bool bConfirmed);
	bool bFired = false;
	TObjectPtr<UTextBlock> NameText;
	TObjectPtr<UTextBlock> PriceText;
};
