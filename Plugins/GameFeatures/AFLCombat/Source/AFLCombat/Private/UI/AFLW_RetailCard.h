// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "AFLW_RetailCard.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UAFLRetailSubsystem;

/**
 * UAFLW_RetailCard -- the SMALL CARD (distributed retail S2, ratified mock "SmallCard").
 *
 * Bottom-right corner, C++-built, NOT a CommonUI activatable ON PURPOSE: the world never dims, game
 * input keeps running, movement stays live (the ruled non-intrusive flow). Key-first (F/C/Q/E hints
 * on the rows); the rows are also clickable when a cursor exists. All state text is PUSHED by the
 * retail subsystem -- the card renders and forwards clicks, nothing more.
 */
UCLASS()
class UAFLW_RetailCard : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwnerSubsystem(UAFLRetailSubsystem* InOwner) { Owner = InOwner; }

	void SetHeader(const FText& Name, const FText& Meta, const FText& Price);
	void SetWearState(const FText& State);
	void SetBuyRow(const FText& Label, bool bEnabled);
	void SetStatus(const FText& Text, const FLinearColor& Color);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION() void HandleBuy();
	UFUNCTION() void HandleCart();
	UFUNCTION() void HandleDiscard();
	UFUNCTION() void HandleDetails();

private:
	UPROPERTY(Transient) TObjectPtr<UAFLRetailSubsystem> Owner;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MetaText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PriceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WearText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UButton>    BuyButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BuyLabel;
	UPROPERTY(Transient) TObjectPtr<UButton>    CartButton;
	UPROPERTY(Transient) TObjectPtr<UButton>    DiscardButton;
	UPROPERTY(Transient) TObjectPtr<UButton>    DetailsButton;
};
