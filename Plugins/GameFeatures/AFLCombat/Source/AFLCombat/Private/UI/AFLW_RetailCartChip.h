// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "AFLW_RetailCartChip.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UAFLRetailSubsystem;

/**
 * UAFLW_RetailCartChip -- the cart chip (distributed retail S2; operator: "easily minimized or opened
 * at player discretion. Non intrusive best practices").
 *
 * Top-right, tiny when collapsed ("3 · 2,970 V · [V]"), expands to the line list + checkout row.
 * Exists only while the cart is non-empty or a till/checkout is active (chip discipline, plan s"Cart
 * discipline"). All content pushed by the subsystem; renders + forwards clicks only.
 */
UCLASS()
class UAFLW_RetailCartChip : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerSubsystem(UAFLRetailSubsystem* InOwner) { Owner = InOwner; }

	/** One call renders the whole chip: collapsed shows SummaryLine only; expanded adds lines + checkout.
	 *  An empty CheckoutLabel hides the checkout row (empty-cart courtesy view). */
	void SetCartView(const FText& SummaryLine, const TArray<FText>& Lines, const FText& CheckoutLabel,
		const FText& Status, bool bExpanded);

	/** Reposition the pinned corner panel (lower-right ruling; the subsystem stacks it above the card). */
	void SetCornerOffset(const FVector2D& Offset);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION() void HandleToggle();
	UFUNCTION() void HandleCheckout();

private:
	UPROPERTY(Transient) TObjectPtr<UAFLRetailSubsystem> Owner;

	UPROPERTY(Transient) TObjectPtr<UBorder>      Panel;
	UPROPERTY(Transient) TObjectPtr<UButton>      ToggleButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>   SummaryText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> LinesBox;
	UPROPERTY(Transient) TObjectPtr<UButton>      CheckoutButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>   CheckoutText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>   StatusText;
};
