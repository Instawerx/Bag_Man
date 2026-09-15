// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_Landing.h"

#include "AFLCombat.h"
#include "AFLOnlineSubsystem.h"
#include "UI/AFLSystemMenuSubsystem.h"   // Esc on the root screen -> System Menu (QUIT TO DESKTOP)
#include "Blueprint/WidgetTree.h"
#include "CommonUIExtensions.h"
#include "CommonUserSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Materials/MaterialInterface.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"
#include "UI/AFLW_RouteChoice.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_Landing)

namespace AFLLanding
{
	// Brand lock (IRONICS_CC_DESIGN_BRIEF section 0) -- the values the approved mockup used.
	static const FLinearColor Surface(0.0028f, 0.0043f, 0.0231f, 0.94f); // #0E122B at 94%
	static const FLinearColor Accent(0.013f, 0.102f, 1.0f, 1.0f);        // #1E5AFF
	static const FLinearColor AccentInk(0.276f, 0.445f, 1.0f, 1.0f);     // #8FB0FF
	static const FLinearColor AccentFill(0.013f, 0.102f, 1.0f, 0.16f);
	static const FLinearColor Field(1.f, 1.f, 1.f, 0.06f);
	static const FLinearColor Dim(1.f, 1.f, 1.f, 0.55f);
	static const FLinearColor Faint(1.f, 1.f, 1.f, 0.45f);
	static const FLinearColor Bad(1.0f, 0.145f, 0.18f, 1.0f);           // #FF6B78

	static const TCHAR* LogoPath  = TEXT("/Game/Characters/Cosmetics/T_IRONICS_Logo_Transparent.T_IRONICS_Logo_Transparent");
	static const TCHAR* MediaTex  = TEXT("/Game/Movies/MT_AFL_StartLoop.MT_AFL_StartLoop");
	static const TCHAR* MediaPlyr = TEXT("/Game/Movies/MP_AFL_StartLoop.MP_AFL_StartLoop");
	static const TCHAR* MediaSrc  = TEXT("/Game/Movies/MS_AFL_StartLoop.MS_AFL_StartLoop");
	static const TCHAR* NeonMat   = TEXT("/Game/BagMan/UI/Materials/M_AFL_NeonBorder.M_AFL_NeonBorder");

	static const TCHAR* PrefsSection = TEXT("/Script/AFLCombat.AFLAuthPrefs");
	static constexpr int32 ResendCooldownSeconds = 45;
	static constexpr float CardWidth = 452.f;

	static UFont* Orbitron() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/Orbitron.Orbitron")); }
	static UFont* NotoSans() { return LoadObject<UFont>(nullptr, TEXT("/Game/UI/Foundation/Fonts/NotoSans.NotoSans")); }
	static UFont* Mono()     { return LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono")); }

	static void Style(UTextBlock* T, UFont* F, float Size, const FLinearColor& C)
	{
		if (!T) return;
		FSlateFontInfo Info(F, static_cast<int32>(Size));
		T->SetFont(Info);
		T->SetColorAndOpacity(FSlateColor(C));
	}

	static FString MaskEmail(const FString& Email)
	{
		int32 At;
		if (!Email.FindChar(TEXT('@'), At) || At < 1) return Email;
		return Email.Left(1) + TEXT("•••••") + Email.Mid(At);
	}

	static FString DigitsOnly(const FString& In, int32 Max)
	{
		FString Out;
		for (const TCHAR C : In) { if (FChar::IsDigit(C) && Out.Len() < Max) Out.AppendChar(C); }
		return Out;
	}
}

UAFLW_Landing::UAFLW_Landing()
{
	bIsBackHandler = true; // root screen: back is never a trap or an escape-to-nothing
}

UAFLW_LinkAccountCard::UAFLW_LinkAccountCard()
{
	bLinkMode = true;
}

void UAFLW_LinkAccountCard::Open(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	ULocalPlayer* LP = World && World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetLocalPlayer() : nullptr;
	if (!LP) return;
	UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
		FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Modal")),
		TSubclassOf<UCommonActivatableWidget>(UAFLW_LinkAccountCard::StaticClass()));
}

