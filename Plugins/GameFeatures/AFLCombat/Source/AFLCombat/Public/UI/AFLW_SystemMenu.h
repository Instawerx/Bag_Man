// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"

#include "AFLW_SystemMenu.generated.h"

class UButton;
class UTextBlock;
class UWidget;
struct FUIInputConfig;

/**
 * UAFLW_SystemMenu -- the in-hub pause / account overlay (operator-approved mockup 2026-09-02).
 *
 * Houses the two controls the shipped client had NO way to reach (found on the first live lap): SIGN OUT
 * (drops the PlayFab + EOS/Epic session and the stored stay-signed-in token, then returns to the sign-in
 * screen) and QUIT TO DESKTOP. RESUME closes it; SETTINGS is a placeholder pending the settings surface.
 *
 * Pushed onto UI.Layer.Modal (above the Menu stack) so it overlays the front-end without disturbing it,
 * exactly like the RouteChoice fix. bIsBackHandler routes Escape / gamepad-B: from the confirm step it
 * returns to the menu list, from the menu list it closes (== RESUME).
 *
 * Built in C++ (RebuildWidget), same as AFLW_RouteChoice / AFLW_Landing; the pixel-perfect chrome from the
 * approved mockup (SVG icons, the top-bar account chip entry point) is a later WBP polish pass.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLCOMBAT_API UAFLW_SystemMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UAFLW_SystemMenu();

	/** Push a fresh System Menu onto UI.Layer.Modal for the world-context's local player. Returns it (or null). */
	static UAFLW_SystemMenu* Open(const UObject* WorldContext);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	/** Menu input mode + visible cursor -- so the overlay is interactive even when opened FROM gameplay
	 *  (the global-Esc case where no menu was focused). Cursor shown, keys routed to the UI. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION() void HandleResume();
	UFUNCTION() void HandleSettings();
	UFUNCTION() void HandleSignOut();
	UFUNCTION() void HandleQuit();
	UFUNCTION() void HandleConfirmCancel();
	UFUNCTION() void HandleConfirmProceed();

private:
	/** Which destructive action the confirm panel is currently gating. */
	enum class EConfirm : uint8 { None, SignOut, Quit };
	EConfirm Pending = EConfirm::None;

	void ShowMenu();
	void ShowConfirm(EConfirm Which);
	void DoSignOut();
	void DoQuit();

	// Panels toggled by visibility (built once in RebuildWidget).
	UPROPERTY(Transient) TObjectPtr<UWidget> MenuPanel = nullptr;
	UPROPERTY(Transient) TObjectPtr<UWidget> ConfirmPanel = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmTitle = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmBody = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ConfirmProceedLabel = nullptr;

	/** Default focus targets per step. */
	UPROPERTY(Transient) TObjectPtr<UButton> SignOutButton = nullptr;      // menu step
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmCancelButton = nullptr; // confirm step
};
