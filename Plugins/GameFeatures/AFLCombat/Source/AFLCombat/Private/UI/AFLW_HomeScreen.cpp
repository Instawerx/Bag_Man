// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_HomeScreen.h"

#include "UI/AFLW_RouteChoice.h" // post-login route consumption (operator ruling 2026-09-01)

#include "AFLCombat.h"              // LogAFLCombat
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "CommonUIExtensions.h"     // PushContentToLayer_ForPlayer -- the same call the store push uses
#include "Containers/Ticker.h"      // the door probe waits for the screen to exist
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Internationalization/Text.h"
#include "NativeGameplayTags.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_HomeScreen)

#define LOCTEXT_NAMESPACE "AFLHomeScreen"

/** The CommonUI menu stack. File-local, matching how the store cheat declares the same tag. */
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Menu_HomeDoor, "UI.Layer.Menu");

/**
 * One footer nav item.
 *
 * ⚠ THE MEMBER POINTERS ARE THE POINT. A nav item is three things -- an identity, a button, a destination
 * -- and the obvious implementations keep those in three separate places: an enum, a list of bindings, and
 * a switch. Three lists drift. Here the association is stated once, so `ApplyNavAvailability` (binds the
 * click, disables the unbuilt), `OpenNavTarget` (pushes) and `TryParseNavTarget` (console) all read the
 * SAME row. Adding a sixth item is one line; forgetting to bind it is not expressible.
 */
struct FAFLNavRoute
{
	EAFLNavTarget Target;

	/** Console id for `afl.Home.Nav`. Lowercase, and the only string in the routing. */
	const TCHAR* Id;

	TObjectPtr<UCommonButtonBase>           UAFLW_HomeScreen::*Button;
	TSoftClassPtr<UCommonActivatableWidget> UAFLW_HomeScreen::*Destination;
};

namespace
{
	/**
	 * `afl.Home.Door <league|staked>` -- exercise the door routing without an input device.
	 *
	 * ⚠ THIS IS THE ONLY WAY THE PUSH GETS VERIFIED. A unit test cannot reach it (there is no widget tree
	 * and no layer stack), and the headless `-game` session that boots the front end has no mouse. Without
	 * this the navigation would ship on the strength of the code reading correctly, which is exactly the
	 * standard that let a screen sit for weeks with all 31 widgets unparented.
	 *
	 * It calls ChooseDoor, so it goes through the SAME availability gate a click does -- asking for the
	 * staked door while it is shut prints the refusal rather than bypassing it.
	 */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLHomeDoorCmd(
		TEXT("afl.Home.Door"),
		TEXT("Choose a home-screen door as if clicked: afl.Home.Door [league|staked]. Goes through the real gate."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				const bool bStaked = Args.Num() > 0 && Args[0].StartsWith(TEXT("s"), ESearchCase::IgnoreCase);

				// ⚠ IT WAITS FOR THE SCREEN RATHER THAN FAILING FAST, and that is the point of the probe:
				// -ExecCmds fires at engine init, LONG before the front end builds its widgets, so a
				// fail-fast version can only ever be run by hand -- which is the thing there is no way to
				// do headlessly. Polls to a hard deadline, then gives up loudly.
				const bool bWantStaked = bStaked;
				TWeakObjectPtr<UWorld> WeakWorld(World);
				double Deadline = 25.0;

				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
					[bWantStaked, WeakWorld, Deadline](float Delta) mutable -> bool
					{
						Deadline -= Delta;
						UWorld* W = WeakWorld.Get();
						if (!W || Deadline <= 0.0)
						{
							UE_LOG(LogAFLCombat, Error,
								TEXT("AFL_HOME: afl.Home.Door gave up -- no home screen appeared."));
							return false;   // stop ticking
						}

						// The live instance, not the CDO: pushing a layer needs a real widget with a local
						// player behind it.
						for (TObjectIterator<UAFLW_HomeScreen> It; It; ++It)
						{
							UAFLW_HomeScreen* Home = *It;
							if (Home && Home->GetWorld() == W && !Home->HasAnyFlags(RF_ClassDefaultObject))
							{
								Home->ChooseDoor(bWantStaked ? EAFLHomeDoor::Staked : EAFLHomeDoor::League);
								return false;
							}
						}
						return true;   // keep waiting
					}), 0.5f);

