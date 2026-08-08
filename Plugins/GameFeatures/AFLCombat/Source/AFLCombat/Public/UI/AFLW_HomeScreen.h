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
 * ══ WHAT THIS CLASS DELIBERATELY DOES *NOT* DO ════════════════════════════════════════════════════════
 *
 * It does not navigate. `IRONICS_HOME_SCREEN_SPEC.md` sec9.4 ends the spec AT the split -- the two lobbies
 * behind the doors are separate surfaces that do not exist yet. So this class RESOLVES the choice and
 * BROADCASTS it (`OnDoorChosen` + `BP_OnDoorChosen`); the WBP performs the push. Inventing destinations
 * here would bake a guess into C++ that the spec has not made.
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
	 * Defaults to FALSE and that is the honest shipping state today: the staked lobby is unbuilt and every
	 * staked queue is unpublished in `config/queue-registry.json`. A door that accepts a click and goes
	 * nowhere is worse than one that says why it cannot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Home")
	bool bStakedPlayAvailable = false;

	/** One line, shown under the staked title while disabled (sec5 requires a reason, not a dead control). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AFL|Home")
	FText StakedUnavailableReason;

	/** Push the wallet readout. Both doors show it: it is chrome, and a player's balance is not a stake. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Home")
	void SetWalletReadout(int64 Watts, int64 Volts);

	/** Server/consumer-facing: can this door be entered? Staked is gated; league is always open. */
	UFUNCTION(BlueprintPure, Category = "AFL|Home")
	bool IsDoorAvailable(EAFLHomeDoor Door) const;

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

	/** Shared resolution path for both doors -- gate, then broadcast. */
	void ChooseDoor(EAFLHomeDoor Door);

	/** Apply `bStakedPlayAvailable` to the staked door and its reason line. */
	void ApplyStakedAvailability();
};
