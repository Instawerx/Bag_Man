// Copyright C12 AI Gaming. All Rights Reserved.
#pragma once

#include "CommonActivatableWidget.h"
#include "Engine/TimerHandle.h"
#include "AFLW_Landing.generated.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UImage;
class UTextBlock;
class UWidgetSwitcher;

/**
 * UAFLW_Landing -- the IRONICS START SCREEN (Identity Program I-2/I-3; operator-approved mockup 2026-09-15,
 * "extra points for rolling electric neon borders").
 *
 * Full-bleed Shanty Town loop (MediaTexture; brand ground fallback), the REAL logo, and ONE sign-in card
 * with three doors: EMAIL (a 6-digit code emailed and typed IN-GAME -- creates the account on first use, the
 * SAME account as the IRONICS site), SIGN IN WITH EPIC, and PLAY NOW (a guest bound to this device; link
 * later from the System Menu). STAY SIGNED IN keeps a sealed refresh token so the next launch signs in
 * silently. Every refusal names its reason on the card; Esc always works (System Menu on the root, BACK on
 * the code step). The card's border is the rolling electric-neon material (M_AFL_NeonBorder).
 *
 * LINK MODE (UAFLW_LinkAccountCard below): the same card, opened from the System Menu by a signed-in GUEST,
 * attaches an email or Epic credential to the guest's account -- same PlayFab player, nothing lost.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLCOMBAT_API UAFLW_Landing : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UAFLW_Landing();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	// Own the input mode so the sign-in / route cards are always clickable from a cold-boot input state.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	// Doors
	UFUNCTION() void HandleSendCode();
	UFUNCTION() void HandleContinue();
	UFUNCTION() void HandleResend();
	UFUNCTION() void HandleChangeEmail();
	UFUNCTION() void HandleEpicClicked();
	UFUNCTION() void HandlePlayNow();
	UFUNCTION() void HandleStayToggled();
	UFUNCTION() void HandleCodeChanged(const FText& Text);
	UFUNCTION() void HandleCodeCommitted(const FText& Text, ETextCommit::Type Method);
	UFUNCTION() void HandleEmailCommitted(const FText& Text, ETextCommit::Type Method);

	void KickLocalPlayInit();
	void StartVideoGround();
	void HandleLoggedIn(bool bSuccess);
	/** UAFLOnlineSubsystem::OnLoggedIn -- a login that lands at ANY time while this card is up advances it. */
	void HandleOnlineLoggedIn();
	void HandlePortalLink(bool bOk, const FString& Reason);
	void PushRouteChoice();
	void ShowStep(int32 Index);
	void SetStatus(const FText& Text, bool bBad = false);
	void ArmSignInWaiter();
	void TickResend();
	void ApplyStayVisual();

	/** True for the LINK ACCOUNT variant (constructor-set so the pooled instance's activation sees it). */
	bool bLinkMode = false;
	/** Identity I-5a -- the LINK EMAIL variant of the link card: a signed-in, non-guest (Epic-first) account adds an
	 *  address. Derived on EVERY activation from the subsystem (the pooled instance serves guests and veterans alike):
	 *  the Epic door and its captions collapse, the copy says what an email adds and what a site account folds in. */
	bool bEmailOnlyLink = false;
	void ApplyLinkVariant();

private:
	UPROPERTY(Transient) TObjectPtr<UImage>           VideoImage;
	UPROPERTY(Transient) TObjectPtr<UImage>           LogoImage;
	UPROPERTY(Transient) TObjectPtr<UImage>           NeonImage;
	UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher>  Steps;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       SubText;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> EmailBox;
	UPROPERTY(Transient) TObjectPtr<UButton>          SendCodeButton;
	UPROPERTY(Transient) TObjectPtr<UButton>          EpicButton;
	UPROPERTY(Transient) TObjectPtr<UButton>          PlayNowButton;
	UPROPERTY(Transient) TObjectPtr<UWidget>          OrRow;
	UPROPERTY(Transient) TObjectPtr<UWidget>          DoorRow;
	UPROPERTY(Transient) TObjectPtr<UWidget>          CaptionsRow;
	UPROPERTY(Transient) TObjectPtr<UWidget>          StayRow;
	UPROPERTY(Transient) TObjectPtr<UWidget>          StayNoteText;
	UPROPERTY(Transient) TObjectPtr<UWidget>          RecruitNote;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       RecruitText;
	UPROPERTY(Transient) TObjectPtr<UBorder>          StayCheck;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       CodeIntroText;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> CodeBox;
	UPROPERTY(Transient) TObjectPtr<UButton>          ContinueButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       ResendText;
	UPROPERTY(Transient) TObjectPtr<UButton>          ResendButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       FooterText;

	bool bStaySignedIn = true;
	bool bSignInInFlight = false;
	/** One WHERE TO? push per activation (the waiter AND the OnLoggedIn broadcast can both report one login). */
	bool bRouteChoicePushed = false;
	FString ChallengeId;
	FString PendingEmail;
	int32 ResendSecondsLeft = 0;
	FTimerHandle ResendTimer;
};

/** The System Menu's LINK ACCOUNT card: the Landing card in link mode, on the modal layer, for a signed-in guest. */
UCLASS()
class AFLCOMBAT_API UAFLW_LinkAccountCard : public UAFLW_Landing
{
	GENERATED_BODY()
public:
	UAFLW_LinkAccountCard();
	static void Open(const UObject* WorldContext);
};
