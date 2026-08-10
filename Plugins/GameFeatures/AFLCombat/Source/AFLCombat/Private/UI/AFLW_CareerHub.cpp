// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLW_CareerHub.h"

#include "AFLCombat.h"              // LogAFLCombat
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Containers/Ticker.h"
#include "CommonActivatableWidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLW_CareerHub)

#define LOCTEXT_NAMESPACE "AFLCareerHub"

/**
 * One tab row. Member pointers for the same reason `FAFLNavRoute` uses them: a tab is an identity, a
 * button and a destination, and keeping those in three lists is how they drift. One row, three readers.
 */
struct FAFLCareerTabRow
{
	EAFLCareerTab Tab;
	const TCHAR*  Id;
	TObjectPtr<UCommonButtonBase>           UAFLW_CareerHub::*Button;
	TSoftClassPtr<UCommonActivatableWidget> UAFLW_CareerHub::*Content;
};

namespace
{
	/** `afl.Career.Tab <volume|rank|replays>` -- the headless way to prove a tab actually shows something. */
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLCareerTabCmd(
		TEXT("afl.Career.Tab"),
		TEXT("Select a Career hub tab as if clicked: afl.Career.Tab [volume|rank|replays]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				const FName TabId(Args.Num() > 0 ? *Args[0].ToLower() : TEXT("replays"));
				TWeakObjectPtr<UWorld> WeakWorld(World);
				double Deadline = 25.0;

				// Waits for the screen, like every other probe here: -ExecCmds fires at engine init, long
				// before the front end has built anything, and a headless session has no mouse.
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
					[TabId, WeakWorld, Deadline](float Delta) mutable -> bool
					{
						Deadline -= Delta;
						UWorld* W = WeakWorld.Get();
						if (!W || Deadline <= 0.0)
						{
							UE_LOG(LogAFLCombat, Error, TEXT("AFL_CAREER: afl.Career.Tab gave up -- no hub."));
							return false;
						}
						for (TObjectIterator<UAFLW_CareerHub> It; It; ++It)
						{
							UAFLW_CareerHub* Hub = *It;
							if (Hub && Hub->GetWorld() == W && !Hub->HasAnyFlags(RF_ClassDefaultObject))
							{
								Hub->SelectTabById(TabId);
								return false;
							}
						}
						return true;
					}), 0.5f);

				Ar.Logf(TEXT("afl.Career.Tab -- will select '%s' once the hub exists."), *TabId.ToString());
			}));
}

TArrayView<const FAFLCareerTabRow> UAFLW_CareerHub::GetTabRows()
{
	// In the order they are drawn. Volume before Rank because volume is the permanent axis and the one a
	// player has unconditionally earned; rank is the one that can have moved against them since last time.
	static const FAFLCareerTabRow Rows[] = {
		{ EAFLCareerTab::Volume,  TEXT("volume"),  &UAFLW_CareerHub::Tab_Volume,  &UAFLW_CareerHub::VolumeTabClass  },
		{ EAFLCareerTab::Rank,    TEXT("rank"),    &UAFLW_CareerHub::Tab_Rank,    &UAFLW_CareerHub::RankTabClass    },
		{ EAFLCareerTab::Replays, TEXT("replays"), &UAFLW_CareerHub::Tab_Replays, &UAFLW_CareerHub::ReplaysTabClass },
	};
	return MakeArrayView(Rows);
}

const FAFLCareerTabRow* UAFLW_CareerHub::FindTabRow(EAFLCareerTab Tab)
{
	for (const FAFLCareerTabRow& Row : GetTabRows())
	{
		if (Row.Tab == Tab)
		{
			return &Row;
		}
	}
	return nullptr;
}

FName UAFLW_CareerHub::GetTabId(EAFLCareerTab Tab)
{
	const FAFLCareerTabRow* Row = FindTabRow(Tab);
	return Row ? FName(Row->Id) : NAME_None;
}

bool UAFLW_CareerHub::TryParseTab(FName TabId, EAFLCareerTab& OutTab)
{
	for (const FAFLCareerTabRow& Row : GetTabRows())
	{
		if (TabId == FName(Row.Id))
		{
			OutTab = Row.Tab;
			return true;
		}
	}
	return false;
}

bool UAFLW_CareerHub::IsTabAvailable(EAFLCareerTab Tab) const
{
	const FAFLCareerTabRow* Row = FindTabRow(Tab);
	return Row && !(this->*(Row->Content)).IsNull();
}

EAFLCareerTab UAFLW_CareerHub::ResolveLandingTab() const
{
	// REPLAYS FIRST, and not because it is alphabetically convenient: it is the only tab with content
	// today, and landing on an empty one would make the hub look broken on the very first open. When the
	// other two ship, the preference below is the thing to revisit -- Volume is the likelier landing for a
	// finished Career surface, since it is the axis that only ever goes up.
	for (const EAFLCareerTab Preferred : { EAFLCareerTab::Replays, EAFLCareerTab::Volume, EAFLCareerTab::Rank })
	{
		if (IsTabAvailable(Preferred))
		{
			return Preferred;
		}
	}
	// Nothing is built. Return the ruled resident rather than an arbitrary value, so the empty-state
	// message names the tab a player expects to find here.
	return EAFLCareerTab::Replays;
}

void UAFLW_CareerHub::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ApplyTabAvailability();
}