TSharedRef<SWidget> UAFLW_Landing::RebuildWidget()
{
	using namespace AFLLanding;
	UFont* Display = Orbitron();
	UFont* Body = NotoSans();
	UFont* Data = Mono();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LandingCanvas"));
	WidgetTree->RootWidget = Canvas;

	// GROUND: the loop (root) or a dim scrim (link mode, over the System Menu).
	VideoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("VideoGround"));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(VideoImage))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}
	VideoImage->SetColorAndOpacity(bLinkMode ? FLinearColor(0.f, 0.f, 0.f, 0.55f) : FLinearColor(0.06f, 0.08f, 0.14f));

	LogoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Logo"));
	if (UTexture2D* Logo = LoadObject<UTexture2D>(nullptr, LogoPath)) { LogoImage->SetBrushFromTexture(Logo, false); }
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(LogoImage))
	{
		S->SetAnchors(FAnchors(0.f, 0.5f, 0.f, 0.5f));
		S->SetAlignment(FVector2D(0.f, 0.5f));
		S->SetPosition(FVector2D(84.f, -30.f));
		S->SetSize(FVector2D(300.f, 300.f));
	}
	if (bLinkMode) { LogoImage->SetVisibility(ESlateVisibility::Collapsed); }

	// THE CARD, wrapped so the rolling neon border rides 3 px outside it.
	UOverlay* CardWrap = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CardWrap"));
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(CardWrap))
	{
		S->SetAnchors(bLinkMode ? FAnchors(0.5f, 0.5f, 0.5f, 0.5f) : FAnchors(1.f, 0.5f, 1.f, 0.5f));
		S->SetAlignment(bLinkMode ? FVector2D(0.5f, 0.5f) : FVector2D(1.f, 0.5f));
		S->SetPosition(bLinkMode ? FVector2D(0.f, 0.f) : FVector2D(-44.f, 0.f));
		S->SetSize(FVector2D(CardWidth + 6.f, 0.f));
		S->SetAutoSize(true);
	}
	NeonImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("NeonBorder"));
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, NeonMat))
	{
		NeonImage->SetBrushFromMaterial(Mat);
	}
	else
	{
		// No material yet (asset not authored on this build): a static accent hairline, never a hole.
		NeonImage->SetColorAndOpacity(FLinearColor(0.013f, 0.102f, 1.0f, 0.35f));
	}
	NeonImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* OS = CardWrap->AddChildToOverlay(NeonImage))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card"));
	Card->SetBrushColor(Surface);
	Card->SetPadding(FMargin(22.f, 20.f, 22.f, 18.f));
	if (UOverlaySlot* OS = CardWrap->AddChildToOverlay(Card))
	{
		OS->SetPadding(FMargin(3.f));
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CardCol"));
	Card->SetContent(Col);

	auto Label = [&](const TCHAR* Name, UFont* F, float Size, const FLinearColor& C, const FText& Text, bool bWrap = true) -> UTextBlock*
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Style(T, F, Size, C);
		T->SetAutoWrapText(bWrap);
		T->SetText(Text);
		return T;
	};
	auto Button = [&](const TCHAR* Name, const FText& Text, const FLinearColor& Fill, const FLinearColor& Ink, float Height = 38.f) -> UButton*
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		B->SetBackgroundColor(Fill);
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(T, Display, 13.f, Ink);
		T->SetText(Text);
		T->SetJustification(ETextJustify::Center);
		B->AddChild(T);
		// A UButton's content padding lives on its child slot (the height knob: 38 px doors, 30 px text buttons).
		if (UButtonSlot* BS = Cast<UButtonSlot>(T->Slot)) { BS->SetPadding(FMargin(12.f, (Height - 18.f) * 0.5f)); }
		return B;
	};
	auto MakeField = [&](const TCHAR* Name, const FText& Hint, UFont* F, float Size) -> UEditableTextBox*
	{
		UEditableTextBox* E = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), Name);
		E->SetHintText(Hint);
		E->WidgetStyle.SetFont(FSlateFontInfo(F, static_cast<int32>(Size)));
		E->WidgetStyle.SetForegroundColor(FSlateColor(FLinearColor::White));
		E->WidgetStyle.SetBackgroundColor(FSlateColor(AFLLanding::Field));
		E->WidgetStyle.SetPadding(FMargin(12.f, 9.f));
		return E;
	};

	TitleText = Label(TEXT("Title"), Display, 20.f, FLinearColor::White,
		bLinkMode ? NSLOCTEXT("AFLLanding", "LinkTitle", "LINK ACCOUNT") : NSLOCTEXT("AFLLanding", "SignIn", "SIGN IN"), false);
	Col->AddChildToVerticalBox(TitleText);
	SubText = Label(TEXT("Sub"), Body, 12.f, Dim, bLinkMode
		? NSLOCTEXT("AFLLanding", "LinkSub", "Keep everything you've earned on this PC and take it anywhere. Linking makes this your IRONICS account on the site too.")
		: NSLOCTEXT("AFLLanding", "SubLine", "One account across the game and the IRONICS site. New here? Signing in creates it."));
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(SubText)) { VS->SetPadding(FMargin(0.f, 4.f, 0.f, 12.f)); }

	Steps = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Steps"));
	Col->AddChildToVerticalBox(Steps);

	// ---- STEP 0: the doors ---------------------------------------------------------------------
	UVerticalBox* Doors = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Doors"));
	Steps->AddChild(Doors);

	Doors->AddChildToVerticalBox(Label(TEXT("EmailLabel"), Data, 11.f, Dim, NSLOCTEXT("AFLLanding", "EmailLabel", "EMAIL"), false));
	UHorizontalBox* EmailRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EmailRow"));
	if (UVerticalBoxSlot* VS = Doors->AddChildToVerticalBox(EmailRow)) { VS->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
	EmailBox = MakeField(TEXT("EmailBox"), NSLOCTEXT("AFLLanding", "EmailHint", "you@example.com"), Body, 14.f);
	EmailBox->OnTextCommitted.AddDynamic(this, &UAFLW_Landing::HandleEmailCommitted);
	if (UHorizontalBoxSlot* HS = EmailRow->AddChildToHorizontalBox(EmailBox)) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HS->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f)); }
	SendCodeButton = Button(TEXT("SendCode"), NSLOCTEXT("AFLLanding", "SendCode", "SEND CODE"), Accent, FLinearColor::White);
	SendCodeButton->OnClicked.AddDynamic(this, &UAFLW_Landing::HandleSendCode);
	EmailRow->AddChildToHorizontalBox(SendCodeButton);
	if (UVerticalBoxSlot* VS = Doors->AddChildToVerticalBox(Label(TEXT("EmailNote"), Body, 11.f, Faint,
		NSLOCTEXT("AFLLanding", "EmailNote", "We email a 6-digit code. Type it here — no password, no browser."))))
	{ VS->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }

	OrRow = Label(TEXT("Or"), Data, 11.f, Faint, NSLOCTEXT("AFLLanding", "Or", "———————   OR   ———————"), false);
	Cast<UTextBlock>(OrRow)->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* VS = Doors->AddChildToVerticalBox(OrRow)) { VS->SetPadding(FMargin(0.f, 12.f, 0.f, 12.f)); }

	UHorizontalBox* DoorRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DoorRow"));
	Doors->AddChildToVerticalBox(DoorRow);
	EpicButton = Button(TEXT("EpicBtn"), bLinkMode ? NSLOCTEXT("AFLLanding", "LinkEpic", "LINK EPIC ACCOUNT") : NSLOCTEXT("AFLLanding", "Epic", "SIGN IN WITH EPIC"), AccentFill, AccentInk);
	EpicButton->OnClicked.AddDynamic(this, &UAFLW_Landing::HandleEpicClicked);
	if (UHorizontalBoxSlot* HS = DoorRow->AddChildToHorizontalBox(EpicButton)) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HS->SetPadding(FMargin(0.f, 0.f, bLinkMode ? 0.f : 4.f, 0.f)); }
	PlayNowButton = Button(TEXT("PlayNow"), NSLOCTEXT("AFLLanding", "PlayNow", "PLAY NOW"), FLinearColor(1.f, 1.f, 1.f, 0.06f), FLinearColor::White);
	PlayNowButton->OnClicked.AddDynamic(this, &UAFLW_Landing::HandlePlayNow);
	if (UHorizontalBoxSlot* HS = DoorRow->AddChildToHorizontalBox(PlayNowButton)) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HS->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f)); }
	if (bLinkMode) { PlayNowButton->SetVisibility(ESlateVisibility::Collapsed); }
	{
		UHorizontalBox* Caps = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Captions"));
		if (UVerticalBoxSlot* VS = Doors->AddChildToVerticalBox(Caps)) { VS->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
		UTextBlock* C1 = Label(TEXT("Cap1"), Body, 11.f, Faint, bLinkMode
			? NSLOCTEXT("AFLLanding", "LinkEpicCap", "Already used by another player? It is refused, never moved.")
			: NSLOCTEXT("AFLLanding", "EpicCap", "Same Epic account as the site."));
		if (UHorizontalBoxSlot* HS = Caps->AddChildToHorizontalBox(C1)) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HS->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f)); }
		if (!bLinkMode)
		{
			UTextBlock* C2 = Label(TEXT("Cap2"), Body, 11.f, Faint, NSLOCTEXT("AFLLanding", "GuestCap", "Guest. Progress stays on this PC until you link."));
			if (UHorizontalBoxSlot* HS = Caps->AddChildToHorizontalBox(C2)) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HS->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f)); }
		}
	}

	// STAY SIGNED IN (the accent row of the shipped Landing; 30 px hit floor)
	{
		UButton* Stay = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StayRow"));
		Stay->SetBackgroundColor(Accent);
		Stay->OnClicked.AddDynamic(this, &UAFLW_Landing::HandleStayToggled);
		StayRow = Stay;
		if (UVerticalBoxSlot* VS = Doors->AddChildToVerticalBox(Stay)) { VS->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f)); }
		UHorizontalBox* Inner = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Stay->AddChild(Inner);
		if (UButtonSlot* BS = Cast<UButtonSlot>(Inner->Slot)) { BS->SetPadding(FMargin(10.f, 6.f)); } // 30 px hit floor
		StayCheck = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StayCheck"));
		StayCheck->SetPadding(FMargin(0.f));
		UTextBlock* Tick = Label(TEXT("Tick"), Body, 13.f, FLinearColor::White, NSLOCTEXT("AFLLanding", "Tick", "✓"), false);
		StayCheck->SetContent(Tick);
		if (UHorizontalBoxSlot* HS = Inner->AddChildToHorizontalBox(StayCheck)) { HS->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f)); }
		Inner->AddChildToHorizontalBox(Label(TEXT("StayLabel"), Body, 13.f, FLinearColor::White, NSLOCTEXT("AFLLanding", "Stay", "Stay signed in on this device"), false));
		StayNoteText = Label(TEXT("StayNote"), Body, 11.f, Faint, NSLOCTEXT("AFLLanding", "StayNote", "Persistent sign-in — one click next time. Nothing to remember; sign-out clears it."));
		if (UVerticalBoxSlot* VS = Doors->AddChildToVerticalBox(StayNoteText)) { VS->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }
		if (bLinkMode) { Stay->SetVisibility(ESlateVisibility::Collapsed); StayNoteText->SetVisibility(ESlateVisibility::Collapsed); }
	}

	// NEW? note (the welcome grant, approved members only)
	{
		UBorder* Note = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Recruit"));
		Note->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.05f));
		Note->SetPadding(FMargin(10.f, 8.f));
		Note->SetContent(Label(TEXT("RecruitText"), Body, 12.f, FLinearColor(1.f, 1.f, 1.f, 0.72f), bLinkMode
			? NSLOCTEXT("AFLLanding", "LinkNote", "Already have an IRONICS account on the site? Sign out and sign in to it instead — guest progress stays on this PC.")
			: NSLOCTEXT("AFLLanding", "Recruit", "NEW? Your first sign-in creates your IRONICS account. Approved beta members receive 3 Weapon Credits.")));
		RecruitNote = Note;
		if (UVerticalBoxSlot* VS = Doors->AddChildToVerticalBox(Note)) { VS->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f)); }
	}

	// ---- STEP 1: the code -----------------------------------------------------------------------
	UVerticalBox* CodeStep = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CodeStep"));
	Steps->AddChild(CodeStep);
	CodeIntroText = Label(TEXT("CodeIntro"), Body, 13.f, FLinearColor(1.f, 1.f, 1.f, 0.72f), FText::GetEmpty());
	CodeStep->AddChildToVerticalBox(CodeIntroText);
	CodeBox = MakeField(TEXT("CodeBox"), NSLOCTEXT("AFLLanding", "CodeHint", "6-digit code"), Data, 26.f);
	CodeBox->OnTextChanged.AddDynamic(this, &UAFLW_Landing::HandleCodeChanged);
	CodeBox->OnTextCommitted.AddDynamic(this, &UAFLW_Landing::HandleCodeCommitted);
	if (UVerticalBoxSlot* VS = CodeStep->AddChildToVerticalBox(CodeBox)) { VS->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f)); }
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ResendRow"));
		if (UVerticalBoxSlot* VS = CodeStep->AddChildToVerticalBox(Row)) { VS->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f)); }
		ResendText = Label(TEXT("ResendText"), Body, 12.f, Dim, FText::GetEmpty());
		if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(ResendText)) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HS->SetVerticalAlignment(VAlign_Center); }
		ResendButton = Button(TEXT("Resend"), NSLOCTEXT("AFLLanding", "Resend", "RESEND"), FLinearColor(0.f, 0.f, 0.f, 0.f), AccentInk, 30.f);
		ResendButton->OnClicked.AddDynamic(this, &UAFLW_Landing::HandleResend);
		Row->AddChildToHorizontalBox(ResendButton);
		UButton* Change = Button(TEXT("ChangeEmail"), NSLOCTEXT("AFLLanding", "ChangeEmail", "CHANGE EMAIL"), FLinearColor(0.f, 0.f, 0.f, 0.f), AccentInk, 30.f);
		Change->OnClicked.AddDynamic(this, &UAFLW_Landing::HandleChangeEmail);
		Row->AddChildToHorizontalBox(Change);
	}
	ContinueButton = Button(TEXT("Continue"), NSLOCTEXT("AFLLanding", "Continue", "CONTINUE"), Accent, FLinearColor::White);
	ContinueButton->OnClicked.AddDynamic(this, &UAFLW_Landing::HandleContinue);
	if (UVerticalBoxSlot* VS = CodeStep->AddChildToVerticalBox(ContinueButton)) { VS->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f)); }
	if (UVerticalBoxSlot* VS = CodeStep->AddChildToVerticalBox(Label(TEXT("CodeNote"), Body, 11.f, Faint,
		NSLOCTEXT("AFLLanding", "CodeNote", "Paste works too. Continue unlocks at six digits. Esc goes back.")))) { VS->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f)); }

	// Status (every state names its reason) -- shared by both steps
	StatusText = Label(TEXT("Status"), Body, 12.f, Dim, FText::GetEmpty());
	if (UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(StatusText)) { VS->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f)); }

	// Footer: ESC hint + version stamp (root only)
	FooterText = Label(TEXT("Footer"), Data, 11.f, Dim, FText::GetEmpty(), false);
	FooterText->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(FooterText))
	{
		S->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		S->SetAlignment(FVector2D(0.f, 1.f));
		S->SetOffsets(FMargin(0.f, -36.f, 0.f, 18.f));
	}
	{
		FString Version;
		if (GConfig) { GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni); }
		FooterText->SetText(FText::FromString(bLinkMode ? TEXT("ESC · BACK") : FString::Printf(TEXT("ESC · SYSTEM MENU     ·     %s"), *Version)));
	}

	return Super::RebuildWidget();
}

