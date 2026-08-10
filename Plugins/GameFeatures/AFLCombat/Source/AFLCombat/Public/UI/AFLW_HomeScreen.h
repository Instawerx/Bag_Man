// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"

#include "AFLW_HomeScreen.generated.h"

class UCommonButtonBase;
class UCommonTextBlock;
class UWidget;

/** The two doors of the R98 split. There is no third value and there is deliberately no "None": the home
 *  screen always presents exactly two products, and a door that could be neither would be a lobby again. */
UENUM(BlueprintType)
enum class EAFLHomeDoor : uint8
{
	/** Free, no buy-in, Haywire + Pro Mod, bots allowed, unrated. Played for LOOT and WATTS. */
	League,
	/** Buy-in, Pro Mod only, rated, no bots. The DENOMINATION (Watts / Volts) is chosen BEHIND this door. */
	Staked
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAFLHomeDoorChosen, EAFLHomeDoor, Door);

/**
 * UAFLW_HomeScreen -- the IRONICS home screen: LEAGUE PLAY | STAKED PLAY (R98, R100).
 *
 * This is the FIRST decision in the game, made before any lobby exists. Two doors, two products, two
 * purposes; they share no surface, control set or mental model. `IRONICS_HOME_SCREEN_SPEC.md` is the
 * surface spec, `ssot/ui-frontend.md` sec3.2 the architecture, and R98 the ruling.
 *
 * ══ WHY THIS IS C++ AND NOT A BLUEPRINT GRAPH ═════════════════════════════════════════════════════════
 *
 * The screen carries ONE invariant that must not be violable by a designer edit:
 *
 *     A LEAGUE PLAY PLAYER NEVER SEES A STAKE AMOUNT -- because there isn't one to pick.
 *
 * League play is where Watts are EARNED; staked play is where they are RISKED. A surface that asks a
 * league player for a buy-in has misread which half of the economy they are standing in, and most players
 * are on the free side -- the whole point of the split is that the majority are never routed through a
 * wagering surface. That rule lives here, in code, where `EnsureNoStakeOnLeagueRoute` holds it and a test
 * can prove it, rather than in a widget graph where a well-meaning rewire could quietly break it.
 *
 * ══ NAVIGATION -- WIRED 2026-08-10, AND WHY IT MOVED INTO C++ ═════════════════════════════════════════
 *
 * This class used to resolve the choice and broadcast it WITHOUT navigating, on the stated grounds that
 * `IRONICS_HOME_SCREEN_SPEC.md` sec9.4 ends the spec at the split and *"the two lobbies behind the doors are
 * separate surfaces that do not exist yet"* -- so a destination in C++ would have been a guess.
 *
 * **They exist now** (`WBP_IRONICS_Lobby_League` / `WBP_IRONICS_Lobby_Staked`, both on `UAFLW_Lobby_Root`,
 * each declaring its own door). The reason for not navigating has therefore expired, and the push lives
 * here where a test can reach it rather than in a widget graph where it cannot.
 *
 * ⚠ THE PUSH IS GATED ON THE CLASS BEING SET, so a WBP that wants to own navigation still can: leave
 * `LeagueLobbyClass`/`StakedLobbyClass` empty and handle `BP_OnDoorChosen` instead. Both fire either way --
 * C++ never suppresses the hook, because analytics and the sec6 select transition hang off it.
 *
 * ══ COLOUR (R100) ═════════════════════════════════════════════════════════════════════════════════════
 *
 * The two doors are NOT colour-coded. They differ by density, motion rate and content -- never by palette.
 * Electric leads fill on both; Arc-Violet is a rim/focus accent on both and never a fill, core or text
 * colour. Chrome is the app's own furniture (`ui-frontend` sec10.2); giving the staked door its own hue
 * would make chrome carry meaning that belongs to identity. Nothing here sets colour: the palette is the
 * WBP's and the style system's. This comment exists so a future edit knows the constraint is a RULING.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_HomeScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UAFLW_HomeScreen(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Fires once a door is chosen AND permitted. The WBP (or a future front-end router) performs the push. */
	UPROPERTY(BlueprintAssignable, Category = "AFL|Home")
	FAFLHomeDoorChosen OnDoorChosen;

	/**
	 * Is staked play open to this player right now? FALSE puts the staked door in the spec's Disabled
	 * treatment (sec5) with `StakedUnavailableReason` shown beneath its title.
	 *
	 * Still FALSE, but ⚠ **NOT FOR THE REASONS THIS COMMENT USED TO GIVE.** It said the staked lobby was
	 * unbuilt and every staked queue unpublished. Both are now untrue: `WBP_IRONICS_Lobby_Staked` exists,
	 * and `/queues` publishes 40 cells including the WattsPlay and VoltsPlay ladders.
	 *
	 * The door stays shut for a narrower and more specific reason: **entering a staked queue requires S4
	 * TicketReview, which R22 makes unskippable, and S4 is unbuilt.** `UAFLW_Lobby_Root::CommitQueue`
	 * therefore raises `OnTicketReviewRequested` and queues NOTHING -- so an enabled staked door would lead
	 * to a lobby whose CTA looks live and silently does nothing, which the states table explicitly forbids
	 * ("never a silent no-op"). A door that says why it cannot open is better than one that opens onto that.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Home")
	bool bStakedPlayAvailable = false;

	/** One line, shown under the staked title while disabled (sec5 requires a reason, not a dead control). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Home")
	FText StakedUnavailableReason;

	/**
	 * The two lobbies, pushed onto `UI.Layer.Menu` when their door is chosen.
	 *
	 * SOFT references on purpose: the home screen is the first thing built at boot and must not drag both
	 * lobby widget trees -- and everything they reference -- into memory before the player has picked one.
	 *
	 * ⚠ EACH LOBBY DECLARES ITS OWN DOOR (`UAFLW_Lobby_Root::Door`, EditDefaultsOnly), so pushing the right
	 * CLASS is the whole of the routing. Nothing here passes a door through, which is what keeps the
	 * staked lobby from ever being opened in league configuration by a caller that forgot an argument.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Home")
	TSoftClassPtr<UCommonActivatableWidget> LeagueLobbyClass;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Home")
	TSoftClassPtr<UCommonActivatableWidget> StakedLobbyClass;

	/** Push the wallet readout. Both doors show it: it is chrome, and a player's balance is not a stake. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Home")
	void SetWalletReadout(int64 Watts, int64 Volts);

	/** Server/consumer-facing: can this door be entered? Staked is gated; league is always open. */
	UFUNCTION(BlueprintPure, Category = "AFL|Home")
	bool IsDoorAvailable(EAFLHomeDoor Door) const;

	/**
	 * Choose a door: gate it, broadcast it, then navigate.
	 *
	 * PUBLIC because it is the screen's primary verb, not a button detail -- the two doors call it, and so
	 * does `afl.Home.Door`, which is the only way to exercise the push without an input device. It re-checks
	 * availability itself, so no caller can route around the gate.
	 */
	UFUNCTION(BlueprintCallable, Category = "AFL|Home")
	void ChooseDoor(EAFLHomeDoor Door);

	/**
	 * THE INVARIANT, as a callable predicate so a test can hold it.
	 *
	 * True when the league route carries no stake. Any caller routing a player through LEAGUE must pass a
	 * zero stake, because league has no buy-in to pick; a non-zero value means somebody has plumbed a
	 * wagering surface into the free half of the economy.
	 */
	UFUNCTION(BlueprintPure, Category = "AFL|Home")
	static bool IsStakeLegalForDoor(EAFLHomeDoor Door, int64 StakeAmount);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	/** Blueprint hook for the navigation push. Fires immediately after `OnDoorChosen`. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Home", meta = (DisplayName = "On Door Chosen"))
	void BP_OnDoorChosen(EAFLHomeDoor Door);

	/** Blueprint hook for the sec5 Disabled treatment (42% opacity, 65% grayscale, motion stopped). */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Home", meta = (DisplayName = "On Staked Availability Changed"))
	void BP_OnStakedAvailabilityChanged(bool bAvailable, const FText& Reason);

	// -- BindWidget slots. The two doors are REQUIRED: a home screen missing one of them is not this screen,
	//    so it should fail loudly at compile rather than render a single-door surface that looks intentional.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Home")
	TObjectPtr<UCommonButtonBase> LeagueDoor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "AFL|Home")
	TObjectPtr<UCommonButtonBase> StakedDoor;

	/** Optional: chrome the WBP may or may not author yet. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Home")
	TObjectPtr<UCommonTextBlock> StakedReason;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Home")
	TObjectPtr<UCommonTextBlock> WattsChip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "AFL|Home")
	TObjectPtr<UCommonTextBlock> VoltsChip;

private:
	// Bound to UCommonButtonBase::OnClicked(), a plain no-param FCommonButtonEvent -- so these are ordinary
	// members, not UFUNCTIONs. Marking them UFUNCTION would imply a dynamic binding that does not exist here.
	void HandleLeagueClicked();
	void HandleStakedClicked();

	/** Push the lobby this door opens onto UI.Layer.Menu. No-op when the WBP owns navigation. */
	void PushLobbyForDoor(EAFLHomeDoor Door);

	/** Apply `bStakedPlayAvailable` to the staked door and its reason line. */
	void ApplyStakedAvailability();
};