void UAFLW_CareerHub::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Re-applied per activation for the same reason the footer is: a destination can be configured between
	// two visits, and a hub that only ever reads its classes once would need a relaunch to notice.
	ApplyTabAvailability();
	SelectTab(ResolveLandingTab());
}

UWidget* UAFLW_CareerHub::NativeGetDesiredFocusTarget() const
{
	// Focus the tab that is actually showing, not the first in the row -- landing focus on a disabled
	// control is how a controller player arrives somewhere they cannot move from.
	const FAFLCareerTabRow* Row = FindTabRow(ActiveTab);
	if (Row)
	{
		if (UCommonButtonBase* Button = const_cast<UAFLW_CareerHub*>(this)->*(Row->Button))
		{
			return Button;
		}
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UAFLW_CareerHub::ApplyTabAvailability()
{
	for (const FAFLCareerTabRow& Row : GetTabRows())
	{
		UCommonButtonBase* Button = this->*(Row.Button);
		if (!Button)
		{
			continue;   // BindWidgetOptional: a WBP need not author every tab
		}

		// RemoveAll first: this runs on every activation, and a duplicate binding would select the tab N
		// times per click -- harmless here but the same defect that would push a screen N times elsewhere.
		const EAFLCareerTab Tab = Row.Tab;
		Button->OnClicked().RemoveAll(this);
		Button->OnClicked().AddWeakLambda(this, [this, Tab] { SelectTab(Tab); });

		// ⚠ DISABLED, NOT HIDDEN -- the footer's rule and the staked door's rule. A player should be able
		// to see that Volume and Rank are coming; hiding them would make the tab row silently change shape
		// as surfaces ship, and a row that grows is harder to trust than one that fills in.
		Button->SetIsInteractionEnabled(IsTabAvailable(Row.Tab));
	}
}

UCommonActivatableWidget* UAFLW_CareerHub::EnsureTabContent(EAFLCareerTab Tab)
{
	const uint8 Key = static_cast<uint8>(Tab);
	if (TObjectPtr<UCommonActivatableWidget>* Existing = HostedContent.Find(Key))
	{
		return *Existing;   // built once; re-selecting a tab must not rebuild its state
	}

	const FAFLCareerTabRow* Row = FindTabRow(Tab);
	if (!Row || (this->*(Row->Content)).IsNull() || !ContentSwitcher)
	{
		return nullptr;
	}

	// Synchronous at the moment of the click, like every other soft load in this front end: the soft
	// reference exists so opening Career does not pay for all three tabs, and by here the player has
	// committed to one.
	UClass* ContentClass = (this->*(Row->Content)).LoadSynchronous();
	if (!ContentClass)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_CAREER: tab '%s' class failed to load."), Row->Id);
		return nullptr;
	}

	// Constructed through the WidgetTree, not NewObject: a widget created outside the tree has no owning
	// player and its own BindWidget/CommonUI wiring never runs -- it would render and do nothing.
	UCommonActivatableWidget* Content =
		WidgetTree ? WidgetTree->ConstructWidget<UCommonActivatableWidget>(ContentClass) : nullptr;
	if (!Content)
	{
		UE_LOG(LogAFLCombat, Error, TEXT("AFL_CAREER: could not construct content for tab '%s'."), Row->Id);
		return nullptr;
	}

	ContentSwitcher->AddChild(Content);
	HostedContent.Add(Key, Content);
	return Content;
}

void UAFLW_CareerHub::SelectTab(EAFLCareerTab Tab)
{
	const FAFLCareerTabRow* Row = FindTabRow(Tab);
	if (!Row)
	{
		// Only reachable if a value was added to EAFLCareerTab without a row -- the case the table exists
		// to make loud. Error, not warning: a code defect, not a state.
		UE_LOG(LogAFLCombat, Error,
			TEXT("AFL_CAREER: tab %d has no row -- add one to GetTabRows()."), (int32)Tab);
		return;
	}

	UCommonActivatableWidget* Content = EnsureTabContent(Tab);
	if (!Content)
	{
		// Re-checked here as well as visually disabled, for the reason every other surface in this front
		// end re-checks: SetIsInteractionEnabled is presentation, and a gamepad or accessibility path can
		// still deliver the click.
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_CAREER: tab '%s' has no content yet -- refused."), Row->Id);
		if (EmptyTabReason)
		{
			EmptyTabReason->SetText(FText::Format(
				LOCTEXT("TabUnbuilt", "{0} is not built yet."), FText::FromString(FString(Row->Id).ToUpper())));
			EmptyTabReason->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		return;
	}

	if (EmptyTabReason)
	{
		EmptyTabReason->SetVisibility(ESlateVisibility::Collapsed);
	}

	ActiveTab = Tab;
	ContentSwitcher->SetActiveWidget(Content);

	UE_LOG(LogAFLCombat, Log, TEXT("AFL_CAREER: showing '%s' (%s)."), Row->Id, *Content->GetClass()->GetName());
	BP_OnTabChanged(Tab);
}

void UAFLW_CareerHub::SelectTabById(FName TabId)
{
	EAFLCareerTab Tab = EAFLCareerTab::Replays;
	if (!TryParseTab(TabId, Tab))
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_CAREER: unknown tab id '%s'."), *TabId.ToString());
		return;
	}
	SelectTab(Tab);
}

#undef LOCTEXT_NAMESPACE
