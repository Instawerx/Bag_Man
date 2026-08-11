// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/Lobby/AFLW_Lobby_Root.h"

#include "AFLCombat.h"                 // LogAFLCombat
#include "AFLMatchmakingSubsystem.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "CommonUIExtensions.h"        // PushContentToLayer_ForPlayer -- the same call every other push uses
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "Cosmetics/AFLWalletComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "NativeGameplayTags.h"
#include "UI/Lobby/AFLW_Lobby_DetailPanel.h"
#include "UI/Lobby/AFLW_Lobby_QueueRow.h"
#include "UI/Lobby/AFLW_TicketReview.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_Lobby_Root)

#define LOCTEXT_NAMESPACE "AFLLobbyRoot"

/** The CommonUI menu stack. File-local, matching how the home screen declares the same tag. */
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_Layer_Menu_Lobby, "UI.Layer.Menu");

UAFLW_Lobby_Root::UAFLW_Lobby_Root(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  THE INVARIANT
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

bool UAFLW_Lobby_Root::IsAxisLegalForDoor(EAFLHomeDoor InDoor, EAFLLobbyAxis Axis)
{
	switch (Axis)
	{
	case EAFLLobbyAxis::Ruleset:
	case EAFLLobbyAxis::VenueClass:
	case EAFLLobbyAxis::Size:
		return true;

	case EAFLLobbyAxis::League:
		// R86. Staked play is PRO MOD ONLY, so on that door the league is a STATED FACT, not a control.
		// Rendering it as a one-option picker would offer a choice that does not exist.
		return InDoor == EAFLHomeDoor::League;

	case EAFLLobbyAxis::Denomination:
	case EAFLLobbyAxis::Stake:
		// R98's sharpest clause. `LEAGUE_DOOR_SPEC.md` §1: the stake axis is "— absent —. NOT HIDDEN, NOT
		// ZEROED, NOT DISABLED", and it warns in the same breath about the future temptation to render a
		// greyed-out field here "for consistency with the staked door" -- which reintroduces exactly the
		// framing R98 removed. A league player never encounters a buy-in, so there is nothing to grey out.
		return InDoor == EAFLHomeDoor::Staked;
	}
	return false;
}

EAFLPlayTier UAFLW_Lobby_Root::ResolveTier(EAFLHomeDoor InDoor, EAFLDenomination InDenomination)
{
	if (InDoor == EAFLHomeDoor::League)
	{
		return EAFLPlayTier::LeaguePlay;
	}
	return InDenomination == EAFLDenomination::Volts ? EAFLPlayTier::VoltsPlay : EAFLPlayTier::WattsPlay;
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  LIFECYCLE
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

void UAFLW_Lobby_Root::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// UCommonButtonBase::OnClicked() is a METHOD returning a plain no-param FCommonButtonEvent; the dynamic
	// sibling OnButtonBaseClicked is private and unreachable from C++. So these are weak lambdas on a
	// simple multicast, never AddDynamic. Same idiom as UAFLW_HomeScreen.
	BindAxisButton(MatchPlayTab,     [this] { SelectRuleset(EAFLRuleset::MatchPlay); });
	BindAxisButton(BattleRoyaleTab,  [this] { SelectRuleset(EAFLRuleset::BattleRoyale); });
	BindAxisButton(HaywireButton,    [this] { SelectLeague(EAFLLeague::Haywire); });
	BindAxisButton(ProModButton,     [this] { SelectLeague(EAFLLeague::ProMod); });
	BindAxisButton(WattsButton,      [this] { SelectDenomination(EAFLDenomination::Watts); });
	BindAxisButton(VoltsButton,      [this] { SelectDenomination(EAFLDenomination::Volts); });
	BindAxisButton(ArenaButton,      [this] { SelectVenueClass(EAFLVenueClass::Arena); });
	BindAxisButton(MapButton,        [this] { SelectVenueClass(EAFLVenueClass::Map); });
	BindAxisButton(CommitButton,     [this] { CommitQueue(); });

	// BACK. This was declared BindWidgetOptional and never bound, so the lobby had no way out: the widget
	// sat on UI.Layer.Menu with nothing to pop it. Optional binding is why it failed silently -- a missing
	// BindWidget errors at compile, a missing BindWidgetOptional is just null, and a bound-but-unwired
	// button looks identical to a wired one until someone clicks it. Same idiom as UAFLW_VenueShowcase.
	BindAxisButton(BackButton,       [this] { DeactivateWidget(); });

	if (StakeNumericField)
	{
		StakeNumericField->OnTextChanged.AddDynamic(this, &UAFLW_Lobby_Root::HandleStakeTextChanged);
	}

	if (VenueNote)
	{
		// R18 §2.2: the venue is disclosed as an OUTCOME, stated up front rather than framed as a
		// limitation -- and never as a picker. It is authored here so a WBP cannot quietly drop it.
		VenueNote->SetText(LOCTEXT("VenueAtStart", "Venue assigned at match start"));
	}

	// The door arrives from the WBP class defaults, so it is already set by the time we get here and no
	// SetDoor call will ever transition to it. Apply the door's invariants against whatever it IS.
	EnforceDoorInvariants();

	ApplyDoorScoping();
	BindQueueDirectory();
}

void UAFLW_Lobby_Root::BindQueueDirectory()
{
	UAFLQueueDirectorySubsystem* Directory = UAFLQueueDirectorySubsystem::Get(this);
	if (!Directory)
	{
		// Not an error worth logging every construct: the directory is a GameInstanceSubsystem, so this is
		// null in a widget-preview or a test harness with no game instance. The screen still renders from
		// whatever SetQueueSet is handed directly, which is exactly why that stayed public.
		return;
	}

	if (!DirectoryHandle.IsValid())
	{
		DirectoryHandle = Directory->OnUpdated.AddUObject(this, &UAFLW_Lobby_Root::HandleDirectoryUpdated);
	}
	if (!PresenceHandle.IsValid())
	{
		PresenceHandle = Directory->OnPresenceUpdated.AddUObject(this, &UAFLW_Lobby_Root::SetOnlineCount);
	}

	// The MEASURED online count -- clients that heartbeat inside the window. Deliberately not summed from
	// /population, which publishes no headline total on purpose, and not the queued count, which is a
	// different quantity. It reads 0 with nobody playing, which is the correct output of a real source.
	SetOnlineCount(Directory->GetOnlineCount());
	Directory->RefreshPresence();

	// Take what is already cached BEFORE asking for a refresh. Returning to this screen from a match or a
	// cancelled queue should paint the ladder immediately rather than showing an empty list for a round
	// trip -- the previous read is still the best answer we have until a newer one lands.
	if (Directory->GetQueues().Num() > 0)
	{
		SetQueueSet(Directory->GetQueues());
	}
	Directory->Refresh();
}

void UAFLW_Lobby_Root::BindWallet()
{
	// The store's idiom (UAFLW_FrontEndMarket): the wallet is a UPlayerStateComponent on the local PS.
	APlayerController* PC = GetOwningPlayer();
	APlayerState* PS = PC ? PC->PlayerState : nullptr;
	UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;

	if (!Wallet)
	{
		// UNKNOWN, not zero. This is the honest state and it is also the safe one: SetWalletReadout with
		// INDEX_NONE skeletons the chips and RefreshCommitState disables a staked entry with "Balance
		// unavailable" rather than a shortfall the player cannot act on.
		SetWalletReadout(INDEX_NONE, INDEX_NONE);
		UE_LOG(LogAFLCombat, Verbose,
			TEXT("AFL_LOBBY: no wallet on the local PlayerState yet -- balance reads unavailable."));
		return;
	}

	// AddUnique: this runs on every activation, and a duplicate binding would refresh the chips N times per
	// change for the life of the process.
	Wallet->OnWalletChanged.AddUniqueDynamic(this, &UAFLW_Lobby_Root::HandleWalletChanged);

	// ⚠ SEED THROUGH IsBalanceKnown, NOT THROUGH THE NUMBERS. GetVolts() returns 0 both for a player who
	// has nothing and for a load that has not landed; only the flag separates them, and staked §6 turns on
	// exactly that distinction -- skeleton and disable on unknown, shortfall and route-to-store on a real
	// zero. Reading the numbers alone would show every player a confident 0 for the first few frames.
	if (Wallet->IsBalanceKnown())
	{
		SetWalletReadout(Wallet->GetWatts(), Wallet->GetVolts());
	}
	else
	{
		SetWalletReadout(INDEX_NONE, INDEX_NONE);
	}
}

void UAFLW_Lobby_Root::HandleWalletChanged(int32 InVolts, int32 InWatts)
{
	// ⚠ THE DELEGATE IS (Volts, Watts); SetWalletReadout IS (Watts, Volts). Swapped on purpose here rather
	// than by renaming either side -- both orders are load-bearing elsewhere, and on a wagering surface a
	// silent transposition shows a player the wrong currency's balance while looking entirely plausible.
	SetWalletReadout(InWatts, InVolts);
}

void UAFLW_Lobby_Root::HandleDirectoryUpdated(EAFLQueueDirectoryState DirectoryState)
{
	UAFLQueueDirectorySubsystem* Directory = UAFLQueueDirectorySubsystem::Get(this);
	if (!Directory)
	{
		return;
	}

	if (DirectoryState == EAFLQueueDirectoryState::Failed && Directory->GetQueues().Num() == 0)
	{
		// No ladder at all. The commit bar already states "No queue is open in this combination yet.", which
		// is true but incomplete -- so say which of the two it is. §7: "Population unavailable" and "nothing
		// is open" are different claims, and the one thing forbidden is presenting either as an empty lobby.
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_LOBBY: the queue directory could not be read -- no ladder to draw."));
	}

	SetQueueSet(Directory->GetQueues());
}

void UAFLW_Lobby_Root::NativeDestruct()
{
	// The directory is a GameInstanceSubsystem and outlives every lobby screen, so an unreleased handle is a
	// subscription that survives the widget. AddUObject would not call into a dead object, but the entry
	// stays on the invocation list for the life of the process, once per screen visit.
	if (DirectoryHandle.IsValid())
	{
		if (UAFLQueueDirectorySubsystem* Directory = UAFLQueueDirectorySubsystem::Get(this))
		{
			Directory->OnUpdated.Remove(DirectoryHandle);
		}
		DirectoryHandle.Reset();
	}
	if (PresenceHandle.IsValid())
	{
		if (UAFLQueueDirectorySubsystem* Directory = UAFLQueueDirectorySubsystem::Get(this))
		{
			Directory->OnPresenceUpdated.Remove(PresenceHandle);
		}
		PresenceHandle.Reset();
	}

	// The wallet lives on the PlayerState, which outlives this screen. RemoveDynamic is a no-op when the
	// binding was never made, so no bound-flag to keep in sync.
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const APlayerState* PS = PC->PlayerState)
		{
			if (UAFLWalletComponent* Wallet = PS->FindComponentByClass<UAFLWalletComponent>())
			{
				Wallet->OnWalletChanged.RemoveDynamic(this, &UAFLW_Lobby_Root::HandleWalletChanged);
			}
		}
	}

	Super::NativeDestruct();
}

