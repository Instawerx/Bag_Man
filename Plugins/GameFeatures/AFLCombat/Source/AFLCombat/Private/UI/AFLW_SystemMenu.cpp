// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_SystemMenu.h"

#include "AFLCombat.h"
#include "AFLOnlineSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "CommonUIExtensions.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"      // FUIInputConfig / ECommonInputMode
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_SystemMenu)

namespace AFLSysMenu
{
	static const FLinearColor Dim(0.024f, 0.035f, 0.070f, 0.62f);       // pause scrim over the front-end
	static const FLinearColor Surface(0.0028f, 0.0043f, 0.0231f, 1.f);  // #0E122B card
	static const FLinearColor RowNeutral(1.f, 1.f, 1.f, 0.025f);        // resume/settings fill
	static const FLinearColor Accent(0.013f, 0.102f, 1.0f, 1.0f);       // #1E5AFF
	static const FLinearColor AccentFill(0.013f, 0.102f, 1.0f, 0.18f);  // sign-out fill (focused)
	static const FLinearColor Danger(0.49f, 0.066f, 0.066f, 1.f);       // muted red (quit)
	static const FLinearColor DangerText(0.886f, 0.604f, 0.604f, 1.f);  // #E29A9A
	static const FLinearColor TextMuted(0.55f, 0.60f, 0.72f, 1.f);

	// The front-end boot map -- signing out returns here so the Landing re-shows the Epic sign-in.
	static const TCHAR* FrontEndMap = TEXT("L_IRONICS_Armory");

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

UAFLW_SystemMenu::UAFLW_SystemMenu()
{
	// Escape / gamepad-B routes to NativeOnHandleBackAction (confirm -> menu, menu -> close).
	bIsBackHandler = true;
}

UAFLW_SystemMenu* UAFLW_SystemMenu::Open(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	ULocalPlayer* LP = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	if (!LP)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_SYSMENU: Open failed -- no local player."));
		return nullptr;
	}
	UCommonActivatableWidget* Pushed = UCommonUIExtensions::PushContentToLayer_ForPlayer(LP,
		FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Modal")),
		TSubclassOf<UCommonActivatableWidget>(UAFLW_SystemMenu::StaticClass()));
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_SYSMENU: opened on UI.Layer.Modal."));
	return Cast<UAFLW_SystemMenu>(Pushed);
}

