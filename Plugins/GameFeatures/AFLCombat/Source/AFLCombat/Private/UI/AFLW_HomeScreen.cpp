// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_HomeScreen.h"

#include "AFLCombat.h"              // LogAFLCombat
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Internationalization/Text.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_HomeScreen)

#define LOCTEXT_NAMESPACE "AFLHomeScreen"

UAFLW_HomeScreen::UAFLW_HomeScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Honest default: the staked lobby is unbuilt and every staked queue is unpublished. The reason is
	// authored here rather than left empty so a WBP that forgets to set it still says something true.
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
}

void UAFLW_HomeScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Re-apply on every activation: availability is a live product state (a staked queue can open between
	// two visits to this screen), and the screen is returned to rather than constructed each time.
	ApplyStakedAvailability();
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
	BP_OnDoorChosen(Door);
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