				Ar.Logf(TEXT("afl.Home.Door -- will choose %s as soon as the home screen exists."),
					bStaked ? TEXT("STAKED") : TEXT("LEAGUE"));
			}));

	/** `afl.Home.Nav <loadout|store|venues|career|settings>` -- same wait-for-the-screen probe, for the footer. */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLHomeNavCmd(
		TEXT("afl.Home.Nav"),
		TEXT("Open a footer nav destination as if clicked: afl.Home.Nav [loadout|store|venues|career|settings]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				const FName NavId(Args.Num() > 0 ? *Args[0].ToLower() : TEXT("store"));
				TWeakObjectPtr<UWorld> WeakWorld(World);
				double Deadline = 25.0;

				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
					[NavId, WeakWorld, Deadline](float Delta) mutable -> bool
					{
						Deadline -= Delta;
						UWorld* W = WeakWorld.Get();
						if (!W || Deadline <= 0.0)
						{
							UE_LOG(LogAFLCombat, Error, TEXT("AFL_HOME: afl.Home.Nav gave up -- no home screen."));
							return false;
						}
						for (TObjectIterator<UAFLW_HomeScreen> It; It; ++It)
						{
							UAFLW_HomeScreen* Home = *It;
							if (Home && Home->GetWorld() == W && !Home->HasAnyFlags(RF_ClassDefaultObject))
							{
								Home->OpenNavTargetById(NavId);
								return false;
							}
						}
						return true;
					}), 0.5f);

				Ar.Logf(TEXT("afl.Home.Nav -- will open '%s' as soon as the home screen exists."), *NavId.ToString());
			}));
}

UAFLW_HomeScreen::UAFLW_HomeScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Authored here rather than left empty so a WBP that forgets to set it still says something true. The
	// shipping WBP overrides this with the specific reason -- S4 TicketReview is unbuilt (see the header);
	// this generic fallback only shows on a WBP that set nothing at all.
	StakedUnavailableReason = LOCTEXT("StakedNotOpen", "Not open yet");
}

void UAFLW_HomeScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// BindWidget guarantees these exist on a compiled WBP, but this class is also instantiable from a test
	// harness with no widget tree at all -- so bind defensively rather than assume the designer contract.
	// UCommonButtonBase exposes OnClicked() as a METHOD returning a plain FCommonButtonEvent, not a
	// UPROPERTY delegate -- its dynamic sibling OnButtonBaseClicked is private/BlueprintAssignable and
	// unreachable from C++. So this is AddUObject on a no-param event, NOT AddDynamic.
	if (LeagueDoor)
	{
		LeagueDoor->OnClicked().AddUObject(this, &UAFLW_HomeScreen::HandleLeagueClicked);
	}
	if (StakedDoor)
	{
		StakedDoor->OnClicked().AddUObject(this, &UAFLW_HomeScreen::HandleStakedClicked);
	}

	ApplyStakedAvailability();
	ApplyNavAvailability();
}

void UAFLW_HomeScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Re-apply on every activation: availability is a live product state (a staked queue can open between
	// two visits to this screen), and the screen is returned to rather than constructed each time. The same
	// is true of the nav -- a destination can be configured in between visits.
	ApplyStakedAvailability();
	ApplyNavAvailability();

	// POST-LOGIN ROUTE (operator ruling 2026-09-01): the route-choice screen picked MATCHMAKING ->
	// open the League door through the SAME proven wiring a click would use. Consumed exactly once.
	if (UAFLW_RouteChoice::ConsumePendingMatchmakingRoute())
	{
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_ROUTE: home screen consuming matchmaking route -> League door."));
		ChooseDoor(EAFLHomeDoor::League);
	}
}

UWidget* UAFLW_HomeScreen::NativeGetDesiredFocusTarget() const
{
	// sec7: LEAGUE takes initial focus. Not arbitrary -- most players are on the free side (R98), so the
	// default landing spot is the one most players want. A controller player should never have to travel
	// through the wagering door to reach the free one.
	if (LeagueDoor)
	{
		return LeagueDoor;
	}
	return Super::NativeGetDesiredFocusTarget();
}

bool UAFLW_HomeScreen::IsDoorAvailable(EAFLHomeDoor Door) const
{
	// League is ALWAYS open. It is the free half of the economy and the on-ramp; gating it would strand
	// the majority of the population behind a door built for the minority.
	return (Door == EAFLHomeDoor::League) || bStakedPlayAvailable;
}

bool UAFLW_HomeScreen::IsStakeLegalForDoor(EAFLHomeDoor Door, int64 StakeAmount)
{
	// THE R98 INVARIANT. League has no buy-in to pick, so the only legal stake on that route is none.
	// Staked play's amount is chosen behind its door, so any non-negative value is legal here.
	if (Door == EAFLHomeDoor::League)
	{
		return StakeAmount == 0;
	}
	return StakeAmount >= 0;
}

