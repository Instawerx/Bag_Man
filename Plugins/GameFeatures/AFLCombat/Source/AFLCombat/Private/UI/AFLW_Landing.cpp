// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_Landing.h"

#include "AFLCombat.h"
#include "AFLOnlineSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "CommonUIExtensions.h"
#include "CommonUserSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/AFLW_RouteChoice.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_Landing)

namespace AFLLanding
{
	// Brand lock (IRONICS_CC_DESIGN_BRIEF section 0).
	static const FLinearColor Surface(0.0028f, 0.0043f, 0.0231f, 0.94f); // #0E122B glass
	static const FLinearColor Accent(0.013f, 0.102f, 1.0f, 1.0f);        // #1E5AFF
	static const FLinearColor Dim(1.f, 1.f, 1.f, 0.55f);
	static const FLinearColor Bad(1.0f, 0.16f, 0.22f, 1.0f);

	// Real game art + media (soft paths -- every miss degrades gracefully).
	static const TCHAR* LogoPath  = TEXT("/Game/Characters/Cosmetics/T_IRONICS_Color_Logo_BC.T_IRONICS_Color_Logo_BC");
	static const TCHAR* MediaTex  = TEXT("/Game/Movies/MT_AFL_StartLoop.MT_AFL_StartLoop");
	static const TCHAR* MediaPlyr = TEXT("/Game/Movies/MP_AFL_StartLoop.MP_AFL_StartLoop");
	static const TCHAR* MediaSrc  = TEXT("/Game/Movies/MS_AFL_StartLoop.MS_AFL_StartLoop");

	static const TCHAR* PrefsSection = TEXT("/Script/AFLCombat.AFLAuthPrefs");

	static UFont* Orbitron() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/Orbitron.Orbitron")); }
	static UFont* NotoSans() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/NotoSans.NotoSans")); }

	static void Style(UTextBlock* T, UFont* F, float Size, const FLinearColor& C)
	{
		if (!T) return;
		FSlateFontInfo Info(F, static_cast<int32>(Size));
		T->SetFont(Info);
		T->SetColorAndOpacity(FSlateColor(C));
	}
}

UAFLW_Landing::UAFLW_Landing()
{
	bIsBackHandler = true; // root screen: back is swallowed, never a trap or an escape-to-nothing
}