void UAFLW_Landing::NativeOnActivated()
{
	Super::NativeOnActivated();
	using namespace AFLLanding;

	bool bSaved = true;
	if (GConfig && GConfig->GetBool(PrefsSection, TEXT("bStaySignedIn"), bSaved, GGameUserSettingsIni)) { bStaySignedIn = bSaved; }
	ApplyStayVisual();

	// POOLED INSTANCE: re-arm every per-visit state here, never trust the constructor.
	bSignInInFlight = false;
	bRouteChoicePushed = false;
	ChallengeId.Reset();
	PendingEmail.Reset();
	ResendSecondsLeft = 0;
	if (CodeBox) { CodeBox->SetText(FText::GetEmpty()); }
	ShowStep(0);
	SetStatus(FText::GetEmpty());

	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (Online)
	{
		Online->SetStaySignedIn(bStaySignedIn);
		Online->OnLoggedIn.RemoveAll(this);
		Online->OnPortalLinkResult.RemoveAll(this);
		if (bLinkMode)
		{
			Online->OnPortalLinkResult.AddUObject(this, &UAFLW_Landing::HandlePortalLink);
		}
		else
		{
			Online->OnLoggedIn.AddUObject(this, &UAFLW_Landing::HandleOnlineLoggedIn);
		}
	}

	if (bLinkMode)
	{
		return; // the card only; the world behind it (System Menu, Outpost) keeps running
	}

	KickLocalPlayInit();
	StartVideoGround();

	if (Online && Online->IsLoggedIn() && bStaySignedIn)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: already signed in (remembered) -> route choice."));
		PushRouteChoice();
		return;
	}
	// STAY SIGNED IN: a sealed portal refresh token signs the player in silently; the card shows only if it cannot.
	if (Online && bStaySignedIn && Online->HasStoredGameSession())
	{
		bSignInInFlight = true;
		SetStatus(NSLOCTEXT("AFLLanding", "Resuming", "Signing you in…"));
		Online->TryResumeGameSession([WeakThis = TWeakObjectPtr<UAFLW_Landing>(this)](bool bResuming)
		{
			if (UAFLW_Landing* Self = WeakThis.Get())
			{
				if (bResuming) { Self->ArmSignInWaiter(); }
				else { Self->bSignInInFlight = false; Self->SetStatus(FText::GetEmpty()); }
			}
		});
	}
