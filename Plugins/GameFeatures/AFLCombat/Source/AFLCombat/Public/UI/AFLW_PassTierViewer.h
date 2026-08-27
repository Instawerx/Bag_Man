// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "AFLLiveOpsTypes.h"
#include "CommonActivatableWidget.h"
#include "AFLW_PassTierViewer.generated.h"

class UAFLPassProgressComponent;
class UAFLPassSeasonAsset;
class UCommonTextBlock;
class UPanelWidget;
class UTextBlock;
class UButton;
class UImage;

/**
 * One row of the tier ladder.
 *
 * MIRRORS UAFLW_CreatorChannelRowBase, which works: a C++ base carrying the data plus a
 * BlueprintImplementableEvent, and a WBP that draws it. The row does not read the component -- it is
 * TOLD what to draw. A row that fetched its own state could render a different tier than the one it
 * was spawned for, which is the failure the creator's rail already documents.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_PassTierRowBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row")
	int32 TierIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row")
	FText TierLabelText;

	/** Reward ids, empty when that track gives nothing at this tier. */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row")
	FName FreeRewardId;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row")
	FName PremiumRewardId;

	/** Has the player reached this tier? */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row")
	bool bEarned = false;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row")
	bool bFreeClaimed = false;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row")
	bool bPremiumClaimed = false;

	/**
	 * Would a claim on this track actually grant right now?
	 *
	 * Straight from IsTrackClaimable, which mirrors ServerClaimTier's own conditions. The WBP must
	 * enable its claim control on THIS and nothing weaker -- a button lit by "earned" alone would
	 * refuse for an unsubscribed player, which is the silent no-op the states table forbids.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row")
	bool bFreeClaimable = false;

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row")
	bool bPremiumClaimable = false;

	/** Set immediately before OnRowDataSet fires. */
	void SetRowData(const struct FAFLPassTierRowData& In);

	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Pass|Row")
	void OnRowDataSet();
};

/** The values a row draws. A struct so adding a field is one edit, not one per assignment. */
USTRUCT(BlueprintType)
struct FAFLPassTierRowData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row") int32 TierIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row") FText TierLabelText;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row") FName FreeRewardId;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row") FName PremiumRewardId;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row") bool bEarned = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row") bool bFreeClaimed = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row") bool bPremiumClaimed = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row") bool bFreeClaimable = false;
	UPROPERTY(BlueprintReadOnly, Category = "AFL|Pass|Row") bool bPremiumClaimable = false;
};

/**
 * The Battle Pass tier viewer.
 *
 * READS THE SPINE, DECIDES NOTHING. Every value comes from UAFLPassProgressComponent's read API --
 * IsTierEarned / IsTrackClaimed / IsTrackClaimable / GetUnclaimablePremiumCount -- and claiming goes
 * back through ServerClaimTier. The widget never computes whether something is claimable, because a
 * second implementation of that rule would drift from the server's and the drift would show up as a
 * button that refuses.
 *
 * ⚠ NO PRICE, AND NO PURCHASE. The upsell shows how many rewards are waiting and raises an event;
 * what a subscription costs lives in IRONICS_PRICING_SSOT ($5/month, real money) and the purchase
 * path is the existing entitlement seam. A price string here would be a third place for it to live.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_PassTierViewer : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Rebuild every row from the component. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Pass|Viewer")
	void RefreshLadder();

	/** Claim one tier, then refresh. Server decides what is owed; this only asks. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Pass|Viewer")
	void RequestClaimTier(int32 TierIndex);

	/** Claim everything earned and unclaimed. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Pass|Viewer")
	void RequestClaimAll();

	/** How many premium rewards are waiting behind the subscription. 0 = do not show the upsell. */
	UFUNCTION(BlueprintPure, Category = "AFL|Pass|Viewer")
	int32 GetUpsellCount() const;

	/** Raised when the player asks to subscribe. The WBP routes it; this class does not price it. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Pass|Viewer")
	void OnUpsellRequested();

protected:
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** The player's progress. Resolved from the owning PlayerState; null off a live session. */
	UAFLPassProgressComponent* GetProgress() const;

	/** Rows are spawned into this. BindWidgetOptional so an unauthored WBP still compiles. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Pass|Viewer")
	TObjectPtr<UPanelWidget> TierListContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Pass|Viewer")
	TObjectPtr<UCommonTextBlock> SeasonNameText;

	/** "12 / 100". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Pass|Viewer")
	TObjectPtr<UCommonTextBlock> TierCounterText;

	/** "7 rewards waiting" -- hidden when the count is 0. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Pass|Viewer")
	TObjectPtr<UCommonTextBlock> UpsellText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Pass|Viewer")
	TObjectPtr<UButton> ClaimAllButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Pass|Viewer")
	TObjectPtr<UButton> UpsellButton;

	/** The row widget spawned per tier. */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Pass|Viewer")
	TSubclassOf<UAFLW_PassTierRowBase> RowClass;

	UFUNCTION()
	void HandleClaimAllClicked();

	UFUNCTION()
	void HandleUpsellClicked();

	UFUNCTION()
	void HandleProgressChanged();
};