TSharedRef<SWidget> UAFLW_Landing::RebuildWidget()
{
	using namespace AFLLanding;
	UFont* Display = Orbitron();
	UFont* Body = NotoSans();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LandingCanvas"));
	WidgetTree->RootWidget = Canvas;

	// VIDEO GROUND -- full-bleed. MediaTexture when the loop exists; brand gradient fallback via tint.
	VideoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("VideoGround"));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(VideoImage))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}
	VideoImage->SetColorAndOpacity(FLinearColor(0.06f, 0.08f, 0.14f)); // pre-video ground

	// THE REAL LOGO (T_IRONICS_Color_Logo_BC -- game art only, naming ruling 2026-09-01).
	LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Logo"));
	if (UTexture2D* Logo = LoadObject<UTexture2D>(nullptr, LogoPath))
	{
		LogoImage->SetBrushFromTexture(Logo, false);
	}
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(LogoImage))
	{
		S->SetAnchors(FAnchors(0.f, 0.5f, 0.f, 0.5f));
		S->SetAlignment(FVector2D(0.f, 0.5f));
		S->SetPosition(FVector2D(84.f, -30.f));
		S->SetSize(FVector2D(300.f, 300.f));
	}

	// SIGN-IN CARD (approved mock: Epic primary, stay-signed-in, recruit note).
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card"));
	Card->SetBrushColor(Surface);
	Card->SetPadding(FMargin(26.f));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Card))
	{
		S->SetAnchors(FAnchors(1.f, 0.5f, 1.f, 0.5f));
		S->SetAlignment(FVector2D(1.f, 0.5f));
		S->SetPosition(FVector2D(-64.f, 0.f));
		S->SetSize(FVector2D(360.f, 0.f));
		S->SetAutoSize(true);
	}
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CardCol"));
	Card->SetContent(Col);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Style(Title, Display, 15.f, FLinearColor::White);
	Title->SetText(NSLOCTEXT("AFLLanding", "SignIn", "SIGN IN"));
	Col->AddChildToVerticalBox(Title);

	UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Sub"));
	Style(Sub, Body, 11.f, Dim);
	Sub->SetAutoWrapText(true);
	Sub->SetText(NSLOCTEXT("AFLLanding", "SubLine", "One account across the game and the IRONICS site — your Epic identity."));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Sub)) { VS->SetPadding(FMargin(0.f, 6.f, 0.f, 16.f)); }

	SignInButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SignInButton"));
	SignInButton->SetBackgroundColor(Accent);
	SignInLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SignInLabel"));
	Style(SignInLabel, Display, 14.f, FLinearColor::White);
	SignInLabel->SetText(NSLOCTEXT("AFLLanding", "Epic", "SIGN IN WITH EPIC"));
	SignInButton->AddChild(SignInLabel);
	SignInButton->OnClicked.AddDynamic(this, &UAFLW_Landing::HandleSignInClicked);
	Col->AddChildToVerticalBox(SignInButton);

	// STAY SIGNED IN toggle row (persisted preference; EOS PersistentAuth consumes it).
	UButton* StayRow = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StayRow"));
	StayRow->SetBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	StayRow->OnClicked.AddDynamic(this, &UAFLW_Landing::HandleStayToggled);
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(StayRow)) { VS->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f)); }
	{
		UVerticalBox* StayCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		StayRow->AddChild(StayCol);
		StayCheck = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StayCheck"));
		StayCheck->SetBrushColor(Accent);
		StayCheck->SetPadding(FMargin(2.f));
		UTextBlock* StayLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(StayLabel, Body, 12.f, FLinearColor(1.f, 1.f, 1.f, 0.85f));
		StayLabel->SetText(NSLOCTEXT("AFLLanding", "Stay", "✓  Stay signed in on this device"));
		StayCheck->SetContent(StayLabel);
		StayCol->AddChildToVerticalBox(StayCheck);
		UTextBlock* StayNote = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(StayNote, Body, 10.f, FLinearColor(1.f, 1.f, 1.f, 0.4f));
		StayNote->SetAutoWrapText(true);
		StayNote->SetText(NSLOCTEXT("AFLLanding", "StayNote", "Epic persistent auth — one-click next time. No password is ever stored. Sign-out clears it."));
		if (UVerticalBoxSlot* VS = StayCol->AddChildToVerticalBox(StayNote)) { VS->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
	}

	// Recruit note (the proven signup grant).
	UBorder* Recruit = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Recruit"));
	Recruit->SetBrushColor(FLinearColor(0.013f, 0.102f, 1.0f, 0.12f));
	Recruit->SetPadding(FMargin(12.f, 10.f));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Recruit)) { VS->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f)); }
	{
		UTextBlock* R = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(R, Body, 11.f, FLinearColor(1.f, 1.f, 1.f, 0.75f));
		R->SetAutoWrapText(true);
		R->SetText(NSLOCTEXT("AFLLanding", "Recruit", "NEW? First sign-in creates your IRONICS account — and grants 3 Weapon Credits."));
		Recruit->SetContent(R);
	}

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
	Style(StatusText, Body, 11.f, Dim);
	StatusText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(StatusText)) { VS->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f)); }

	return Super::RebuildWidget();
}

void UAFLW_Landing::NativeOnActivated()
{
	Super::NativeOnActivated();
	using namespace AFLLanding;

	// Restore the persisted preference.
	bool bSaved = true;
	if (GConfig && GConfig->GetBool(PrefsSection, TEXT("bStaySignedIn"), bSaved, GGameUserSettingsIni))
	{
		bStaySignedIn = bSaved;
	}
	if (StayCheck)
	{
		StayCheck->SetBrushColor(bStaySignedIn ? Accent : FLinearColor(1.f, 1.f, 1.f, 0.08f));
	}

	KickLocalPlayInit();
	StartVideoGround();

	// Remembered device: the dev CustomID (and, shipping, EOS persistent auth) may already be mid-login
	// from GameInstance init -- a completed login skips the card straight to the route choice.
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (Online && Online->IsLoggedIn() && bStaySignedIn)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: already signed in (remembered) -> route choice."));
		PushRouteChoice();
	}