#if !UE_BUILD_SHIPPING
	else
	{
		SetStatus(NSLOCTEXT("AFLLanding", "DevHint", "DEV: EPIC uses the dev identity in PIE; EMAIL and PLAY NOW talk to api.ironics.org."));
	}
#endif
}

void UAFLW_Landing::NativeOnDeactivated()
{
	if (UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
	{
		Online->OnLoggedIn.RemoveAll(this);
		Online->OnPortalLinkResult.RemoveAll(this);
	}
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(ResendTimer); }
	Super::NativeOnDeactivated();
}

UWidget* UAFLW_Landing::NativeGetDesiredFocusTarget() const
{
	if (Steps && Steps->GetActiveWidgetIndex() == 1 && CodeBox) { return CodeBox; }
	return EmailBox ? EmailBox : Super::NativeGetDesiredFocusTarget();
}

TOptional<FUIInputConfig> UAFLW_Landing::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

bool UAFLW_Landing::NativeOnHandleBackAction()
{
	if (Steps && Steps->GetActiveWidgetIndex() == 1)
	{
		HandleChangeEmail(); // code step: BACK to the doors
		return true;
	}
	if (bLinkMode)
	{
		DeactivateWidget(); // the link card closes; the System Menu underneath is untouched
		return true;
	}
	// Root screen: nothing behind us -- Esc opens the System Menu (QUIT TO DESKTOP lives there).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAFLSystemMenuSubsystem* Menu = GI->GetSubsystem<UAFLSystemMenuSubsystem>()) { Menu->OpenSystemMenu(); }
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------
// Doors
// ---------------------------------------------------------------------------------------------------------

void UAFLW_Landing::HandleEmailCommitted(const FText&, ETextCommit::Type Method)
{
	if (Method == ETextCommit::OnEnter) { HandleSendCode(); }
}

void UAFLW_Landing::HandleSendCode()
{
	using namespace AFLLanding;
	if (bSignInInFlight) { return; }
	const FString Email = EmailBox ? EmailBox->GetText().ToString().TrimStartAndEnd() : FString();
	int32 At;
	if (!Email.FindChar(TEXT('@'), At) || At < 1 || At >= Email.Len() - 1)
	{
		SetStatus(NSLOCTEXT("AFLLanding", "EmailBad", "Type the email you want on your IRONICS account."), true);
		return;
	}
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online) { SetStatus(NSLOCTEXT("AFLLanding", "NoOnline", "Sign-in is not available in this build."), true); return; }
	if (GConfig) { GConfig->SetBool(PrefsSection, TEXT("bStaySignedIn"), bStaySignedIn, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
	Online->SetStaySignedIn(bStaySignedIn);
	SetStatus(NSLOCTEXT("AFLLanding", "Sending", "Sending your code…"));
	Online->RequestEmailCode(Email, [WeakThis = TWeakObjectPtr<UAFLW_Landing>(this), Email](bool bOk, const FString& NewChallengeId, const FString& Reason)
	{
		UAFLW_Landing* Self = WeakThis.Get();
		if (!Self) return;
		if (!bOk)
		{
			Self->SetStatus(FText::FromString(Reason), true);
			return;
		}
		Self->ChallengeId = NewChallengeId;
		Self->PendingEmail = Email;
		if (Self->CodeBox) { Self->CodeBox->SetText(FText::GetEmpty()); }
		if (Self->CodeIntroText)
		{
			Self->CodeIntroText->SetText(FText::Format(NSLOCTEXT("AFLLanding", "CodeIntro", "We sent a 6-digit code to {0}. It expires in 10 minutes."), FText::FromString(AFLLanding::MaskEmail(Email))));
		}
		Self->ResendSecondsLeft = AFLLanding::ResendCooldownSeconds;
		Self->TickResend();
		if (UWorld* World = Self->GetWorld())
		{
			World->GetTimerManager().SetTimer(Self->ResendTimer, Self, &UAFLW_Landing::TickResend, 1.0f, true);
		}
		Self->SetStatus(FText::GetEmpty());
		Self->ShowStep(1);
		if (Self->CodeBox) { Self->CodeBox->SetKeyboardFocus(); }
	});
}

void UAFLW_Landing::TickResend()
{
	if (ResendSecondsLeft > 0) { ResendSecondsLeft--; }
	if (ResendText)
	{
		ResendText->SetText(ResendSecondsLeft > 0
			? FText::Format(NSLOCTEXT("AFLLanding", "ResendIn", "Didn't get it? Check spam, or resend in 0:{0}"), FText::FromString(FString::Printf(TEXT("%02d"), ResendSecondsLeft)))
			: NSLOCTEXT("AFLLanding", "ResendNow", "Didn't get it? Check spam, or"));
	}
	if (ResendButton) { ResendButton->SetIsEnabled(ResendSecondsLeft <= 0); }
	if (ResendSecondsLeft <= 0)
	{
		if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(ResendTimer); }
	}
}

void UAFLW_Landing::HandleResend()
{
	if (ResendSecondsLeft > 0 || PendingEmail.IsEmpty()) { return; }
	if (EmailBox) { EmailBox->SetText(FText::FromString(PendingEmail)); }
	HandleSendCode();
}

void UAFLW_Landing::HandleChangeEmail()
{
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(ResendTimer); }
	ChallengeId.Reset();
	if (CodeBox) { CodeBox->SetText(FText::GetEmpty()); }
	SetStatus(FText::GetEmpty());
	ShowStep(0);
	if (EmailBox) { EmailBox->SetKeyboardFocus(); }
}

