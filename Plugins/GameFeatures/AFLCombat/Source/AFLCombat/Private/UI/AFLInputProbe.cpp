// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLInputProbe.h"

#if !UE_BUILD_SHIPPING

#include "AFLCombat.h"                          // LogAFLCombat
#include "CommonInputSubsystem.h"               // UCommonInputSubsystem (the per-LocalPlayer input-type filter)
#include "CommonInputTypeEnum.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h" // EMouseButtons
#include "GenericPlatform/GenericWindow.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Input/CommonUIActionRouterBase.h"    // the CommonUI action router (active input mode / capture)
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
#include "LoadingScreenManager.h"               // ULoadingScreenManager (its pre-processor eats EVERYTHING while shown)
#include "Containers/Ticker.h"
#include "UI/AFLW_RouteChoice.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace
{
	/** Front of the Slate pre-processor chain: logs what the OS delivered, consumes nothing. */
	class FAFLInputProbeProcessor : public IInputProcessor
	{
	public:
		virtual void Tick(const float /*DeltaTime*/, FSlateApplication& /*SlateApp*/, TSharedRef<ICursor> /*Cursor*/) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication& /*SlateApp*/, const FKeyEvent& InKeyEvent) override
		{
			if (!InKeyEvent.IsRepeat())
			{
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: FRONT-OF-CHAIN saw KEY DOWN %s (user %d)"),
					*InKeyEvent.GetKey().ToString(), InKeyEvent.GetUserIndex());
			}
			return false;
		}

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& /*SlateApp*/, const FPointerEvent& MouseEvent) override
		{
			const FVector2D P = MouseEvent.GetScreenSpacePosition();
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: FRONT-OF-CHAIN saw MOUSE DOWN %s at (%.0f,%.0f) (user %d)"),
				*MouseEvent.GetEffectingButton().ToString(), P.X, P.Y, MouseEvent.GetUserIndex());
			return false;
		}

		virtual bool HandleMouseMoveEvent(FSlateApplication& /*SlateApp*/, const FPointerEvent& MouseEvent) override
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - LastMoveLog > 2.0)
			{
				LastMoveLog = Now;
				const FVector2D P = MouseEvent.GetScreenSpacePosition();
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: FRONT-OF-CHAIN saw MOUSE MOVE at (%.0f,%.0f) (throttled 2s)"), P.X, P.Y);
			}
			return false;
		}

	private:
		double LastMoveLog = 0.0;
	};

	TSharedPtr<FAFLInputProbeProcessor> GFrontLogger;

	const TCHAR* FilterStr(bool bBlocked) { return bBlocked ? TEXT("BLOCKED") : TEXT("open"); }

	void LogSlatePath(const TCHAR* Label, const FWidgetPath& Path)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE:   %s: %d widgets (window -> leaf)"), Label, Path.Widgets.Num());
		for (int32 i = 0; i < Path.Widgets.Num(); ++i)
		{
			const TSharedRef<SWidget>& W = Path.Widgets[i].Widget;
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE:     [%2d] %-34s vis=%-22s enabled=%d"),
				i, *W->GetTypeAsString(), *W->GetVisibility().ToString(), W->IsEnabled() ? 1 : 0);
		}
	}
}

void AFLInputProbe::EnsureFrontLogger()
{
	if (!GFrontLogger.IsValid() && FSlateApplication::IsInitialized())
	{
		GFrontLogger = MakeShared<FAFLInputProbeProcessor>();
		// Index 0 = ahead of the loading-screen eater and the CommonInput filter, so it sees what the OS delivered.
		FSlateApplication::Get().RegisterInputPreProcessor(GFrontLogger, 0);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: front-of-chain logger registered (index 0, never consumes)."));
	}
}

