// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLSystemMenuSubsystem.h"

#include "AFLCombat.h"
#include "Engine/Engine.h"                 // GEngine->GameViewport
#include "Engine/GameViewportClient.h"     // GetGameViewportWidget()
#include "Engine/World.h"                  // GetMapName / GetTimerManager (hint)
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/CoreMisc.h"                 // IsRunningDedicatedServer()
#include "Styling/CoreStyle.h"             // GetDefaultFontStyle (hint)
#include "TimerManager.h"                  // hint auto-dismiss timer
#include "UI/AFLW_Chat.h"                  // Enter opens text chat (COMMS wiring)
#include "UI/AFLW_SystemMenu.h"
#include "UObject/UObjectGlobals.h"        // FCoreUObjectDelegates::PostLoadMapWithWorld
#include "Widgets/Layout/SBox.h"           // hint layout
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"       // hint text

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
 * The Slate pre-processor for the global gameplay hotkeys. Runs before widget/gameplay input, so it works
 * even when a menu is focused or the player is stuck in-world. Handles Escape (open the System Menu) and
 * Enter (open text chat) -- both ONLY from gameplay, yielding when a CommonUI widget owns focus. Holds a
 * weak ref to its owning subsystem; the subsystem outlives it (it registers/unregisters this).
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
		if (InKeyEvent.GetKey() == EKeys::Enter && !InKeyEvent.IsRepeat())
		{
			if (UAFLSystemMenuSubsystem* O = Owner.Get())
			{
				// Enter opens text chat, but ONLY from gameplay. When chat (or any menu) is focused,
				// IsGameplayContext() is false, so Enter is not consumed here -> it reaches the focused
				// widget and the chat compose box sends the message. One key both opens and sends.
				if (IsGameplayContext())
				{
					UAFLW_Chat::Open(O);
					return true; // consume: Enter opened chat from gameplay
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

	// Discoverability: a brief "Press ESC for the menu" hint on entering a gameplay map. The menu itself
	// works (first live-lap log-proven: AFL_SYSMENU registered + opened), but nothing on-screen told the
	// player Esc opens it -- the "top-bar account chip" entry point is deferred WBP polish.
	MapLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UAFLSystemMenuSubsystem::HandlePostLoadMap);
}

void UAFLSystemMenuSubsystem::Deinitialize()
{
	if (EscapeProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(EscapeProcessor);
	}
	EscapeProcessor.Reset();
	if (MapLoadHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadHandle);
		MapLoadHandle.Reset();
	}
	RemoveMenuHint();
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

void UAFLSystemMenuSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (IsRunningDedicatedServer() || !LoadedWorld)
	{
		return;
	}
	// The front-end (Armory) uses Esc as "back", so the menu hint only belongs on gameplay maps.
	if (LoadedWorld->GetMapName().Contains(TEXT("Armory")))
	{
		RemoveMenuHint();
		return;
	}
	ShowMenuHint(LoadedWorld);
}

void UAFLSystemMenuSubsystem::ShowMenuHint(UWorld* World)
{
	if (!GEngine || !GEngine->GameViewport || !World)
	{
		return;
	}
	RemoveMenuHint();

	// Transient (not persistent) on purpose: a corner chip risks colliding with the health/weapon HUD, so
	// a top-centre hint that auto-dismisses teaches the control without permanently occupying the screen.
	// Text-only with a shadow -- no SBorder dependency; readable over the bright arena ground.
	HintWidget = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 30.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AFLSysMenu", "MenuHint", "Press  ESC  for the menu  (wallet, sign out, quit)      Press  ENTER  to chat"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.86f, 0.90f, 0.98f, 0.95f)))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			.ShadowOffset(FVector2D(1.f, 1.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f))
		];
	GEngine->GameViewport->AddViewportWidgetContent(HintWidget.ToSharedRef(), /*ZOrder*/ 5);

	// Auto-dismiss after a few seconds -- long enough to read once on entry, gone before it becomes clutter.
	World->GetTimerManager().SetTimer(HintTimerHandle,
		FTimerDelegate::CreateUObject(this, &UAFLSystemMenuSubsystem::RemoveMenuHint), 6.0f, /*bLoop*/ false);
}

void UAFLSystemMenuSubsystem::RemoveMenuHint()
{
	if (HintWidget.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(HintWidget.ToSharedRef());
	}
	HintWidget.Reset();
}
