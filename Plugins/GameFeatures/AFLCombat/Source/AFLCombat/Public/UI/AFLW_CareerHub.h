// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Containers/ArrayView.h"

#include "AFLW_CareerHub.generated.h"

class UCommonActivatableWidgetSwitcher;
class UCommonButtonBase;
class UCommonTextBlock;
class UPanelWidget;

/**
 * The Career hub's tabs.
 *
 * ⚠ THE FIRST TWO ARE SEPARATE BECAUSE R10 SAYS THEY MUST BE, not because two tabs looked better than
 * one. `league-play.md` R10: "Cumulative volume and skill rating are separate axes and must never be
 * conflated: volume rewards attendance, rating measures strength, and matchmaking consumes rating only."
 * A single "progress" surface mixing a permanent elimination count with a rating that falls is exactly
 * the conflation that ruling forbids -- and it would read to a player as their career going backwards,
 * when §9.1 promises the opposite ("you cannot un-play a match, so a record of matches played cannot
 * honestly go backwards").
 *
 * REPLAYS is here by ruling (2026-08-10): denied a sixth footer slot because a sixth item breaks the S1
 * layout's symmetry and crowds touch targets, and placed here because reviewing past match evidence is an
 * analytical task, which is what this surface is for.
 *
 * A typed enum rather than data-driven rows, matching `EAFLNavTarget`: these three come from the spec, not
 * from a designer's discretion, and the compiler noticing an unwired one is worth more than the freedom to
 * add a fourth without a recompile.
 */
UENUM(BlueprintType)
enum class EAFLCareerTab : uint8
{
	/** Cumulative eliminations against the §3 threshold ladder. Permanent, never decays (R10, §9.1). */
	Volume,
	/** The two OpenSkill ladders, one per RULESET -- BR and MATCH PLAY (R64/R65). Moves both ways. */
	Rank,
	/** `W_ReplayBrowserScreen`, hosted rather than pushed. */
	Replays
};

/** One tab: its identity, its label, and a member pointer to the class that fills it. */
struct FAFLCareerTabRow;

/**
 * UAFLW_CareerHub -- the Career surface, and the home REPLAYS was ruled into.
 *
 * ══ WHAT THIS IS AND IS NOT ═══════════════════════════════════════════════════════════════════════════
 *
 * It is a HOST. Each tab's content is a separate activatable widget swapped inside one switcher, so a tab
 * is not a push: the player stays on Career and the back stack still has exactly one thing on it. That is
 * the whole point of the ruling -- "wired as a sub-tab view inside the parent Career layout widget rather
 * than polluting the root-level navigation stack".
 *
 * ⚠ ONLY REPLAYS HAS CONTENT TODAY, and the other two ship DISABLED with a stated reason rather than as
 * empty panels. Same rule as the footer's Venues and Career items and the staked door before S4 landed: a
 * control that accepts a click and shows nothing is the silent no-op the states table forbids. Drop a
 * class into `VolumeTabClass` or `RankTabClass` and that tab lights up -- no code change.
 *
 * ⚠ AND THE TAB SET IS DERIVED, NOT RULED AS A SCREEN. `league-play.md` specifies the two career AXES
 * (§3 volume, §9.2 rating) and R10 requires they stay apart; the ruling put REPLAYS here. Nobody has yet
 * ruled what a Career SCREEN looks like. The information architecture follows from those two documents,
 * and is the part most likely to be revised when someone does design it.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_CareerHub : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/**
	 * The three tabs' content.
	 *
	 * SOFT, like every other destination in this front end: Career is reached from the footer, most
	 * sessions never open it, and the replay browser drags a list entry widget and its art with it.
	 *
	 * `ReplaysTabClass` is the one that ships set. ⚠ IT IS ALSO WHAT PUTS `W_ReplayBrowserScreen` BACK IN
	 * THE COOK -- a soft class pointer in a CDO is a reference the cooker follows. The 2026-08-10 cook
	 * measured both replay packages dropping out of the build when the dead front-end roots were removed,
	 * because nothing referenced them any more. This is the "nothing" being fixed.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Career") TSoftClassPtr<UCommonActivatableWidget> VolumeTabClass;
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Career") TSoftClassPtr<UCommonActivatableWidget> RankTabClass;
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Career") TSoftClassPtr<UCommonActivatableWidget> ReplaysTabClass;

	/** Show a tab. Refuses -- loudly -- when that tab has no content yet. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Career")
	void SelectTab(EAFLCareerTab Tab);

	/** Same, by console id (`volume`/`rank`/`replays`). The string surface stops at this one function. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Career")
	void SelectTabById(FName TabId);

	/** True when this tab has a class set. The predicate the tab buttons are enabled from. */
	UFUNCTION(BlueprintPure, Category = "AFL|Career")
	bool IsTabAvailable(EAFLCareerTab Tab) const;

	/** The console id for a tab. `NAME_None` if the tab has no row -- see the routing-table note. */
	UFUNCTION(BlueprintPure, Category = "AFL|Career")
	static FName GetTabId(EAFLCareerTab Tab);

	/** Parse a console id. False for anything that is not one of the three, without guessing. */
	static bool TryParseTab(FName TabId, EAFLCareerTab& OutTab);

	/** The tab shown on open when the preferred one is unavailable. Never returns an empty tab. */
	UFUNCTION(BlueprintPure, Category = "AFL|Career")
	EAFLCareerTab ResolveLandingTab() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Blueprint hook for the tab-row selection treatment (the ruled Electric fill on the active tab). */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Career", meta = (DisplayName = "On Tab Changed"))
	void BP_OnTabChanged(EAFLCareerTab Tab);

	/** REQUIRED: a hub with no switcher is not a hub. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Career")
	TObjectPtr<UCommonActivatableWidgetSwitcher> ContentSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Career") TObjectPtr<UCommonButtonBase> Tab_Volume;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Career") TObjectPtr<UCommonButtonBase> Tab_Rank;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Career") TObjectPtr<UCommonButtonBase> Tab_Replays;

	/** Shown in place of content when the selected tab is unbuilt. Says which, and why. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Career")
	TObjectPtr<UCommonTextBlock> EmptyTabReason;

private:
	static TArrayView<const FAFLCareerTabRow> GetTabRows();
	static const FAFLCareerTabRow* FindTabRow(EAFLCareerTab Tab);

	/** Bind the tab buttons and disable any whose content is unbuilt. */
	void ApplyTabAvailability();

	/** Load and host a tab's content in the switcher, once. Returns null when the class is unset. */
	UCommonActivatableWidget* EnsureTabContent(EAFLCareerTab Tab);

	/** Content already instantiated into the switcher, so re-selecting a tab does not rebuild it. */
	UPROPERTY(Transient)
	TMap<uint8, TObjectPtr<UCommonActivatableWidget>> HostedContent;

	EAFLCareerTab ActiveTab = EAFLCareerTab::Replays;
};
