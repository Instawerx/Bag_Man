// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"

#include "AFLW_Landing.generated.h"

class UBorder;
class UButton;
class UImage;
class UTextBlock;

/**
 * UAFLW_Landing -- the IRONICS START SCREEN (UX-flow SSOT N1, ticket AFL-3024; operator-approved
 * mock 2026-09-01).
 *
 * Full-bleed 4K Shanty Town action loop (MediaTexture, soft-referenced -- a brand gradient stands
 * in until the render lands), the REAL game logo, and the sign-in card: SIGN IN WITH EPIC is the
 * one primary CTA (the active identity spine -- website portal and game anchor the same Epic sub;
 * GameLift/AWS-over-PlayFab ruling: no new PlayFab auth surface). First sign-in creates the
 * account (OIDC CreateAccount:true) and fires the proven 3-weapon-credit recruit grant. STAY
 * SIGNED IN persists as a preference the EOS PersistentAuth path consumes -- no password is ever
 * stored. Hosted in the frontend press-start slot (bAlwaysShowStartScreen); owns local-play init.
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
	virtual bool NativeOnHandleBackAction() override; // root screen: swallow back
	// Own the input mode so the sign-in / route cards are always clickable from a cold-boot input state.
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION() void HandleSignInClicked();
	UFUNCTION() void HandleStayToggled();

	void KickLocalPlayInit();
	void StartVideoGround();
	void HandleLoggedIn(bool bSuccess);
	/** UAFLOnlineSubsystem::OnLoggedIn -- a login that lands at ANY time while this card is up advances it.
	 *  (New-player bug 2026-09-13: the one-shot 12 s waiter timed out during the browser sign-in, painted
	 *  "failed", and the later real success had no listener -- the player was stranded on the card.) */
	void HandleOnlineLoggedIn();
	void PushRouteChoice();

private:
	UPROPERTY(Transient) TObjectPtr<UImage>     VideoImage;
	UPROPERTY(Transient) TObjectPtr<UImage>     LogoImage;
	UPROPERTY(Transient) TObjectPtr<UButton>    SignInButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SignInLabel;
	UPROPERTY(Transient) TObjectPtr<UBorder>    StayCheck;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;

	bool bStaySignedIn = true;
	bool bSignInInFlight = false;
	/** One WHERE TO? push per activation: the login waiter AND the OnLoggedIn broadcast can both report the same
	 *  success (ResolveLogin drains waiters, then broadcasts). Re-armed in NativeOnActivated (pooled instance). */
	bool bRouteChoicePushed = false;
};