void UAFLW_Landing::HandleCodeChanged(const FText& Text)
{
	const FString Clean = AFLLanding::DigitsOnly(Text.ToString(), 6);
	if (Clean != Text.ToString() && CodeBox) { CodeBox->SetText(FText::FromString(Clean)); }
	if (ContinueButton) { ContinueButton->SetIsEnabled(Clean.Len() == 6); }
}

void UAFLW_Landing::HandleCodeCommitted(const FText&, ETextCommit::Type Method)
{
	if (Method == ETextCommit::OnEnter) { HandleContinue(); }
}

void UAFLW_Landing::HandleContinue()
{
	if (bSignInInFlight) { return; }
	const FString Code = CodeBox ? AFLLanding::DigitsOnly(CodeBox->GetText().ToString(), 6) : FString();
	if (Code.Len() != 6 || ChallengeId.IsEmpty())
	{
		SetStatus(NSLOCTEXT("AFLLanding", "CodeShort", "Type all six digits from the email."), true);
		return;
	}
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online) { return; }
	SetStatus(bLinkMode ? NSLOCTEXT("AFLLanding", "Linking", "Linking…") : NSLOCTEXT("AFLLanding", "SigningIn", "Signing in…"));
	// The DOOR first (it marks the login in flight), THEN the waiter: CallWhenLoggedIn kicks the default login
	// (Epic in a shipped build) when nothing is in flight, which would turn the email door into an Epic sign-in.
	if (!bLinkMode) { bSignInInFlight = true; }
	Online->VerifyEmailCode(ChallengeId, Code, bLinkMode);
	if (!bLinkMode) { ArmSignInWaiter(); }
}