TSharedRef<SWidget> UAFLW_SystemMenu::RebuildWidget()
{
	using namespace AFLSysMenu;
	UFont* Display = Orbitron();
	UFont* Body = NotoSans();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SysMenuCanvas"));
	WidgetTree->RootWidget = Canvas;

	// Dim scrim.
	UBorder* Scrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Scrim"));
	Scrim->SetBrushColor(Dim);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Scrim))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

	// ===== MENU PANEL =====
	UBorder* Menu = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuCard"));
	Menu->SetBrushColor(Surface);
	Menu->SetPadding(FMargin(24.f, 24.f, 24.f, 18.f));
	MenuPanel = Menu;
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Menu))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(FVector2D(468.f, 480.f));
	}
	UVerticalBox* MenuCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Menu->AddChild(MenuCol);

	// Header: identity line.
	UTextBlock* Ident = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(Ident, Display, 20.f, FLinearColor::White);
	Ident->SetText(NSLOCTEXT("AFLSysMenu", "Ident", "SYSTEM MENU"));
	if (UVerticalBoxSlot* VS = MenuCol->AddChildToVerticalBox(Ident))
	{
		VS->SetPadding(FMargin(4.f, 2.f, 0.f, 2.f));
	}
	UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(Sub, Body, 12.f, TextMuted);
	Sub->SetText(NSLOCTEXT("AFLSysMenu", "Sub", "Signed in with Epic"));
	if (UVerticalBoxSlot* VS = MenuCol->AddChildToVerticalBox(Sub))
	{
		VS->SetPadding(FMargin(4.f, 0.f, 0.f, 18.f));
	}

	// Action-row builder: styled UButton + centered Orbitron label. Returns the button for OnClicked binding.
	auto MakeRow = [&](const TCHAR* Name, const FText& Label, const FLinearColor& Fill, const FLinearColor& TextCol) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Btn->SetBackgroundColor(Fill);
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Style(T, Display, 14.f, TextCol);
		T->SetJustification(ETextJustify::Center);
		T->SetText(Label);
		Btn->AddChild(T);
		if (UVerticalBoxSlot* VS = MenuCol->AddChildToVerticalBox(Btn))
		{
			VS->SetPadding(FMargin(0.f, 5.f, 0.f, 5.f));
		}
		return Btn;
	};

	UButton* Resume = MakeRow(TEXT("ResumeBtn"), NSLOCTEXT("AFLSysMenu", "Resume", "RESUME"), RowNeutral, FLinearColor(0.905f, 0.925f, 0.965f, 1.f));
	Resume->OnClicked.AddDynamic(this, &UAFLW_SystemMenu::HandleResume);

	UButton* Settings = MakeRow(TEXT("SettingsBtn"), NSLOCTEXT("AFLSysMenu", "Settings", "SETTINGS"), RowNeutral, FLinearColor(0.905f, 0.925f, 0.965f, 1.f));
	Settings->OnClicked.AddDynamic(this, &UAFLW_SystemMenu::HandleSettings);

	SignOutButton = MakeRow(TEXT("SignOutBtn"), NSLOCTEXT("AFLSysMenu", "SignOut", "SIGN OUT"), AccentFill, FLinearColor::White);
	SignOutButton->OnClicked.AddDynamic(this, &UAFLW_SystemMenu::HandleSignOut);

	UButton* Quit = MakeRow(TEXT("QuitBtn"), NSLOCTEXT("AFLSysMenu", "Quit", "QUIT TO DESKTOP"), Danger, DangerText);
	Quit->OnClicked.AddDynamic(this, &UAFLW_SystemMenu::HandleQuit);

	// Footer hint.
	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(Hint, Body, 11.f, TextMuted);
	Hint->SetText(NSLOCTEXT("AFLSysMenu", "Hint", "Esc to resume"));
	if (UVerticalBoxSlot* VS = MenuCol->AddChildToVerticalBox(Hint))
	{
		VS->SetPadding(FMargin(4.f, 16.f, 0.f, 0.f));
	}

	// ===== CONFIRM PANEL (hidden until Sign Out / Quit) =====
	UBorder* Confirm = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmCard"));
	Confirm->SetBrushColor(Surface);
	Confirm->SetPadding(FMargin(32.f, 30.f, 32.f, 26.f));
	Confirm->SetVisibility(ESlateVisibility::Collapsed);
	ConfirmPanel = Confirm;
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Confirm))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetSize(FVector2D(520.f, 300.f));
	}
	UVerticalBox* ConfirmCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Confirm->AddChild(ConfirmCol);

	ConfirmTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(ConfirmTitle, Display, 22.f, FLinearColor::White);
	ConfirmTitle->SetText(NSLOCTEXT("AFLSysMenu", "ConfirmTitle", "CONFIRM"));
	if (UVerticalBoxSlot* VS = ConfirmCol->AddChildToVerticalBox(ConfirmTitle))
	{
		VS->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
	}

	ConfirmBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(ConfirmBody, Body, 14.f, FLinearColor(0.66f, 0.71f, 0.80f, 1.f));
	ConfirmBody->SetAutoWrapText(true);
	if (UVerticalBoxSlot* VS = ConfirmCol->AddChildToVerticalBox(ConfirmBody))
	{
		VS->SetPadding(FMargin(0.f, 0.f, 0.f, 26.f));
	}

	// Cancel (ghost) -- default focus on the confirm step.
	ConfirmCancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CancelBtn"));
	ConfirmCancelButton->SetBackgroundColor(RowNeutral);
	UTextBlock* CancelLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(CancelLbl, Display, 13.f, FLinearColor(0.78f, 0.82f, 0.90f, 1.f));
	CancelLbl->SetJustification(ETextJustify::Center);
	CancelLbl->SetText(NSLOCTEXT("AFLSysMenu", "Cancel", "CANCEL"));
	ConfirmCancelButton->AddChild(CancelLbl);
	if (UVerticalBoxSlot* VS = ConfirmCol->AddChildToVerticalBox(ConfirmCancelButton))
	{
		VS->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
	ConfirmCancelButton->OnClicked.AddDynamic(this, &UAFLW_SystemMenu::HandleConfirmCancel);

	// Proceed (accent for sign-out, danger for quit -- background set per action in ShowConfirm).
	UButton* Proceed = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ProceedBtn"));
	Proceed->SetBackgroundColor(Accent);
	ConfirmProceedLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Style(ConfirmProceedLabel, Display, 13.f, FLinearColor::White);
	ConfirmProceedLabel->SetJustification(ETextJustify::Center);
	ConfirmProceedLabel->SetText(NSLOCTEXT("AFLSysMenu", "Proceed", "CONFIRM"));
	Proceed->AddChild(ConfirmProceedLabel);
	if (UVerticalBoxSlot* VS = ConfirmCol->AddChildToVerticalBox(Proceed))
	{
		VS->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
	}
	Proceed->OnClicked.AddDynamic(this, &UAFLW_SystemMenu::HandleConfirmProceed);

	return Super::RebuildWidget();
}