void UAFLW_HomeScreen::SetWalletReadout(int64 Watts, int64 Volts)
{
	// Chrome, shown on both sides of the split: a player's BALANCE is not a stake, and hiding it from
	// league players would be a different error -- league is where Watts are earned, so it is exactly the
	// side that wants to see them accumulate.
	//
	// The intended live source is UAFLWalletComponent (Cosmetics/AFLWalletComponent.h), which already backs
	// the store's entitlement checks. Left as a setter rather than a direct bind because the home screen
	// has no verified read path to it yet -- wiring that is owed, and guessing the API here would be worse.
	if (WattsChip)
	{
		WattsChip->SetText(FText::AsNumber(Watts));
	}
	if (VoltsChip)
	{
		VoltsChip->SetText(FText::AsNumber(Volts));
	}
}

void UAFLW_HomeScreen::HandleLeagueClicked()
{
	ChooseDoor(EAFLHomeDoor::League);
}

void UAFLW_HomeScreen::HandleStakedClicked()
{
	ChooseDoor(EAFLHomeDoor::Staked);
}

void UAFLW_HomeScreen::ChooseDoor(EAFLHomeDoor Door)
{
	if (!IsDoorAvailable(Door))
	{
		// Reachable despite the door being visually disabled: SetIsInteractionEnabled is a presentation
		// state, and a gamepad or an accessibility path can still deliver the click. Refuse it HERE, where
		// the gate is authoritative, rather than trusting the button's appearance to enforce a product rule.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_HOME: door %s clicked while unavailable -- refused."),
			Door == EAFLHomeDoor::League ? TEXT("LEAGUE") : TEXT("STAKED"));
		return;
	}

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_HOME: chose %s."),
		Door == EAFLHomeDoor::League ? TEXT("LEAGUE PLAY") : TEXT("STAKED PLAY"));

	OnDoorChosen.Broadcast(Door);

	// ⚠ THE HOOK FIRES REGARDLESS OF WHETHER C++ NAVIGATES. A WBP may be doing the sec6 select transition
	// (the chosen door blooms, the sibling recedes and blurs) or reporting analytics off this; suppressing
	// it when a lobby class happens to be set would make those behaviours silently configuration-dependent.
	BP_OnDoorChosen(Door);

	PushLobbyForDoor(Door);
}

void UAFLW_HomeScreen::PushLobbyForDoor(EAFLHomeDoor Door)
{
	const TSoftClassPtr<UCommonActivatableWidget>& Soft =
		(Door == EAFLHomeDoor::League) ? LeagueLobbyClass : StakedLobbyClass;

	if (Soft.IsNull())
	{
		// Not an error: a WBP owning navigation leaves these empty on purpose (see the header). Verbose so
		// the deliberate case is quiet and the accidental one is still findable.
		UE_LOG(LogAFLCombat, Verbose,
			TEXT("AFL_HOME: no lobby class set for this door -- navigation left to the WBP."));
		return;
	}
	PushScreen(Soft, Door == EAFLHomeDoor::League ? TEXT("league lobby") : TEXT("staked lobby"));
}

bool UAFLW_HomeScreen::PushScreen(const TSoftClassPtr<UCommonActivatableWidget>& Soft, const TCHAR* Context)
{
	// Synchronous load AT THE MOMENT OF THE CLICK. The soft references exist so boot does not pay for every
	// destination; by here the player has committed to one, and a hitch on a menu transition is the right
	// place to spend that cost. Async would mean a click that appears to do nothing for a frame or two.
	UClass* ScreenClass = Soft.LoadSynchronous();
	if (!ScreenClass)
	{
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_HOME: %s class '%s' failed to load -- dead end."), Context, *Soft.ToString());
		return false;
	}

	APlayerController* PC = GetOwningPlayer();
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_HOME: no local player -- cannot push %s."), Context);
		return false;
	}

	// The same call Lyra's own menus use, and the same one afl.Store.Open uses for the cosmetic shop. The
	// pushed widget's InputConfig=Menu captures input, and its own Back/Esc pops it off THIS stack --
	// which is what makes the home screen still be there underneath when the player returns.
	UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, TAG_UI_Layer_Menu_HomeDoor, ScreenClass);

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_HOME: pushed %s (%s) onto UI.Layer.Menu."), *ScreenClass->GetName(), Context);
	return true;
}

TArrayView<const FAFLNavRoute> UAFLW_HomeScreen::GetNavRoutes()
{
	// sec3's footer, in the order it is drawn. FIVE ITEMS -- see EAFLNavTarget for why HOST and REPLAYS are
	// not among them. Both member pointers are taken here, inside the class, which is what lets protected
	// BindWidget slots be named in a plain struct.
	static const FAFLNavRoute Routes[] = {
		{ EAFLNavTarget::Loadout,  TEXT("loadout"),  &UAFLW_HomeScreen::Nav_Loadout,  &UAFLW_HomeScreen::LoadoutScreenClass  },
		{ EAFLNavTarget::Store,    TEXT("store"),    &UAFLW_HomeScreen::Nav_Store,    &UAFLW_HomeScreen::StoreScreenClass    },
		{ EAFLNavTarget::Venues,   TEXT("venues"),   &UAFLW_HomeScreen::Nav_Venues,   &UAFLW_HomeScreen::VenuesScreenClass   },
		{ EAFLNavTarget::Career,   TEXT("career"),   &UAFLW_HomeScreen::Nav_Career,   &UAFLW_HomeScreen::CareerScreenClass   },
		{ EAFLNavTarget::Settings, TEXT("settings"), &UAFLW_HomeScreen::Nav_Settings, &UAFLW_HomeScreen::SettingsScreenClass },
	};
	return MakeArrayView(Routes);
}

