// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"

#include "AFLCarriedValueWidget.generated.h"

class UAFLLootCarryComponent;
class UWidgetSwitcher;
class UTextBlock;

/**
 * UAFLCarriedValueWidget  (UI sub-pass 2A -- the carried-value HUD readout)
 *
 * Lyra C++/BP split (the ULyraWeaponUserInterface pattern): this compiled C++ base owns the binding LOGIC; a thin
 * BP child (W_AFL_CarriedValue) provides the visual via BindWidget. It surfaces the player's AT-RISK carried Watts
 * + part-count, bound EVENT-DRIVEN (no tick) to the PROVEN UAFLLootCarryComponent accessor (sub-pass 1, 948f30d7):
 * GetCarriedTotal() / GetCarriedPartsCount() + OnCarriedValueChanged / OnCarriedPartsChanged.
 *
 * The carry component lives on the EPHEMERAL pawn (the at-risk pool resets on death), so the widget RE-BINDS on the
 * controller's OnPossessedPawnChanged -- without it the HUD goes blank after respawn.
 *
 * The WidgetSwitcher ships with State A (carrying) only; the extraction State B (the progress bar) is sub-pass 2B,
 * which first EXPOSES the extraction state to the client (the Start/Complete/Failed messages are server-only -- a
 * client widget can't receive them; the 2A accessor has no such gap).
 */
UCLASS()
class AFLCOMBAT_API UAFLCarriedValueWidget : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	/** Carrying (State A) vs extracting (State B, added in 2B). Ships State A active. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> StateSwitcher;

	/** The at-risk TOTAL (cache rail + carried parts) -- GetCarriedTotal(). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TotalText;

	/** The carried PART count -- GetCarriedPartsCount(). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PartsText;

	/** The BP styles the glow / MIs from this (amber pulse scaling with the total); the C++ drives the data. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|HUD")
	void OnCarriedRefreshed(int32 Total, int32 PartsCount);

private:
	/** (Re)bind the carried-loot delegates to the given pawn's carry component, then refresh. */
	void BindToCarryComponent(APawn* Pawn);

	/** Read the proven accessor -> the BindWidget texts + the BP styling hook. */
	void RefreshCarried();

	/** Respawn: a fresh pawn = a fresh carry component -> re-bind (no blank HUD). */
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleCarriedValueChanged(int32 NewValue);

	UFUNCTION()
	void HandleCarriedPartsChanged(int32 PartsValue, int32 PartsCount);

	TWeakObjectPtr<UAFLLootCarryComponent> CarryComp;
	TWeakObjectPtr<APlayerController> BoundController;
};