void UAFLW_SystemMenu::ShowMenu()
{
	Pending = EConfirm::None;
	if (MenuPanel) { MenuPanel->SetVisibility(ESlateVisibility::Visible); }
	if (ConfirmPanel) { ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed); }
}

void UAFLW_SystemMenu::ShowConfirm(EConfirm Which)
{
	Pending = Which;
	if (ConfirmTitle && ConfirmBody && ConfirmProceedLabel)
	{
		if (Which == EConfirm::SignOut)
		{
			ConfirmTitle->SetText(NSLOCTEXT("AFLSysMenu", "SignOutTitle", "SIGN OUT?"));
			ConfirmBody->SetText(NSLOCTEXT("AFLSysMenu", "SignOutBody",
				"You'll return to the IRONICS sign-in screen. \"Stay signed in on this device\" will be cleared."));
			ConfirmProceedLabel->SetText(NSLOCTEXT("AFLSysMenu", "SignOutCta", "SIGN OUT"));
		}
		else // Quit
		{
			ConfirmTitle->SetText(NSLOCTEXT("AFLSysMenu", "QuitTitle", "QUIT TO DESKTOP?"));
			ConfirmBody->SetText(NSLOCTEXT("AFLSysMenu", "QuitBody",
				"IRONICS will close. Your progress is saved to your account."));
			ConfirmProceedLabel->SetText(NSLOCTEXT("AFLSysMenu", "QuitCta", "QUIT"));
		}
	}
	if (MenuPanel) { MenuPanel->SetVisibility(ESlateVisibility::Collapsed); }
	if (ConfirmPanel) { ConfirmPanel->SetVisibility(ESlateVisibility::Visible); }
}

void UAFLW_SystemMenu::HandleResume()  { DeactivateWidget(); }

void UAFLW_SystemMenu::HandleSettings()
{
	// Placeholder pending the settings surface (the footer nav already carries a Settings destination).
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_SYSMENU: SETTINGS is a placeholder in this build -- no settings screen wired yet."));
}

void UAFLW_SystemMenu::HandleSignOut() { ShowConfirm(EConfirm::SignOut); }
void UAFLW_SystemMenu::HandleQuit()    { ShowConfirm(EConfirm::Quit); }
void UAFLW_SystemMenu::HandleConfirmCancel() { ShowMenu(); }

void UAFLW_SystemMenu::HandleConfirmProceed()
{
	if (Pending == EConfirm::SignOut) { DoSignOut(); }
	else if (Pending == EConfirm::Quit) { DoQuit(); }
}

void UAFLW_SystemMenu::DoSignOut()
{
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_SYSMENU: SIGN OUT confirmed -> dropping session and returning to sign-in."));
	if (UAFLOnlineSubsystem* Online = UAFLOnlineSubsystem::Get(this))
	{
		Online->Logout();
	}
	DeactivateWidget();
	// Return to the front-end boot map -> the Landing re-shows the Epic sign-in (login state is now cleared).
	UGameplayStatics::OpenLevel(this, FName(AFLSysMenu::FrontEndMap));
}

void UAFLW_SystemMenu::DoQuit()
{
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_SYSMENU: QUIT TO DESKTOP confirmed."));
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
}

bool UAFLW_SystemMenu::NativeOnHandleBackAction()
{
	// From the confirm step, Back returns to the menu list; from the menu, Back closes (== RESUME).
	if (Pending != EConfirm::None)
	{
		ShowMenu();
		return true;
	}
	DeactivateWidget();
	return true;
}

UWidget* UAFLW_SystemMenu::NativeGetDesiredFocusTarget() const
{
	if (Pending != EConfirm::None && ConfirmCancelButton)
	{
		return ConfirmCancelButton;
	}
	if (SignOutButton)
	{
		return SignOutButton;
	}
	return Super::NativeGetDesiredFocusTarget();
}

TOptional<FUIInputConfig> UAFLW_SystemMenu::GetDesiredInputConfig() const
{
	// Menu mode with a visible cursor -- required so the overlay is usable when opened from gameplay
	// (the global-Esc path), where no CommonUI menu was previously focused.
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}