void UAFLW_Lobby_Root::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Re-apply on every activation rather than only on construct: the published set is live product state,
	// and this screen is RETURNED to (from S4, from a cancelled queue, from a match) far more often than it
	// is built. A door that scoped itself once would show the shape it had on first visit.
	ApplyDoorScoping();

	// Re-resolve on every activation, not once: on the front end the PlayerState can arrive after this
	// screen is constructed, and a player returning from a match comes back to a different one.
	BindWallet();

	// Ask the directory for a fresh read on every activation. Population is live product state and this
	// screen is RETURNED to far more often than it is built -- a lobby showing the counts it had when the
	// player left for a match is the stale-estimate failure §3.4 spent three subsections closing.
	BindQueueDirectory();

	// Reconcile before rebuilding: a bracket that was open when this screen was last left can have closed
	// while the player was away, and league §7 rules that the selection moves to the first bracket still
	// open rather than sitting on a dead one.
	ReconcileSelection();
	FinishRefresh();
}

UWidget* UAFLW_Lobby_Root::NativeGetDesiredFocusTarget() const
{
	// `ui-frontend` §12.2: the default focus target is resolved AT RUNTIME, never a compile-time bind -- a
	// hard binding turns a layout edit into a compile break, and an optional one turns it into an
	// unfocusable screen on gamepad. So: the selected row if one exists, then the first selectable row,
	// then the CTA. Every fallback is an actionable control; focus never rests on a readout.
	for (const TObjectPtr<UAFLW_Lobby_QueueRow>& Row : SpawnedRows)
	{
		if (Row && Row->GetQueue().IsSelectable() && Row->GetQueue().Bracket == SelectedBracket)
		{
			return Row;
		}
	}
	for (const TObjectPtr<UAFLW_Lobby_QueueRow>& Row : SpawnedRows)
	{
		if (Row && Row->GetQueue().IsSelectable())
		{
			return Row;
		}
	}
	if (CommitButton)
	{
		return CommitButton;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UAFLW_Lobby_Root::BindAxisButton(UCommonButtonBase* Button, TFunction<void()> Handler)
{
	if (Button)
	{
		// AddWeakLambda, not AddLambda: the button outlives nothing here, but a raw lambda on a multicast
		// delegate has no lifetime tie to `this` at all, and that is the kind of thing that only fails
		// during a teardown nobody is watching.
		Button->OnClicked().AddWeakLambda(this, MoveTemp(Handler));
	}
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  INTAKE
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

void UAFLW_Lobby_Root::SetDoor(EAFLHomeDoor InDoor)
{
	if (Door == InDoor)
	{
		return;
	}
	Door = InDoor;
	EnforceDoorInvariants();

	ApplyDoorScoping();
	ReconcileSelection();
	FinishRefresh();
}

void UAFLW_Lobby_Root::SetQueueSet(const TArray<FAFLLobbyQueue>& InQueues)
{
	Queues = InQueues;

	ReconcileSelection();
	FinishRefresh();
}

void UAFLW_Lobby_Root::SetWalletReadout(int64 Watts, int64 Volts)
{
	WattsBalance = Watts;
	VoltsBalance = Volts;
	RefreshWalletChips();
	RefreshCommitState();
}

void UAFLW_Lobby_Root::SetOnlineCount(int32 Count)
{
	OnlineCount = Count;
	if (OnlineChip)
	{
		// Unknown renders EMPTY, not `0`. Zero online is a claim about the game being dead; not knowing is
		// a claim about us. The chip saying nothing is the only honest third option.
		OnlineChip->SetText(Count >= 0 ? FText::AsNumber(Count) : FText::GetEmpty());
	}
}

void UAFLW_Lobby_Root::SetStakeBand(const FText& BandLabel, bool bValueInSomeBand)
{
	StakeBandLabel = BandLabel;
	bStakeInSomeBand = bValueInSomeBand;

	if (BandReadout)
	{
		// §4.2: the band is stated IN THE SAME PLACE AS THE VALUE, never a tooltip -- "a qualification the
		// player has to go looking for is a qualification that was not made".
		BandReadout->SetText(bValueInSomeBand
			? BandLabel
			: LOCTEXT("OutsideAllBands", "outside all bands"));
	}

	// The band is on the rows too: it is the boundary the money enters at, so it belongs beside the queue
	// the money is going into rather than only beside the field it was typed in. Refreshed IN PLACE -- a
	// band arrives once per keystroke round-trip, and tearing the list down that often would restart the
	// §13 row motion on every character typed.
	RefreshRowsInPlace();
	RefreshCommitState();
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  SELECTION
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

void UAFLW_Lobby_Root::SelectRuleset(EAFLRuleset InRuleset)
{
	if (Ruleset == InRuleset)
	{
		return;
	}
	Ruleset = InRuleset;

	// R41 / handoff §3.2.1: switching the tab re-scopes the axis, the queue list AND the payout shape,
	// because all three are properties of the RULESET rather than of the screen. The bracket vocabularies
	// do not overlap -- Match Play team brackets never carry over to BR, which is a different position
	// model (league §7) -- so the previous bracket is guaranteed out of scope and gets reconciled away.
	ReconcileSelection();
	FinishRefresh();
}

void UAFLW_Lobby_Root::SelectLeague(EAFLLeague InLeague)
{
	if (!IsAxisLegalForDoor(Door, EAFLLobbyAxis::League))
	{
		// Reachable despite the panel being hidden: a hidden widget can still be driven by a gamepad path
		// or a Blueprint call, and R86 is a product rule rather than a presentation one. Refuse it here,
		// where it is authoritative -- the same argument UAFLW_HomeScreen::ChooseDoor makes.
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOBBY: league axis is not on the staked door (R86) -- refused."));
		return;
	}
	if (League == InLeague)
	{
		return;
	}
	League = InLeague;

	ReconcileSelection();
	FinishRefresh();
}

void UAFLW_Lobby_Root::SelectDenomination(EAFLDenomination InDenomination)
{
	if (!IsAxisLegalForDoor(Door, EAFLLobbyAxis::Denomination))
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOBBY: denomination axis is not on the league door (R98) -- refused."));
		return;
	}
	if (Denomination == InDenomination)
	{
		return;
	}
	Denomination = InDenomination;

	// §2: switching denomination RE-BASES EVERYTHING, and the entered amount does NOT carry across. 1,000 W
	// and 1,000 V are not the same bet, and carrying the number would imply they are. The band goes with it
	// -- a boundary resolved against the old ladder is not a fact about the new one.
	Stake = 0;
	StakeBandLabel = FText::GetEmpty();
	bStakeInSomeBand = true;
	if (StakeNumericField)
	{
		StakeNumericField->SetText(FText::GetEmpty());
	}
	if (BandReadout)
	{
		BandReadout->SetText(FText::GetEmpty());
	}

	ReconcileSelection();
	FinishRefresh();
}

void UAFLW_Lobby_Root::SelectVenueClass(EAFLVenueClass InVenue)
{
	if (Venue == InVenue)
	{
		return;
	}
	Venue = InVenue;

	ReconcileSelection();
	FinishRefresh();
}

void UAFLW_Lobby_Root::SelectBracket(const FString& InBracket)
{
	if (SelectedBracket == InBracket)
	{
		return;
	}
	SelectedBracket = InBracket;

	// ⚠ IN PLACE, NOT A REBUILD -- AND THIS IS A CORRECTNESS FIX, NOT AN OPTIMISATION.
	//
	// This function is called FROM a row's or a tile's own OnClicked delegate. Rebuilding would call
	// ClearChildren on the panel holding the very widget whose click is still on the stack, destroying its
	// Slate half mid-broadcast -- the classic destroy-the-widget-from-its-own-handler crash, and one that
	// only shows up under a real click rather than under a scripted SelectBracket call.
	//
	// Refreshing in place also keeps the two views honest: the size grid and the queue list are TWO VIEWS
	// OF ONE QUEUE (staked §5), and both are updated from this one mutation, so there is no path where one
	// of them thinks a different cell is chosen.
	RefreshRowsInPlace();
	RefreshCommitState();
	BroadcastSelection();
}

void UAFLW_Lobby_Root::SetStake(int32 InStake)
{
	if (!IsAxisLegalForDoor(Door, EAFLLobbyAxis::Stake))
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOBBY: a stake was set on the LEAGUE route -- refused (R98)."));
		return;
	}
	Stake = FMath::Max(0, InStake);

	// ⚠ NO BANDING HAPPENS HERE. R59 puts snapping on the server, and §3 of the staked spec is explicit:
	// "The UI displays; it never re-implements the rule." The value travels UNROUNDED; the band arrives
	// back through SetStakeBand. Until it does, the previous label is stale, so it is cleared rather than
	// left standing -- a boundary shown against the wrong amount is worse than no boundary at all.
	StakeBandLabel = FText::GetEmpty();
	if (BandReadout)
	{
		BandReadout->SetText(FText::GetEmpty());
	}

	// Ask the server what this amount matches. The reply lands in SetStakeBand.
	if (Stake > 0)
	{
		if (UAFLQueueDirectorySubsystem* Directory = UAFLQueueDirectorySubsystem::Get(this))
		{
			const int32 Requested = Stake;
			TWeakObjectPtr<UAFLW_Lobby_Root> WeakThis(this);
			Directory->ResolveBand(ResolveTier(Door, Denomination), Stake,
				FAFLOnBandResolved::CreateLambda([WeakThis, Requested](bool bOk, const FText& Label, bool bInSomeBand)
				{
					UAFLW_Lobby_Root* Self = WeakThis.Get();
					if (!Self || Self->Stake != Requested)
					{
						// ⚠ A REPLY FOR AN AMOUNT THE PLAYER HAS ALREADY TYPED PAST IS DISCARDED. Typing
						// "450" fires four requests and they can land out of order; without this check the
						// field could read 450 while the band underneath describes 45.
						return;
					}
					if (bOk)
					{
						Self->SetStakeBand(Label, bInSomeBand);
					}
				}));
		}
	}

	RefreshPresetSelection();
	RefreshCommitState();
}

void UAFLW_Lobby_Root::HandleStakeTextChanged(const FText& Text)
{
	// §7: non-integer and non-positive are rejected AT THE FIELD. The server rejects them too (E1) -- this
	// is convenience, not the guard.
	FString Digits;
	for (const TCHAR Ch : Text.ToString())
	{
		if (FChar::IsDigit(Ch))
		{
			Digits.AppendChar(Ch);
		}
	}
	SetStake(Digits.IsEmpty() ? 0 : FCString::Atoi(*Digits));
}

const FAFLLobbyQueue* UAFLW_Lobby_Root::GetSelectedQueue() const
{
	if (SelectedBracket.IsEmpty())
	{
		return nullptr;
	}
	const EAFLPlayTier Tier = ResolveTier(Door, Denomination);
	for (const FAFLLobbyQueue& Queue : Queues)
	{
		if (Queue.Tier == Tier && Queue.League == League && Queue.Ruleset == Ruleset
			&& Queue.Venue == Venue && Queue.Bracket == SelectedBracket)
		{
			return &Queue;
		}
	}
	return nullptr;
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  SCOPING AND REBUILD
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

void UAFLW_Lobby_Root::ApplyDoorScoping()
{
	const EAFLPlayTier Tier = ResolveTier(Door, Denomination);

	// COLLAPSED, not disabled. `LEAGUE_DOOR_SPEC.md` §1: absent, "not hidden, not zeroed, not disabled" --
	// and Collapsed is the UMG spelling of absent, because it takes no layout space either. A hidden-but-
	// laid-out stake panel would leave a gap shaped exactly like the control R98 removed.
	const auto ApplyAxis = [this](UWidget* Panel, EAFLLobbyAxis Axis)
	{
		if (Panel)
		{
			Panel->SetVisibility(IsAxisLegalForDoor(Door, Axis)
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
		}
	};

	ApplyAxis(LeagueAxisPanel,       EAFLLobbyAxis::League);
	ApplyAxis(DenominationAxisPanel, EAFLLobbyAxis::Denomination);
	ApplyAxis(StakeAxisPanel,        EAFLLobbyAxis::Stake);

	if (SizeAxisLabel)
	{
		// R99 / league §7: under BATTLE ROYALE the axis re-labels to FIELD, because BR is one-vs-everyone
		// and its position count IS the field size -- team brackets never appear there.
		SizeAxisLabel->SetText(Ruleset == EAFLRuleset::BattleRoyale
			? LOCTEXT("AxisField", "FIELD")
			: LOCTEXT("AxisFormat", "FORMAT"));
	}

	BP_OnDoorScoped(Door, Tier);
}

void UAFLW_Lobby_Root::CollectScopedQueues(TArray<const FAFLLobbyQueue*>& Out) const
{
	Out.Reset();

	const EAFLPlayTier Tier = ResolveTier(Door, Denomination);
	for (const FAFLLobbyQueue& Queue : Queues)
	{
		if (Queue.Tier == Tier && Queue.League == League && Queue.Ruleset == Ruleset && Queue.Venue == Venue)
		{
			Out.Add(&Queue);
		}
	}

	Out.Sort([](const FAFLLobbyQueue& A, const FAFLLobbyQueue& B)
	{
		// Handoff §3.3: SORTED BY POPULATION, DESCENDING -- an informed player self-selects toward the
		// populated option, which shortens waits and self-reinforces.
		//
		// Two refinements the rule does not state because it predates the six readings:
		//
		//  1. NotOpen sinks to the bottom. It is not a queue yet (R63), so it has no population to rank by,
		//     and interleaving unopened brackets through a population order would put a dead entry above a
		//     live one purely because of where it sits in the ladder.
		//  2. An unknown count sorts BELOW a known one rather than as zero -- INDEX_NONE does that for free.
		//     Ranking "we could not read it" as "nobody is here" is the collapse §3.2 forbids, and a sort
		//     order is just as much a claim as a label.
		const bool bAOpen = A.State != EAFLPopulationState::NotOpen;
		const bool bBOpen = B.State != EAFLPopulationState::NotOpen;
		if (bAOpen != bBOpen)
		{
			return bAOpen;
		}
		if (A.PlayersMatching != B.PlayersMatching)
		{
			return A.PlayersMatching > B.PlayersMatching;
		}
		// Stable, readable tie-break: the ladder's own order. Two equally-populated brackets should not
		// swap places between refreshes.
		return A.Slots < B.Slots;
	});
}

void UAFLW_Lobby_Root::ReconcileSelection()
{
	TArray<const FAFLLobbyQueue*> Scoped;
	CollectScopedQueues(Scoped);

	// "Selection lands on a bracket that closes: the selection moves to the first bracket that is still
	// open, and to `none open yet` if there is none. POPULATION ARRIVING AFTER FIRST PAINT MUST NEVER
	// SILENTLY MOVE WHAT THE PLAYER PICKED." (league §7) -- so a still-open selection is KEPT even when a
	// refresh reorders the list around it.
	if (!SelectedBracket.IsEmpty())
	{
		for (const FAFLLobbyQueue* Queue : Scoped)
		{
			if (Queue->Bracket == SelectedBracket && Queue->IsSelectable())
			{
				return;
			}
		}
	}

	// ⚠ THE BROADCAST IS DEFERRED, NOT FIRED HERE. Every caller reconciles BEFORE it rebuilds, so
	// broadcasting from inside this function would hand a listener a selection whose row does not exist
	// yet -- and the S2 panel is exactly such a listener. The flag is consumed by FinishRefresh().
	for (const FAFLLobbyQueue* Queue : Scoped)
	{
		if (Queue->IsSelectable())
		{
			SelectedBracket = Queue->Bracket;
			bSelectionChangedPending = true;
			return;
		}
	}

	if (!SelectedBracket.IsEmpty())
	{
		SelectedBracket.Reset();   // nothing open in this scope -- the CTA states why
		bSelectionChangedPending = true;
	}
}

void UAFLW_Lobby_Root::FinishRefresh()
{
	RebuildAxisOptions();
	RebuildQueueList();
	RefreshAxisSelection();
	RefreshCommitState();

	// UNCONDITIONAL, and deliberately not inside the bSelectionChangedPending branch below. On the staked
	// door there are no published cells, so a selection never CHANGES -- there was never one to change from
	// -- and anything hung off that flag simply never runs. That is precisely how the panel ended up
	// showing its authored "Text Block" strings. The empty state has to be driven by every refresh, not by
	// a transition that does not happen.
	if (QueueDetail && !GetSelectedQueue())
	{
		QueueDetail->ShowNoSelection(LOCTEXT("DetailNothingOpen", "Nothing is open in this combination yet"));
	}

	if (bSelectionChangedPending)
	{
		bSelectionChangedPending = false;
		BroadcastSelection();
	}
}

void UAFLW_Lobby_Root::EnforceDoorInvariants()
{
	if (Door == EAFLHomeDoor::Staked)
	{
		// R86, enforced rather than assumed: there is no staked Haywire cell in the registry at all, so a
		// Haywire selection on this door scopes the list to nothing and reads as an empty lobby rather than
		// as an illegal combination.
		//
		// ⚠ THIS MUST RUN AT INITIALISATION, NOT ONLY ON A DOOR TRANSITION -- and that distinction was the
		// whole staked bug. WBP_IRONICS_Lobby_Staked carries Door=Staked in its class defaults (that is how
		// one chassis serves two doors), so SetDoor(Staked) hit its `Door == InDoor` early return, this
		// correction never ran, and League stayed at its Haywire member default. Every staked cell is
		// ProMod, so the filter matched ZERO of the 20 published staked queues, the list came up empty, no
		// selection was ever made, and the detail panel kept its authored "Text Block" strings. A door
		// invariant applied only on the transition is not an invariant.
		League = EAFLLeague::ProMod;
	}
	else
	{
		// The league door has no buy-in, so a retained amount would be a wagering value living on the free
		// side of R98. Cleared rather than kept-and-zeroed.
		Stake = 0;
		StakeBandLabel = FText::GetEmpty();
		bStakeInSomeBand = true;
	}
}

void UAFLW_Lobby_Root::RefreshAxisSelection()
{
	// THE AXIS BUTTONS NEVER SHOWED WHICH OPTION WAS CHOSEN. Only spawned rows, tiles and presets were
	// given SetIsSelected; the fixed axis buttons were bound to their handlers and then never told
	// anything. Clicking HAYWIRE genuinely ran SelectLeague and changed the state -- it just left the
	// button looking exactly as it did before, which is indistinguishable from a dead button.
	//
	// RULESET and VENUE CLASS escaped the report only because changing them visibly rewrites the format
	// row underneath, so the feedback arrived by side effect. LEAGUE changes nothing on screen when both
	// leagues publish the same brackets, so it looked broken while working.
	const auto Mark = [](UCommonButtonBase* Button, bool bSelected)
	{
		if (!Button)
		{
			return;
		}

		// ⚠ SetIsSelected IS A SILENT NO-OP UNLESS THE BUTTON IS SELECTABLE. CommonUI guards it with
		// `if (bSelectable && bSelected != InSelected)` and returns without a warning otherwise, so an axis
		// button authored with bSelectable off swallows every call and keeps looking untouched. That is why
		// the first attempt at this fix changed nothing on screen: the state was correct, the calls were
		// made, and CommonUI dropped them. Assert selectable here rather than relying on each WBP to
		// remember -- these buttons ARE the selection indicator for their axis.
		Button->SetIsSelectable(true);
		Button->SetIsSelected(bSelected);
	};

	Mark(MatchPlayTab,    Ruleset == EAFLRuleset::MatchPlay);
	Mark(BattleRoyaleTab, Ruleset == EAFLRuleset::BattleRoyale);
	Mark(HaywireButton,   League == EAFLLeague::Haywire);
	Mark(ProModButton,    League == EAFLLeague::ProMod);
	Mark(WattsButton,     Denomination == EAFLDenomination::Watts);
	Mark(VoltsButton,     Denomination == EAFLDenomination::Volts);
	Mark(ArenaButton,     Venue == EAFLVenueClass::Arena);
	Mark(MapButton,       Venue == EAFLVenueClass::Map);
}

void UAFLW_Lobby_Root::RefreshRowsInPlace()
{
	// Same cells, same widgets, new band and new selection. Used on the paths that CANNOT tear the list
	// down -- a click handler running inside one of these very widgets (SelectBracket), and the
	// per-keystroke band round-trip.
	const auto Restate = [this](UAFLW_Lobby_QueueRow* Widget)
	{
		if (!Widget)
		{
			return;
		}
		// COPY first. SetQueue assigns into the member this reference points at, so passing GetQueue()
		// straight through would be a self-assignment -- survivable today, and exactly the sort of thing
		// that stops being survivable when someone adds a clear-then-fill to the setter.
		const FAFLLobbyQueue Cell = Widget->GetQueue();
		Widget->SetQueue(Cell, StakeBandLabel);
		Widget->SetIsSelected(Cell.Bracket == SelectedBracket);
	};

	for (const TObjectPtr<UAFLW_Lobby_QueueRow>& Row : SpawnedRows)  { Restate(Row); }
	for (const TObjectPtr<UAFLW_Lobby_QueueRow>& Tile : SpawnedTiles) { Restate(Tile); }
}

void UAFLW_Lobby_Root::RefreshPresetSelection()
{
	// The presets and the numeric field are two ways of saying the same number, so a typed value must light
	// the matching rung and a rung must fill the field. Kept separate from RefreshRowsInPlace because the
	// stake changes on a different beat from the selection.
	for (int32 Index = 0; Index < SpawnedPresets.Num(); ++Index)
	{
		if (SpawnedPresets[Index] && SpawnedPresetRungs.IsValidIndex(Index))
		{
			SpawnedPresets[Index]->SetIsSelected(SpawnedPresetRungs[Index] == Stake);
		}
	}
}

void UAFLW_Lobby_Root::RebuildAxisOptions()
{
	if (!SizeAxisBox || !SizeTileClass)
	{
		return;
	}

	SizeAxisBox->ClearChildren();
	SpawnedTiles.Reset();

	TArray<const FAFLLobbyQueue*> Scoped;
	CollectScopedQueues(Scoped);

	// ⚠ THE LADDER IS DRAWN IN LADDER ORDER, NOT POPULATION ORDER. The list is the surface population sorts
	// (§3.3); the axis is the designed ladder, and a ladder whose rungs reorder themselves as people arrive
	// is unusable as a ladder. Same cells, two orderings, on purpose.
	Scoped.Sort([](const FAFLLobbyQueue& A, const FAFLLobbyQueue& B) { return A.Slots < B.Slots; });

	for (const FAFLLobbyQueue* Queue : Scoped)
	{
		UAFLW_Lobby_QueueRow* Tile = CreateWidget<UAFLW_Lobby_QueueRow>(this, SizeTileClass);
		if (!Tile)
		{
			continue;
		}
		// Options are READ OFF THE LIVE QUEUES, never authored as a list (handoff §3.2.1). An unopened
		// bracket still gets a tile -- disabled, at 42%, reading `Not open yet` -- because the whole
		// designed ladder is information: a player should be able to see what the game intends to offer
		// and which of it has actually shipped. Hiding it would be honest but mute (league §3.2).
		Tile->SetQueue(*Queue, StakeBandLabel);
		Tile->SetIsSelected(Queue->Bracket == SelectedBracket);

		const FString Bracket = Queue->Bracket;
		Tile->OnClicked().AddWeakLambda(this, [this, Bracket] { SelectBracket(Bracket); });

		SizeAxisBox->AddChild(Tile);
		SpawnedTiles.Add(Tile);
	}

	// The stake ladder is the other half of §5's "looked up once and rendered twice". Rungs come from the
	// cell the server published, never from a list authored here -- §16.1 flags the preset values as
	// placeholders and says explicitly: DO NOT HARDEN THEM.
	if (StakePresetBox && StakePresetClass && IsAxisLegalForDoor(Door, EAFLLobbyAxis::Stake))
	{
		StakePresetBox->ClearChildren();
		SpawnedPresets.Reset();
		SpawnedPresetRungs.Reset();

		// Note the empty-array fallback is a NAMED LOCAL, not a temporary bound to a reference -- binding a
		// `TArray<int32>()` temporary to `const TArray<int32>&` through a ternary leaves a dangling
		// reference the moment the full expression ends.
		static const TArray<int32> NoRungs;
		const FAFLLobbyQueue* Selected = GetSelectedQueue();
		const TArray<int32>& Rungs = Selected ? Selected->StakeRungs
		                                      : (Scoped.Num() ? Scoped[0]->StakeRungs : NoRungs);
		for (const int32 Rung : Rungs)
		{
			UCommonButtonBase* Preset = CreateWidget<UCommonButtonBase>(this, StakePresetClass);
			if (!Preset)
			{
				continue;
			}
			Preset->SetIsSelected(Rung == Stake);
			Preset->OnClicked().AddWeakLambda(this, [this, Rung] { SetStake(Rung); });
			StakePresetBox->AddChild(Preset);
			SpawnedPresets.Add(Preset);
			SpawnedPresetRungs.Add(Rung);
		}
	}
}

void UAFLW_Lobby_Root::RebuildQueueList()
{
	if (!QueueListBox || !QueueRowClass)
	{
		return;
	}

	QueueListBox->ClearChildren();
	SpawnedRows.Reset();

	TArray<const FAFLLobbyQueue*> Scoped;
	CollectScopedQueues(Scoped);

	int32 OpenCount = 0;
	int32 ColdCount = 0;
	for (const FAFLLobbyQueue* Queue : Scoped)
	{
		UAFLW_Lobby_QueueRow* Row = CreateWidget<UAFLW_Lobby_QueueRow>(this, QueueRowClass);
		if (!Row)
		{
			continue;
		}
		Row->SetQueue(*Queue, StakeBandLabel);
		Row->SetIsSelected(Queue->Bracket == SelectedBracket);

		const FString Bracket = Queue->Bracket;
		Row->OnClicked().AddWeakLambda(this, [this, Bracket] { SelectBracket(Bracket); });

		QueueListBox->AddChild(Row);
		SpawnedRows.Add(Row);

		if (Queue->IsSelectable()) { ++OpenCount; }
		if (Queue->State == EAFLPopulationState::Cold) { ++ColdCount; }
	}

	if (ListFooter)
	{
		if (OpenCount == 0)
		{
			// Distinct from all-cold, and the difference matters: this is a statement about CONTENT, not
			// about population. Nothing here has a map behind it yet (R63).
			ListFooter->SetText(LOCTEXT("NothingOpen", "Nothing is open in this combination yet."));
		}
		else if (ColdCount == OpenCount)
		{
			// Handoff §12. Every row still renders with an honest count and stays selectable -- rows are
			// NEVER hidden, because a queue that accepts a player and never returns is indistinguishable
			// from a broken one.
			ListFooter->SetText(LOCTEXT("AllQuiet", "Queues are quiet right now."));
		}
		else
		{
			ListFooter->SetText(FText::Format(
				LOCTEXT("ListFooter", "{0} queues · sorted by population"), FText::AsNumber(Scoped.Num())));
		}
	}

	BP_OnQueuesRebuilt(OpenCount, Scoped.Num());
}

void UAFLW_Lobby_Root::RefreshWalletChips()
{
	// Skeleton, NEVER `0` (staked §6, home §8). A wrong balance is worse than no balance on a surface that
	// leads to a wager, so an unknown balance renders empty and the WBP shows its shimmer over the top.
	if (WattsChip)
	{
		WattsChip->SetText(WattsBalance >= 0 ? FText::AsNumber(WattsBalance) : FText::GetEmpty());
	}
	if (VoltsChip)
	{
		VoltsChip->SetText(VoltsBalance >= 0 ? FText::AsNumber(VoltsBalance) : FText::GetEmpty());
	}
}

void UAFLW_Lobby_Root::RefreshCommitState()
{
	const FAFLLobbyQueue* Selected = GetSelectedQueue();

	bool bEnabled = true;
	FText Reason;

	if (!Selected)
	{
		bEnabled = false;
		Reason = LOCTEXT("CommitNoQueue", "No queue is open in this combination yet.");
	}
	else if (!Selected->IsSelectable())
	{
		// "Nothing in the ruleset is open: Find match is DISABLED -- there is no queue to enter, so an
		// enabled button would be an offer the game cannot honour." (league §7)
		bEnabled = false;
		Reason = LOCTEXT("CommitNotOpen", "Not open yet.");
	}
	else if (IsAxisLegalForDoor(Door, EAFLLobbyAxis::Stake))
	{
		const int64 Balance = (Denomination == EAFLDenomination::Volts) ? VoltsBalance : WattsBalance;

		if (Stake <= 0)
		{
			bEnabled = false;
			Reason = LOCTEXT("CommitNoStake", "Choose a stake.");
		}
		else if (!bStakeInSomeBand)
		{
			bEnabled = false;
			Reason = FText::Format(LOCTEXT("CommitOutOfBand", "{0} matches no band."), FormatStakeWithSuffix(Stake));
		}
		else if (Balance < 0)
		{
			// Balance UNKNOWN -- disabled until it is known (staked §6). Not the same as insufficient, and
			// the reason says so, because "we do not know yet" and "you cannot afford it" are different
			// things to tell someone about their own money.
			bEnabled = false;
			Reason = LOCTEXT("CommitBalanceUnknown", "Balance unavailable.");
		}
		else if (Balance < Stake)
		{
			// ⚠ COURTESY ONLY. `STAKED_DOOR_SPEC.md` §6: the server's 402 is the real gate. This exists so
			// the shortfall is stated inline instead of arriving as a rejection.
			bEnabled = false;
			Reason = FText::Format(LOCTEXT("CommitShortfall", "You need {0} more."),
				FormatStakeWithSuffix(static_cast<int32>(Stake - Balance)));
		}
	}

	if (CommitButton)
	{
		CommitButton->SetIsEnabled(bEnabled);
	}
	if (CommitReason)
	{
		// §10: a disabled CTA is NEVER a silent no-op -- 40% opacity plus a reason string beneath it.
		CommitReason->SetText(Reason);
		CommitReason->SetVisibility(Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (CommitSummary && Selected)
	{
		// "The remembered selection is STATED IN THE ACTION, never implied by it" (§7). The summary carries
		// the bracket and, on the staked route, the amount -- so nobody presses a stake they have not read.
		CommitSummary->SetText(IsAxisLegalForDoor(Door, EAFLLobbyAxis::Stake)
			? FText::Format(LOCTEXT("CommitSummaryStaked", "QUEUE · {0} · {1}"),
				FText::FromString(Selected->Bracket), FormatStakeWithSuffix(Stake))
			: FText::Format(LOCTEXT("CommitSummaryLeague", "FIND MATCH · {0}"),
				FText::FromString(Selected->Bracket)));
	}

	BP_OnCommitStateChanged(bEnabled, Reason);
}

void UAFLW_Lobby_Root::BroadcastSelection()
{
	const FAFLLobbyQueue* Selected = GetSelectedQueue();
	if (!Selected)
	{
		// DO NOT just return. The panel is not an observer that can be left alone -- it is region D's own
		// column, and a column nobody writes to shows whatever the WBP was authored with. That is why the
		// staked door read "Text Block" in every field (no published cells, so no selection is ever
		// reached), and why switching the league door to BATTLE ROYALE left the previous MATCH PLAY
		// figures standing over a list that said "Not open yet".
		if (QueueDetail)
		{
			QueueDetail->ShowNoSelection(LOCTEXT("DetailNothingOpen", "Nothing is open in this combination yet"));
		}
		return;
	}

	// S2 is fed HERE rather than by a listener on OnSelectionChanged: the panel is region D's own right
	// column, not an outside observer, and routing it through the public delegate would mean the screen's
	// two halves could disagree if anything ever unsubscribed. The delegate stays for genuine outsiders.
	if (QueueDetail)
	{
		// Stake is zero on the league route by construction (IsAxisLegalForDoor refuses to set one), so the
		// panel's ladder correctly falls to a no-buy-in preview rather than a pool of nothing.
		QueueDetail->SetQueue(*Selected, Stake, StakeBandLabel);
	}

	OnSelectionChanged.Broadcast(*Selected);
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  COMMIT
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

bool UAFLW_Lobby_Root::RequiresTicketReview() const
{
	// R22, handoff §6. S4 exists BECAUSE of R23: showing potential winnings beside a stake drives
	// engagement, which is exactly why the limits ship with it rather than after it. Both guardrails -- the
	// per-entry cap and the session meter -- must be legible BEFORE they bind, so a staked entry cannot
	// skip the screen that shows them. The league route has neither, because it has no stake.
	return AFLLobby::IsStaked(ResolveTier(Door, Denomination));
}

void UAFLW_Lobby_Root::CommitQueue()
{
	const FAFLLobbyQueue* Selected = GetSelectedQueue();
	if (!Selected || !Selected->IsSelectable())
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOBBY: commit with no open queue selected -- refused."));
		return;
	}

	if (RequiresTicketReview())
	{
		// Queue NOTHING. R22 is explicit that re-queue is a shortcut through NAVIGATION, never through a
		// check -- so this raises S4 and stops. S4 is owed; wiring the commit straight to matchmaking here
		// would be the exact skip the rule forbids, and it would be invisible until a player hit a cap.
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOBBY: staked entry %s at %d -- routing to ticket review (R22)."),
			*Selected->QueueId, Stake);
		OnTicketReviewRequested.Broadcast(Selected->QueueId, Stake);
		PushTicketReview(*Selected);
		return;
	}

	// LEAGUE PLAY. Stake is zero by construction on this route -- IsAxisLegalForDoor refuses to set one --
	// and the server treats a stake on an unstaked tier as a 400 rather than a courtesy.
	UE_LOG(LogAFLCombat, Log, TEXT("AFL_LOBBY: entering %s (league, no buy-in)."), *Selected->QueueId);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UAFLMatchmakingSubsystem* Matchmaking = GameInstance->GetSubsystem<UAFLMatchmakingSubsystem>())
		{
			Matchmaking->StartMatchmaking(Selected->QueueId, 0);
		}
	}

	OnQueueCommitted.Broadcast(Selected->QueueId, 0);
}

void UAFLW_Lobby_Root::PushTicketReview(const FAFLLobbyQueue& Queue)
{
	if (TicketReviewClass.IsNull())
	{
		// Not an error: a WBP owning navigation leaves this empty and handles OnTicketReviewRequested.
		UE_LOG(LogAFLCombat, Verbose,
			TEXT("AFL_LOBBY: no ticket review class set -- S4 navigation left to the WBP."));
		return;
	}

	UClass* ScreenClass = TicketReviewClass.LoadSynchronous();
	APlayerController* PC = GetOwningPlayer();
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!ScreenClass || !LocalPlayer)
	{
		// ⚠ LOUD, because the alternative is a CTA that appears to do nothing. The lobby has already
		// refused to queue -- R22 -- so if S4 does not appear the player has pressed the only button on
		// the screen and got silence, which is the exact state the states table forbids.
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_LOBBY: cannot push ticket review (class=%s player=%s) -- staked entry dead-ends."),
			ScreenClass ? TEXT("ok") : TEXT("FAILED"), LocalPlayer ? TEXT("ok") : TEXT("none"));
		return;
	}

	UCommonActivatableWidget* Pushed =
		UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, TAG_UI_Layer_Menu_Lobby, ScreenClass);

	// The push returns the instance, which is the only chance to hand it the ticket -- S4 deliberately owns
	// no lookup back into the lobby. It confirms what it was GIVEN, so what the player reviews cannot
	// diverge from what they selected while the screen was open.
	if (UAFLW_TicketReview* Review = Cast<UAFLW_TicketReview>(Pushed))
	{
		// The bracket id IS the label -- `5v5`, `BR_36` (R99). Prettified nowhere, for the reason
		// AFLW_Lobby_QueueRow::FormatBracket gives: a second spelling of a bracket is a second place it
		// can drift from the registry.
		const FText Title = FText::Format(LOCTEXT("S4QueueLine", "{0} · {1}"),
			FText::FromString(Queue.Bracket), UEnum::GetDisplayValueAsText(Queue.Ruleset));
		Review->SetTicket(Queue.QueueId, Stake, Denomination == EAFLDenomination::Volts,
			Title, StakeBandLabel);
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning,
			TEXT("AFL_LOBBY: pushed '%s' for S4 but it is not a UAFLW_TicketReview -- it will show no ticket."),
			*ScreenClass->GetName());
	}
}

// ══════════════════════════════════════════════════════════════════════════════════════════════════════
//  FORMATTING
// ══════════════════════════════════════════════════════════════════════════════════════════════════════

const TCHAR* UAFLW_Lobby_Root::CurrencySuffix() const
{
	// Copy law (`IRONICS_UI_STYLE_SSOT.md` §5, handoff §0): Volts and Watts, integers only.
	// **NEVER USD, ANYWHERE, EVER** -- the single hardest 1:1 break from the PokerStars reference.
	if (Door == EAFLHomeDoor::League)
	{
		return TEXT("");
	}
	return Denomination == EAFLDenomination::Volts ? TEXT("V") : TEXT("W");
}

FText UAFLW_Lobby_Root::FormatStakeWithSuffix(int32 Amount) const
{
	const FString Suffix = CurrencySuffix();
	if (Suffix.IsEmpty())
	{
		return FText::AsNumber(Amount);
	}
	return FText::Format(LOCTEXT("StakeWithSuffix", "{0} {1}"), FText::AsNumber(Amount), FText::FromString(Suffix));
}

#undef LOCTEXT_NAMESPACE
