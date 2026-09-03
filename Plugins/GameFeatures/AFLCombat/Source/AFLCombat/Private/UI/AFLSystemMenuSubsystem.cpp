// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLSystemMenuSubsystem.h"

#include "AFLCombat.h"
#include "Engine/Engine.h"                 // GEngine->GameViewport
#include "Engine/GameViewportClient.h"     // GetGameViewportWidget()
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/CoreMisc.h"                 // IsRunningDedicatedServer()
#include "UI/AFLW_SystemMenu.h"
#include "Widgets/SViewport.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLSystemMenuSubsystem)

namespace
{
	// GAMEPLAY vs MENU (operator ruling 2026-09-02: Esc keeps meaning "back" inside menus; it only summons
	// the System Menu from gameplay). In gameplay the game VIEWPORT widget itself holds focus; when any
	// CommonUI menu is up, a specific UI widget (a button) holds focus instead. So "the viewport is the
	// focused widget, or nothing is" == gameplay.
	bool IsGameplayContext()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}
		const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
		if (!Focused.IsValid())
		{
			return true; // nothing focused -> treat as gameplay
		}
		if (GEngine && GEngine->GameViewport)
		{
			const TSharedPtr<SViewport> ViewportWidget = GEngine->GameViewport->GetGameViewportWidget();
			if (ViewportWidget.IsValid() && Focused.Get() == ViewportWidget.Get())
			{
				return true; // the game viewport itself has focus -> gameplay, no menu is up
			}
		}
		return false; // a specific UI widget has focus -> a menu is open, leave Escape to it (== back)
	}
}

/**
 * The Slate pre-processor. Runs before widget/gameplay input, so it works even when a menu is focused or
 * the player is stuck in-world. Holds a weak ref to its owning subsystem; the subsystem outlives it
 * (it registers/unregisters this).
 */
class FAFLEscapeInputProcessor : public IInputProcessor
{
public:
	explicit FAFLEscapeInputProcessor(UAFLSystemMenuSubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float /*DeltaTime*/, FSlateApplication& /*SlateApp*/, TSharedRef<ICursor> /*Cursor*/) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& /*SlateApp*/, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape && !InKeyEvent.IsRepeat())
		{
			if (UAFLSystemMenuSubsystem* O = Owner.Get())
			{
				// Only from GAMEPLAY, and only when a System Menu is not already open. Inside menus we do
				// NOT consume Escape -> it keeps its normal "back" meaning (CommonUI routes it to the
				// focused widget). The System Menu, once open, is itself a focused menu, so its own back
				// handler closes it via that same path.
				if (O->ShouldOpenOnEscape() && IsGameplayContext())
				{
					O->OpenSystemMenu();
					return true; // consume: Escape opened the menu from gameplay
				}
			}
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("AFLEscapeSystemMenu"); }

private:
	TWeakObjectPtr<UAFLSystemMenuSubsystem> Owner;
};

void UAFLSystemMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Client-only: a dedicated server has no Slate application and no player to open a menu for.
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (FSlateApplication::IsInitialized())
	{
		EscapeProcessor = MakeShared<FAFLEscapeInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(EscapeProcessor);
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_SYSMENU: global Escape handler registered -- Esc opens the System Menu anywhere."));
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_SYSMENU: Slate not initialized -- global Escape handler NOT registered."));
	}
}

void UAFLSystemMenuSubsystem::Deinitialize()
{
	if (EscapeProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(EscapeProcessor);
	}
	EscapeProcessor.Reset();
	Super::Deinitialize();
}

void UAFLSystemMenuSubsystem::OpenSystemMenu()
{
	if (OpenMenu.IsValid())
	{
		return; // one at a time
	}
	OpenMenu = UAFLW_SystemMenu::Open(this);
}