#if !UE_BUILD_SHIPPING
	if (StatusText)
	{
		StatusText->SetText(NSLOCTEXT("AFLLanding", "DevHint", "DEV: sign-in uses the dev identity in PIE."));
	}
#endif
}

bool UAFLW_Landing::NativeOnHandleBackAction()
{
	return true; // root: nothing behind us
}

void UAFLW_Landing::KickLocalPlayInit()
{
	// The press-start slot normally triggers local-play init; forced-on PC the landing owns it.
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UCommonUserSubsystem* Users = GI->GetSubsystem<UCommonUserSubsystem>())
		{
			Users->TryToInitializeForLocalPlay(0, FInputDeviceId(), false);
		}
	}
}

void UAFLW_Landing::StartVideoGround()
{
	using namespace AFLLanding;
	UMediaTexture* Tex = LoadObject<UMediaTexture>(nullptr, MediaTex);
	UMediaPlayer* Player = LoadObject<UMediaPlayer>(nullptr, MediaPlyr);
	UMediaSource* Source = LoadObject<UMediaSource>(nullptr, MediaSrc);
	if (Tex && Player && Source && VideoImage)
	{
		Player->SetLooping(true);
		Player->OpenSource(Source);
		VideoImage->SetBrushFromMaterial(nullptr);
		FSlateBrush Brush;
		Brush.SetResourceObject(Tex);
		VideoImage->SetBrush(Brush);
		VideoImage->SetColorAndOpacity(FLinearColor::White);
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: start loop playing."));
	}
	else
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: no start loop media yet -- brand ground fallback."));
	}
}

void UAFLW_Landing::HandleSignInClicked()
{
	using namespace AFLLanding;
	if (bSignInInFlight)
	{
		return;
	}
	bSignInInFlight = true;
	if (GConfig)
	{
		GConfig->SetBool(PrefsSection, TEXT("bStaySignedIn"), bStaySignedIn, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	if (StatusText)
	{
		StatusText->SetText(NSLOCTEXT("AFLLanding", "SigningIn", "Signing in…"));
	}
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online)
	{
		HandleLoggedIn(false);
		return;
	}
	// Dev = CustomID; shipping = EOS OIDC (PersistentAuth honors the saved preference on that path).
	Online->EnsureLogin();
	Online->CallWhenLoggedIn([WeakThis = TWeakObjectPtr<UAFLW_Landing>(this)](bool bOk)
	{
		if (UAFLW_Landing* Self = WeakThis.Get())
		{
			Self->HandleLoggedIn(bOk);
		}
	}, 12.0f);
}

void UAFLW_Landing::HandleStayToggled()
{
	using namespace AFLLanding;
	bStaySignedIn = !bStaySignedIn;
	if (StayCheck)
	{
		StayCheck->SetBrushColor(bStaySignedIn ? Accent : FLinearColor(1.f, 1.f, 1.f, 0.08f));
	}
}

void UAFLW_Landing::HandleLoggedIn(bool bSuccess)
{
	using namespace AFLLanding;
	bSignInInFlight = false;
	if (!bSuccess)
	{
		if (StatusText)
		{
			StatusText->SetText(NSLOCTEXT("AFLLanding", "Failed", "Sign-in failed — check the connection and try again."));
			StatusText->SetColorAndOpacity(FSlateColor(Bad));
		}
		return;
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: signed in -> route choice."));
	PushRouteChoice();
}

void UAFLW_Landing::PushRouteChoice()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		return;
	}
	UCommonActivatableWidget* Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
		FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")),
		TSubclassOf<UCommonActivatableWidget>(UAFLW_RouteChoice::StaticClass()));
	if (UAFLW_RouteChoice* Choice = Cast<UAFLW_RouteChoice>(Pushed))
	{
		Choice->OnRouteChosen.AddWeakLambda(this, [this](bool /*bMatchmaking*/)
		{
			DeactivateWidget(); // landing done -> the frontend flow continues to the home screen
		});
	}
}