void UAFLW_Landing::HandleEpicClicked()
{
	using namespace AFLLanding;
	if (bSignInInFlight) { return; }
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online) { HandleLoggedIn(false); return; }
	if (GConfig) { GConfig->SetBool(PrefsSection, TEXT("bStaySignedIn"), bStaySignedIn, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
	Online->SetStaySignedIn(bStaySignedIn);
	if (bLinkMode)
	{
		SetStatus(NSLOCTEXT("AFLLanding", "LinkingEpic", "Finish the Epic sign-in in the browser…"));
		Online->LinkEpicToCurrent();
		return;
	}
	bSignInInFlight = true;
	SetStatus(NSLOCTEXT("AFLLanding", "SigningIn", "Signing in…"));
	Online->EnsureLogin(); // dev = CustomID; shipping = EOS OIDC (PersistentAuth honours the saved preference)
	ArmSignInWaiter();
}

void UAFLW_Landing::HandlePlayNow()
{
	if (bSignInInFlight) { return; }
	UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
	if (!Online) { HandleLoggedIn(false); return; }
	Online->SetStaySignedIn(bStaySignedIn);
	bSignInInFlight = true;
	SetStatus(NSLOCTEXT("AFLLanding", "GuestIn", "Setting up your guest…"));
	Online->GuestLogin();
	ArmSignInWaiter();
}

void UAFLW_Landing::ArmSignInWaiter()
{
	// The waiter is the FAILURE channel (a refusal resolves it with false promptly). Success also arrives by
	// OnLoggedIn, so the timeout is not the deadline for a real login -- a browser sign-in can run past 2 min.
	if (UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
	{
		Online->CallWhenLoggedIn([WeakThis = TWeakObjectPtr<UAFLW_Landing>(this)](bool bOk)
		{
			if (UAFLW_Landing* Self = WeakThis.Get()) { Self->HandleLoggedIn(bOk); }
		}, 180.0f);
	}
}

void UAFLW_Landing::HandleStayToggled()
{
	bStaySignedIn = !bStaySignedIn;
	ApplyStayVisual();
	if (UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this)) { Online->SetStaySignedIn(bStaySignedIn); }
}

void UAFLW_Landing::ApplyStayVisual()
{
	using namespace AFLLanding;
	if (UButton* Row = Cast<UButton>(StayRow)) { Row->SetBackgroundColor(bStaySignedIn ? Accent : FLinearColor(1.f, 1.f, 1.f, 0.08f)); }
	if (StayCheck) { StayCheck->SetVisibility(bStaySignedIn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden); }
}

void UAFLW_Landing::ShowStep(int32 Index)
{
	if (Steps) { Steps->SetActiveWidgetIndex(Index); }
}

void UAFLW_Landing::SetStatus(const FText& Text, bool bBad)
{
	using namespace AFLLanding;
	if (!StatusText) return;
	StatusText->SetText(Text);
	StatusText->SetColorAndOpacity(FSlateColor(bBad ? Bad : Dim));
	StatusText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

// ---------------------------------------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------------------------------------

void UAFLW_Landing::HandleOnlineLoggedIn()
{
	if (!IsActivated() || bRouteChoicePushed || bLinkMode) { return; }
	if (!bStaySignedIn && !bSignInInFlight)
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: login landed (stay-signed-in off) -> waiting for a click."));
		return;
	}
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: login landed (OnLoggedIn) -> route choice."));
	HandleLoggedIn(true);
}

void UAFLW_Landing::HandlePortalLink(bool bOk, const FString& Reason)
{
	if (!IsActivated()) { return; }
	if (!bOk)
	{
		SetStatus(FText::FromString(Reason), true);
		return;
	}
	SetStatus(NSLOCTEXT("AFLLanding", "Linked", "Linked. Your progress now follows you — and this is your IRONICS account on the site too."));
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: LINK ACCOUNT succeeded."));
	if (UWorld* World = GetWorld())
	{
		FTimerHandle Close;
		World->GetTimerManager().SetTimer(Close, [WeakThis = TWeakObjectPtr<UAFLW_Landing>(this)]()
		{
			if (UAFLW_Landing* Self = WeakThis.Get()) { Self->DeactivateWidget(); }
		}, 2.2f, false);
	}
}

void UAFLW_Landing::HandleLoggedIn(bool bSuccess)
{
	if (!bSuccess)
	{
		UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this);
		const bool bStillInFlight = Online && Online->IsLoginInFlight();
		if (bStillInFlight)
		{
			SetStatus(NSLOCTEXT("AFLLanding", "WaitingBrowser", "Waiting for your Epic sign-in to finish in the browser… Finished there? This screen continues on its own. Esc opens the system menu."));
		}
		else
		{
			const FString Why = Online ? Online->GetLastLoginFailure() : FString();
			SetStatus(Why.IsEmpty()
				? NSLOCTEXT("AFLLanding", "Failed", "Sign-in failed — check the connection and try again.")
				: FText::Format(NSLOCTEXT("AFLLanding", "FailedWhy", "Sign-in failed — {0}. Esc opens the system menu."), FText::FromString(Why)), true);
		}
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: sign-in waiter resolved false (%s)."), bStillInFlight ? TEXT("login still in flight") : TEXT("login FAILED"));
		bSignInInFlight = false;
		return;
	}
	bSignInInFlight = false;
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(ResendTimer); }
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_LANDING: signed in -> route choice."));
	PushRouteChoice();
}