void AFLInputProbe::Dump(UWorld* World, const TCHAR* Why)
{
	if (!FSlateApplication::IsInitialized())
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE[%s]: Slate not initialised."), Why);
		return;
	}
	FSlateApplication& Slate = FSlateApplication::Get();
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE[%s] ================ begin ================"), Why);

	// ── Slate: app activity, capture, focus, and the hit-test chain to the focused widget ──────────
	const TSharedPtr<SWidget> Focused = Slate.GetUserFocusedWidget(0);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: slate: appActive=%d anyMouseCaptor=%d user0MouseCapture=%d focused=%s"),
		Slate.IsActive() ? 1 : 0, Slate.HasAnyMouseCaptor() ? 1 : 0, Slate.HasUserMouseCapture(0) ? 1 : 0,
		Focused.IsValid() ? *Focused->ToString() : TEXT("<none>"));
	if (Focused.IsValid())
	{
		FWidgetPath HitTestable, Any;
		const bool bHit = Slate.FindPathToWidget(Focused.ToSharedRef(), HitTestable, EVisibility::Visible);
		const bool bAny = Slate.FindPathToWidget(Focused.ToSharedRef(), Any, EVisibility::All);
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: focused widget reachable through a HIT-TESTABLE chain=%d, through any chain=%d"), bHit ? 1 : 0, bAny ? 1 : 0);
		LogSlatePath(TEXT("focused chain (any visibility)"), Any);
	}

	// ── Slate: what a click at the CURRENT cursor position would hit ────────────────────────────────
	const FVector2D Cursor = Slate.GetCursorPos();
	const FWidgetPath Under = Slate.LocateWindowUnderMouse(Cursor, Slate.GetInteractiveTopLevelWindows(), false, 0);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: cursor=(%.0f,%.0f)"), Cursor.X, Cursor.Y);
	LogSlatePath(TEXT("chain under cursor (what a click hits)"), Under);

	ULocalPlayer* LP = (GEngine && World) ? GEngine->GetFirstGamePlayer(World) : nullptr;
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: localPlayer=%s world=%s"), *GetNameSafe(LP), *GetNameSafe(World));

	// ── CommonInput: the per-LocalPlayer filter that eats key AND mouse when a suspend token leaks ──
	if (UCommonInputSubsystem* CI = UCommonInputSubsystem::Get(LP))
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: CommonInput filter: MouseAndKeyboard=%s Gamepad=%s Touch=%s | currentType=%s pointer=%d"),
			FilterStr(CI->GetInputTypeFilter(ECommonInputType::MouseAndKeyboard)),
			FilterStr(CI->GetInputTypeFilter(ECommonInputType::Gamepad)),
			FilterStr(CI->GetInputTypeFilter(ECommonInputType::Touch)),
			*StaticEnum<ECommonInputType>()->GetNameStringByValue(static_cast<int64>(CI->GetCurrentInputType())),
			CI->IsUsingPointerInput() ? 1 : 0);
	}
	else
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: CommonInput subsystem: <none>"));
	}

	// ── CommonUI action router ──────────────────────────────────────────────────────────────────────
	if (UCommonUIActionRouterBase* Router = LP ? LP->GetSubsystem<UCommonUIActionRouterBase>() : nullptr)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: router: activeInputMode=%s activeCapture=%s"),
			*StaticEnum<ECommonInputMode>()->GetNameStringByValue(static_cast<int64>(Router->GetActiveInputMode())),
			*StaticEnum<EMouseCaptureMode>()->GetNameStringByValue(static_cast<int64>(Router->GetActiveMouseCaptureMode())));
	}

	// ── Viewport client + the SViewport ─────────────────────────────────────────────────────────────
	if (UGameViewportClient* GVC = World ? World->GetGameViewport() : nullptr)
	{
		const TSharedPtr<SViewport> VW = GVC->GetGameViewportWidget();
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: viewport: capture=%s lock=%s ignoreInput=%d hideCursorDuringCapture=%d | SViewport hasMouseCapture=%d hasKeyboardFocus=%d focusedDescendants=%d"),
			*StaticEnum<EMouseCaptureMode>()->GetNameStringByValue(static_cast<int64>(GVC->GetMouseCaptureMode())),
			*StaticEnum<EMouseLockMode>()->GetNameStringByValue(static_cast<int64>(GVC->GetMouseLockMode())),
			GVC->IgnoreInput() ? 1 : 0, GVC->HideCursorDuringCapture() ? 1 : 0,
			VW.IsValid() && VW->HasMouseCapture() ? 1 : 0, VW.IsValid() && VW->HasKeyboardFocus() ? 1 : 0,
			VW.IsValid() && Slate.HasFocusedDescendants(VW.ToSharedRef()) ? 1 : 0);
	}

	// ── PlayerController ────────────────────────────────────────────────────────────────────────────
	if (APlayerController* PC = LP ? LP->GetPlayerController(World) : nullptr)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: PC=%s showCursor=%d moveIgnored=%d lookIgnored=%d inputEnabled=%d pawn=%s"),
			*PC->GetName(), PC->ShouldShowMouseCursor() ? 1 : 0, PC->IsMoveInputIgnored() ? 1 : 0, PC->IsLookInputIgnored() ? 1 : 0,
			PC->InputEnabled() ? 1 : 0, *GetNameSafe(PC->GetPawn()));
	}
	else
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: PC=<none for this world>"));
	}

	// ── Loading screen (its pre-processor eats everything while shown) ──────────────────────────────
	if (UGameInstance* GI = World ? World->GetGameInstance() : nullptr)
	{
		if (ULoadingScreenManager* LSM = GI->GetSubsystem<ULoadingScreenManager>())
		{
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: loadingScreenShown=%d"), LSM->GetLoadingScreenDisplayStatus() ? 1 : 0);
		}
	}

	UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE[%s] ================ end ================"), Why);
}

