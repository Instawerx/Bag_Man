// AFL kit (C2, I-1/I-7/I-13/I-14/I-25): the context-driven commit bar. Save / Revert / Equip / Buy
// shown per context; a disabled Save always carries its reason (never a dead control). The bar owns
// NO economy or selection logic -- it fires delegates; the shell routes them to the three seams.

#pragma once

#include "CommonUserWidget.h"
#include "AFLW_ActionBar.generated.h"

class UCommonButtonBase;
class UTextBlock;

UENUM(BlueprintType)
enum class EAFLActionBarContext : uint8
{
	/** Creator, nothing dirty: Save disabled (nothing to save), Revert hidden, Equip hidden. */
	CreatorClean      UMETA(DisplayName = "Creator - Clean"),
	/** Creator with unsaved changes: Save + Revert live (I-7). */
	CreatorDirty      UMETA(DisplayName = "Creator - Dirty"),
	/** A saved build is selected: Equip joins the bar (I-13). */
	SavedBuildSelected UMETA(DisplayName = "Saved Build Selected"),
	/** Store shell: Buy replaces Save/Revert (I-18). */
	StoreItem         UMETA(DisplayName = "Store Item")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAFLActionBarSave);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAFLActionBarRevert);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAFLActionBarEquip);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAFLActionBarBuy);

UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_ActionBar : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "AFL|ActionBar")
	FOnAFLActionBarSave OnSaveClicked;

	UPROPERTY(BlueprintAssignable, Category = "AFL|ActionBar")
	FOnAFLActionBarRevert OnRevertClicked;

	UPROPERTY(BlueprintAssignable, Category = "AFL|ActionBar")
	FOnAFLActionBarEquip OnEquipClicked;

	UPROPERTY(BlueprintAssignable, Category = "AFL|ActionBar")
	FOnAFLActionBarBuy OnBuyClicked;

	/** Which verbs show. Visibility is context, enablement is state -- SetSaveEnabled composes. */
	UFUNCTION(BlueprintCallable, Category = "AFL|ActionBar")
	void SetContext(EAFLActionBarContext InContext);

	/** I-14: everyone builds; SAVE gates. Disabled ALWAYS carries a reason line -- an unexplained
	 *  dead button is the exact shape the states table forbids. */
	UFUNCTION(BlueprintCallable, Category = "AFL|ActionBar")
	void SetSaveEnabled(bool bEnabled, const FText& DisabledReason);

	/** Price text on the Buy verb ("V 1,250"). Formatting is the caller's (mono face in the WBP). */
	UFUNCTION(BlueprintCallable, Category = "AFL|ActionBar")
	void SetBuyPrice(const FText& PriceText);

	UFUNCTION(BlueprintPure, Category = "AFL|ActionBar")
	EAFLActionBarContext GetContext() const { return Context; }

protected:
	virtual void NativeOnInitialized() override;

	// House law: BindWidgetOptional + null-guard; names are the WBP contract for the kit WBPs
	// authored from the ratified spec (I-25).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> Btn_Save;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> Btn_Revert;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> Btn_Equip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> Btn_Buy;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Reason;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_BuyPrice;

private:
	void ApplyContext();

	EAFLActionBarContext Context = EAFLActionBarContext::CreatorClean;
	bool bSaveEnabled = false;
	FText SaveDisabledReason;
};