// ---------------------------------------------------------------------------------------------------------
// Boot plumbing (unchanged from the shipped Landing)
// ---------------------------------------------------------------------------------------------------------

void UAFLW_Landing::KickLocalPlayInit()
{
	// The press-start slot normally triggers local-play init; forced-on PC the landing owns it. Login errors
	// SUPPRESSED: the boot CanPlay login runs EOS AutoLogin -> ShowLoginUI, an engine stub on Windows; the real
	// account sign-in is the AFLOnlineSubsystem flow on this card (see the 2026-09 first-launch dead-button fix).
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UCommonUserSubsystem* Users = GI->GetSubsystem<UCommonUserSubsystem>())
		{
			FCommonUserInitializeParams Params;
			Params.LocalPlayerIndex = 0;
			Params.bCanCreateNewLocalPlayer = true;
			Params.RequestedPrivilege = ECommonUserPrivilege::CanPlay;
			Params.bSuppressLoginErrors = true;
			Users->TryToInitializeUser(Params);
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

void UAFLW_Landing::PushRouteChoice()
{
	if (bRouteChoicePushed) { return; }
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP) { return; }
	bRouteChoicePushed = true;
	// MODAL layer, not Menu (2026-09-02 route-skip fix): the Landing stays ACTIVE underneath until a door is chosen.
	UCommonActivatableWidget* Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
		FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Modal")),
		TSubclassOf<UCommonActivatableWidget>(UAFLW_RouteChoice::StaticClass()));
	if (UAFLW_RouteChoice* Choice = Cast<UAFLW_RouteChoice>(Pushed))
	{
		Choice->OnRouteChosen.RemoveAll(this); // POOLED: rebind, don't accumulate
		Choice->OnRouteChosen.AddWeakLambda(this, [this](bool /*bMatchmaking*/) { DeactivateWidget(); });
	}
}