void AFLInputProbe::ClickFocused(UWorld* World)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	FSlateApplication& Slate = FSlateApplication::Get();
	const TSharedPtr<SWidget> Target = Slate.GetUserFocusedWidget(0);
	if (!Target.IsValid())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_INPUTPROBE: click: nothing is focused -- no target."));
		return;
	}
	const FGeometry& Geo = Target->GetCachedGeometry();
	const FVector2D Centre = Geo.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
	const TSharedPtr<SWindow> Window = Slate.FindWidgetWindow(Target.ToSharedRef());
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;

	UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: click: synthesising OS-level LMB at (%.0f,%.0f) on focused %s (window=%d)"),
		Centre.X, Centre.Y, *Target->ToString(), Native.IsValid() ? 1 : 0);
	Slate.SetCursorPos(Centre);
	Slate.OnMouseMove();
	const bool bDown = Slate.OnMouseDown(Native, EMouseButtons::Left, Centre);
	const bool bUp = Slate.OnMouseUp(EMouseButtons::Left, Centre);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: click: down handled=%d up handled=%d"), bDown ? 1 : 0, bUp ? 1 : 0);
}

namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLInputProbeCmd(
		TEXT("afl.UI.InputProbe"),
		TEXT("DEV ONLY. afl.UI.InputProbe [dump|click|start] -- dump the input pipeline state, synthesise a click on the focused widget, or start the front-of-chain input logger."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				const FString Mode = Args.Num() > 0 ? Args[0].ToLower() : TEXT("dump");
				if (Mode == TEXT("click"))
				{
					AFLInputProbe::ClickFocused(World);
				}
				else if (Mode == TEXT("start"))
				{
					AFLInputProbe::EnsureFrontLogger();
				}
				else
				{
					AFLInputProbe::Dump(World, TEXT("console"));
				}
				Ar.Logf(TEXT("afl.UI.InputProbe %s -- see LogAFLCombat AFL_INPUTPROBE lines."), *Mode);
			}));

	/** The post-match return path exactly as the scoreboard/BR result take it (UGameInstance::ReturnToMainMenu -> ?closed). */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLReturnToFrontEndCmd(
		TEXT("afl.Dev.ReturnToFrontEnd"),
		TEXT("DEV ONLY. Reproduce the after-match return: GameInstance->ReturnToMainMenu() (the same call the scoreboard CONTINUE makes)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				if (UGameInstance* GI = World ? World->GetGameInstance() : nullptr)
				{
					Ar.Logf(TEXT("afl.Dev.ReturnToFrontEnd -- ReturnToMainMenu()."));
					UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: afl.Dev.ReturnToFrontEnd -> GameInstance->ReturnToMainMenu()."));
					GI->ReturnToMainMenu();
				}
			}));

	// ── Autonomous repro: Armory (1st visit) -> click LobbyDoor -> Outpost -> return -> Armory (2nd visit) -> click ──
	//
	// Launch:  UnrealEditor.exe Bag_Man.uproject -game -ExecCmds="afl.Dev.ReproDeadFrontEnd signout"   (or "match")
	// Every step is timed off world state so it needs no hands on the window; the auto probes in RouteChoice fire on
	// the 2nd visit, then a synthetic OS-level click is sent. If the 2nd-visit click travels -> the in-Slate pipeline
	// is healthy and the eater sits outside Slate; if it does not -> the dumps name the layer.
	struct FAFLDeadFrontEndRepro
	{
		enum class EStep : uint8 { WaitFirstRouteChoice, WaitOutpost, WaitSecondRouteChoice, WaitVerdict, Done };
		EStep Step = EStep::WaitFirstRouteChoice;
		bool bMatchPath = false;
		double StepEnteredAt = 0.0;
		double StepStartedWaitingAt = 0.0;
		FTSTicker::FDelegateHandle Handle;

		static UWorld* GameWorld()
		{
			if (!GEngine) return nullptr;
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if ((Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE) && Ctx.World())
				{
					return Ctx.World();
				}
			}
			return nullptr;
		}
		static bool WorldIs(UWorld* World, const TCHAR* Needle) { return World && World->GetMapName().Contains(Needle); }
		static bool LoadingScreenUp(UWorld* World)
		{
			UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
			ULoadingScreenManager* LSM = GI ? GI->GetSubsystem<ULoadingScreenManager>() : nullptr;
			return LSM && LSM->GetLoadingScreenDisplayStatus();
		}
		static UAFLW_RouteChoice* ActiveRouteChoice(UWorld* World)
		{
			for (TObjectIterator<UAFLW_RouteChoice> It; It; ++It)
			{
				if (IsValid(*It) && It->IsActivated() && It->GetWorld() == World) return *It;
			}
			return nullptr;
		}
		void Enter(EStep NewStep, const TCHAR* Msg)
		{
			Step = NewStep;
			StepEnteredAt = FPlatformTime::Seconds();
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_REPRO: %s"), Msg);
		}
		bool Tick(float)
		{
			UWorld* World = GameWorld();
			const double Now = FPlatformTime::Seconds();
			const double InStep = Now - StepEnteredAt;
			switch (Step)
			{
			case EStep::WaitFirstRouteChoice:
				if (WorldIs(World, TEXT("Armory")) && ActiveRouteChoice(World) && !LoadingScreenUp(World))
				{
					if (StepStartedWaitingAt == 0.0) { StepStartedWaitingAt = Now; }
					else if (Now - StepStartedWaitingAt > 2.0)
					{
						UE_LOG(LogAFLCombat, Display, TEXT("AFL_REPRO: 1st visit -- RouteChoice up; synthetic click on the focused LobbyDoor."));
						AFLInputProbe::ClickFocused(World);
						StepStartedWaitingAt = 0.0;
						Enter(EStep::WaitOutpost, TEXT("waiting for the Outpost to load."));
					}
				}
				break;
			case EStep::WaitOutpost:
				if (WorldIs(World, TEXT("Outpost")) && !LoadingScreenUp(World))
				{
					if (StepStartedWaitingAt == 0.0) { StepStartedWaitingAt = Now; }
					else if (Now - StepStartedWaitingAt > 4.0)
					{
						StepStartedWaitingAt = 0.0;
						if (bMatchPath)
						{
							UE_LOG(LogAFLCombat, Display, TEXT("AFL_REPRO: in the Outpost -> ReturnToMainMenu() (the after-match path)."));
							if (UGameInstance* GI = World->GetGameInstance()) { GI->ReturnToMainMenu(); }
						}
						else
						{
							UE_LOG(LogAFLCombat, Display, TEXT("AFL_REPRO: in the Outpost -> OpenLevel(L_IRONICS_Armory) (the Sign-Out path)."));
							UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/BagMan/Armory/L_IRONICS_Armory")));
						}
						Enter(EStep::WaitSecondRouteChoice, TEXT("waiting for the 2nd-visit RouteChoice (auto probes fire at +0/+1/+4s)."));
					}
				}
				else if (InStep > 60.0) { Enter(EStep::Done, TEXT("ABORT: Outpost never loaded within 60s.")); }
				break;
			case EStep::WaitSecondRouteChoice:
				if (WorldIs(World, TEXT("Armory")) && ActiveRouteChoice(World) && !LoadingScreenUp(World))
				{
					if (StepStartedWaitingAt == 0.0) { StepStartedWaitingAt = Now; }
					else if (Now - StepStartedWaitingAt > 6.0)
					{
						StepStartedWaitingAt = 0.0;
						AFLInputProbe::Dump(World, TEXT("repro: 2nd visit, before the synthetic click"));
						UE_LOG(LogAFLCombat, Display, TEXT("AFL_REPRO: 2nd visit -- synthetic click on the focused LobbyDoor."));
						AFLInputProbe::ClickFocused(World);
						Enter(EStep::WaitVerdict, TEXT("waiting up to 10s for the click to travel."));
					}
				}
				else if (InStep > 60.0) { Enter(EStep::Done, TEXT("ABORT: 2nd-visit RouteChoice never appeared within 60s (Landing may be on the sign-in card).")); }
				break;
			case EStep::WaitVerdict:
				if (WorldIs(World, TEXT("Outpost")) || (WorldIs(World, TEXT("Armory")) && !ActiveRouteChoice(World)))
				{
					Enter(EStep::Done, TEXT("VERDICT: the 2nd-visit synthetic click WORKED (route chosen) -> the in-Slate pipeline is healthy; a dead real click means the eater is outside Slate (OS/window level)."));
				}
				else if (InStep > 10.0)
				{
					AFLInputProbe::Dump(World, TEXT("repro: 2nd visit, click did nothing"));
					Enter(EStep::Done, TEXT("VERDICT: the 2nd-visit synthetic click did NOTHING -> an in-Slate eater; read the dumps above (filter / capture / chain under cursor)."));
				}
				break;
			case EStep::Done:
			default:
				return false; // unregister the ticker
			}
			return true;
		}
	};
	TSharedPtr<FAFLDeadFrontEndRepro> GRepro;

	FAutoConsoleCommand GAFLReproDeadFrontEndCmd(
		TEXT("afl.Dev.ReproDeadFrontEnd"),
		TEXT("DEV ONLY. afl.Dev.ReproDeadFrontEnd [signout|match] -- autonomous repro of the dead 2nd-visit front end: click LobbyDoor, reach the Outpost, return by the chosen path, probe, click again, log a VERDICT."),
		FConsoleCommandWithArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args)
			{
				GRepro = MakeShared<FAFLDeadFrontEndRepro>();
				GRepro->bMatchPath = Args.Num() > 0 && Args[0].ToLower() == TEXT("match");
				GRepro->StepEnteredAt = FPlatformTime::Seconds();
				UE_LOG(LogAFLCombat, Display, TEXT("AFL_REPRO: armed (%s path). Step 1: waiting for the 1st-visit RouteChoice."), GRepro->bMatchPath ? TEXT("match/ReturnToMainMenu") : TEXT("sign-out/OpenLevel"));
				AFLInputProbe::EnsureFrontLogger();
				TSharedPtr<FAFLDeadFrontEndRepro> Repro = GRepro;
				GRepro->Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Repro](float Dt) { return Repro->Tick(Dt); }), 0.25f);
			}));

	/** The Sign Out path exactly as the System Menu takes it (UGameplayStatics::OpenLevel of the front-end map, no ?closed). */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLOpenFrontEndCmd(
		TEXT("afl.Dev.OpenFrontEnd"),
		TEXT("DEV ONLY. Reproduce the Sign-Out return: OpenLevel(L_IRONICS_Armory) (the same call the System Menu SIGN OUT makes, without the logout)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				if (World)
				{
					Ar.Logf(TEXT("afl.Dev.OpenFrontEnd -- OpenLevel(L_IRONICS_Armory)."));
					UE_LOG(LogAFLCombat, Display, TEXT("AFL_INPUTPROBE: afl.Dev.OpenFrontEnd -> OpenLevel(/Game/BagMan/Armory/L_IRONICS_Armory)."));
					UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/BagMan/Armory/L_IRONICS_Armory")));
				}
			}));
}

#endif // !UE_BUILD_SHIPPING
