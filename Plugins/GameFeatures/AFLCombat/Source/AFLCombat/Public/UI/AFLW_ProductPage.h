// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"

#include "AFLW_ProductPage.generated.h"

class UTextBlock;
class UButton;
class UBorder;
class UVerticalBox;

/**
 * UAFLW_ProductPage  (AFL-3250 / MAIN_MAP_LOBBY_SYSTEM_HELPER s4 / PX_STORE_BUILD_RULINGS)
 *
 * The at-shelf retail overlay: ~60% width, right-anchored, THE WORLD STAYS VISIBLE beside it --
 * "cards and pages complimentary, not obtrusive" (operator ruling). C++-built tree (the sign
 * widget precedent -- no WBP bridge risk); a data-only BP child may restyle later.
 *
 * Flow (mock 'Flows' artboard, ratified): BUY -> confirm on UI.Layer.Modal -> wallet purchase
 * (dev grant in editor, PlayFab in shipping) -> status line reads the grant/refusal ALOUD
 * (a silent rejection is a trust defect) -> EQUIP commits through the ONE selection seam
 * (ServerSetCosmeticSelection). D-7: owned single-grant rows grey the BUY.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLCOMBAT_API UAFLW_ProductPage : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UAFLW_ProductPage();

	/** Point the page at a catalog row. Reflection-reachable (the pedestal calls it by name). */
	UFUNCTION(BlueprintCallable, Category = "AFL|Retail")
	void FocusCosmeticId(FName InCosmeticId);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;

	UFUNCTION()
	void HandleBuyClicked();

	UFUNCTION()
	void HandleEquipClicked();

	/** Mouse path off the page (lap-2: ESC was the only exit and it was dead). */
	UFUNCTION()
	void HandleCloseClicked();

	/** Confirm-modal resolution (bound to the pushed modal's delegate). */
	UFUNCTION()
	void HandleConfirmResolved(bool bConfirmed);

	void RefreshFromCatalog();
	void SetStatus(const FText& Text, const FLinearColor& Color);

private:
	FName CosmeticId;
	bool bAwaitingGrant = false;
	int32 GrantPolls = 0;
	FTimerHandle GrantPollTimer;
	void PollGrant();

	// C++-built tree leaves (owned by the widget tree; raw observing pointers are the sign
	// widget's proven pattern for a RebuildWidget-constructed hierarchy).
	TObjectPtr<UTextBlock> NameText;
	TObjectPtr<UTextBlock> TierText;
	TObjectPtr<UTextBlock> PriceText;
	TObjectPtr<UTextBlock> DescText;
	TObjectPtr<UTextBlock> StatusText;
	TObjectPtr<UTextBlock> BuyLabel;
	TObjectPtr<UTextBlock> EquipLabel;
	TObjectPtr<UButton> BuyButton;
	TObjectPtr<UButton> EquipButton;
};