const FAFLNavRoute* UAFLW_HomeScreen::FindNavRoute(EAFLNavTarget Target)
{
	for (const FAFLNavRoute& Route : GetNavRoutes())
	{
		if (Route.Target == Target)
		{
			return &Route;
		}
	}
	return nullptr;
}

bool UAFLW_HomeScreen::IsNavTargetRouted(EAFLNavTarget Target)
{
	return FindNavRoute(Target) != nullptr;
}

FName UAFLW_HomeScreen::GetNavTargetId(EAFLNavTarget Target)
{
	const FAFLNavRoute* Route = FindNavRoute(Target);
	return Route ? FName(Route->Id) : NAME_None;
}

bool UAFLW_HomeScreen::TryParseNavTarget(FName NavId, EAFLNavTarget& OutTarget)
{
	for (const FAFLNavRoute& Route : GetNavRoutes())
	{
		if (NavId == FName(Route.Id))
		{
			OutTarget = Route.Target;
			return true;
		}
	}
	return false;   // no guessing, no nearest match: an unknown id is a caller bug, not a routing decision
}

void UAFLW_HomeScreen::OpenNavTargetById(FName NavId)
{
	EAFLNavTarget Target = EAFLNavTarget::Store;
	if (!TryParseNavTarget(NavId, Target))
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_HOME: unknown nav id '%s'."), *NavId.ToString());
		return;
	}
	OpenNavTarget(Target);
}

void UAFLW_HomeScreen::OpenNavTarget(EAFLNavTarget Target)
{
	const FAFLNavRoute* Route = FindNavRoute(Target);
	if (!Route)
	{
		// Only reachable if a value was added to EAFLNavTarget without a row -- which is the case the table
		// exists to make loud rather than silent. Error, not warning: it is a code defect, not a state.
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_HOME: nav target %d has no route -- add a row to GetNavRoutes()."), (int32)Target);
		return;
	}

	const TSoftClassPtr<UCommonActivatableWidget>& Destination = this->*(Route->Destination);
	if (Destination.IsNull())
	{
		// Refused HERE as well as visually disabled, for the same reason ChooseDoor re-checks availability:
		// SetIsInteractionEnabled is presentation, and a gamepad or accessibility path can still deliver
		// the click. A nav item with no destination must not silently appear to work.
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_HOME: nav '%s' has no destination yet -- refused."), Route->Id);
		return;
	}
	PushScreen(Destination, Route->Id);
}

void UAFLW_HomeScreen::ApplyNavAvailability()
{
	for (const FAFLNavRoute& Route : GetNavRoutes())
	{
		UCommonButtonBase* Button = this->*(Route.Button);
		if (!Button)
		{
			continue;   // BindWidgetOptional: a WBP need not author every nav item
		}

		// AddUnique-equivalent: NativeOnInitialized runs once, but ApplyNavAvailability is also called on
		// every activation, and a duplicate binding would push the screen N times per click.
		const EAFLNavTarget Target = Route.Target;
		Button->OnClicked().RemoveAll(this);
		Button->OnClicked().AddWeakLambda(this, [this, Target] { OpenNavTarget(Target); });

		// ⚠ DISABLED, NOT HIDDEN. Same reasoning as the staked door: a player should be able to see that
		// Venues and Career are coming and that they are not available yet. Hiding them would make the
		// footer silently change shape as surfaces ship.
		Button->SetIsInteractionEnabled(!(this->*(Route.Destination)).IsNull());
	}
}

void UAFLW_HomeScreen::ApplyStakedAvailability()
{
	if (StakedDoor)
	{
		// Interaction only -- the door stays VISIBLE and stays a focus stop. sec5's Disabled treatment is
		// 42% opacity with a stated reason, not a hidden control: a player must be able to see that staked
		// play exists and read why it is shut, or the split silently becomes a one-door screen.
		StakedDoor->SetIsInteractionEnabled(bStakedPlayAvailable);
	}

	if (StakedReason)
	{
		StakedReason->SetText(StakedUnavailableReason);
		StakedReason->SetVisibility(bStakedPlayAvailable
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}

	BP_OnStakedAvailabilityChanged(bStakedPlayAvailable, StakedUnavailableReason);
}

#undef LOCTEXT_NAMESPACE
