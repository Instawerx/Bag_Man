// Copyright C12 AI Gaming. All Rights Reserved.

#include "CoreMinimal.h"

// ══ DEVELOPMENT-ONLY MENU SUMMONS ═════════════════════════════════════════════════════════════════════
//
// Surfaces that are deliberately unreachable from the shipping front end, kept summonable for development.
// Everything in this file compiles out of a shipping client.
//
// ⚠ THE GUARD IS `!UE_BUILD_SHIPPING`, NOT `WITH_DEV_AUTOMATION_TESTS`. The ruling asked for "local
// non-shipping development builds", and that is exactly what this macro means. WITH_DEV_AUTOMATION_TESTS
// would also work today, but it is the flag for automation-test code specifically: a build that trims
// automation (`-DWITH_DEV_AUTOMATION_TESTS=0` is a supported thing to do, and Test configurations clear it
// on their own) would silently take the dev tooling with it. Two unrelated capabilities should not share
// one switch. `!UE_BUILD_SHIPPING` also keeps the command in Test builds, which is where a QA pass would
// most want it.

#if !UE_BUILD_SHIPPING

#include "AFLCombat.h"              // LogAFLCombat
#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "NativeGameplayTags.h"

namespace
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Menu_DevSummon, "UI.Layer.Menu");

	/**
	 * ⚠ A RUNTIME STRING PATH, ON PURPOSE -- the one place in this project where that is correct.
	 *
	 * Every other string-built asset path here has been a cook bug (see the DirectoriesToAlwaysCook block
	 * in DefaultGame.ini: five shipping assets were silently missing from packaged builds for exactly this
	 * reason, FBIK among them). The property that made those a defect is the property we want here: the
	 * cooker cannot see this reference, so naming the screen costs the shipping build nothing. It is the
	 * LAST path to the asset -- nothing else in content references it any more -- and /Game/DeveloperUtils
	 * is in DirectoriesToNeverCook besides, so the screen is absent from a packaged client even if this
	 * file were somehow compiled into one.
	 */
	const TCHAR* const GHostMenuPath =
		TEXT("/Game/DeveloperUtils/Host/W_ExperienceSelectionScreen.W_ExperienceSelectionScreen_C");

	/** Push a menu-layer widget for the first local player, once one exists. Shared by every summon here. */
	void SummonDevMenu(UWorld* World, const TCHAR* AssetPath, const TCHAR* Label)
	{
		// Waits rather than failing fast, for the same reason afl.Home.Door does: -ExecCmds fires at engine
		// init, long before the front end has a local player or a layer stack, and a headless session is the
		// only way this gets exercised without a mouse. Polls to a hard deadline, then gives up loudly.
		TWeakObjectPtr<UWorld> WeakWorld(World);
		const FString Path(AssetPath);
		const FString Name(Label);
		double Deadline = 25.0;

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[WeakWorld, Path, Name, Deadline](float Delta) mutable -> bool
			{
				Deadline -= Delta;
				UWorld* W = WeakWorld.Get();
				if (!W || Deadline <= 0.0)
				{
					UE_LOG(LogAFLCombat, Error,
						TEXT("AFL_DEV: %s summon gave up -- no local player appeared."), *Name);
					return false;
				}

				ULocalPlayer* LocalPlayer = GEngine ? GEngine->GetFirstGamePlayer(W) : nullptr;
				if (!LocalPlayer)
				{
					return true;   // keep waiting
				}

				UClass* ScreenClass = LoadClass<UCommonActivatableWidget>(nullptr, *Path);
				if (!ScreenClass)
				{
					// The expected outcome in any build where the asset was correctly excluded. Not a
					// failure of the command -- a demonstration that the deprecation held.
					UE_LOG(LogAFLCombat, Error,
						TEXT("AFL_DEV: %s is not on disk ('%s') -- deprecated and not cooked."), *Name, *Path);
					return false;
				}

				UCommonUIExtensions::PushContentToLayer_ForPlayer(
					LocalPlayer, TAG_UI_Layer_Menu_DevSummon, ScreenClass);
				UE_LOG(LogAFLCombat, Log,
					TEXT("AFL_DEV: summoned %s (%s) onto UI.Layer.Menu."), *ScreenClass->GetName(), *Name);
				return false;
			}), 0.5f);
	}

	/**
	 * `afl.Debug.SummonHostMenu` -- the deprecated HOST surface, for development only.
	 *
	 * HOST was removed from the front end by ruling on 2026-08-10: starting a match is the matchmaking
	 * queue's job (door -> queue -> allocator), and a client that can pick a map and listen-serve it can
	 * originate a session the allocator never authorised. `W_ExperienceSelectionScreen` still exists and
	 * still works -- it moved to /Game/DeveloperUtils/Host/, out of the cooked tree -- because it remains
	 * the fastest way to launch an arbitrary experience while developing one.
	 *
	 * It is NOT a hidden route back to the old front end for players: in a shipping client this command
	 * does not exist and the asset is not in the build.
	 */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLSummonHostMenuCmd(
		TEXT("afl.Debug.SummonHostMenu"),
		TEXT("DEV ONLY. Summon the deprecated HOST / experience-selection screen onto UI.Layer.Menu."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				Ar.Logf(TEXT("afl.Debug.SummonHostMenu -- deprecated surface; will open once a local player exists."));
				SummonDevMenu(World, GHostMenuPath, TEXT("HOST"));
			}));
}

#endif   // !UE_BUILD_SHIPPING
