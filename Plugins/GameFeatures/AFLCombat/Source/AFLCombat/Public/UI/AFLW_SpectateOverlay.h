// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "AFLW_SpectateOverlay.generated.h"

class UTextBlock;
class APawn;
class ULyraHealthComponent;
class UAFLRoundManagerComponent;

/**
 * UAFLW_SpectateOverlay  (Tier-1 round HUD -- Surface 3 LAYER 1: the spectate-out overlay)
 *
 * Plain UUserWidget (passive readout, never takes focus -- mirrors UAFLW_RoundHeader / EnergyMeter, NOT a
 * CommonActivatable). DELIBERATE BASE CHOICE: the surface-3 spec named UCommonUserWidget; this matches the
 * PROVEN Surface 1/2 base (UUserWidget) instead -- an identical passive HUD readout, zero new base-class /
 * CommonUI variable, same BindWidget contract. Trivial to lift to UCommonUserWidget if a CommonUI feature is
 * ever needed (it is not, for Layer 1 or the Layer-2 input cycle, which rides an enhanced-input action).
 *
 * Shown while the LOCAL player is dead under the tactical round model (die mid-round = OUT until the round
 * resolves -- respawn suppressed via State.Round.NoRespawn; the controller keeps possessing its ragdoll =
 * today's hold-cam). LAYER 1 IS THE UI ONLY; the follow-teammate camera is LAYER 2 (its own diff + 2-client proof).
 *
 * DEATH SOURCE = the local pawn's ULyraHealthComponent::IsDeadOrDying() (reads the replicated DeathState),
 * POLLED in NativeTick (mirrors UAFLW_RoundHeader's tick + change-guard). Polling the CURRENT pawn -- not a
 * delegate bound to one health comp -- is deliberate: the round reset REPLACES the pawn (force-restart), so a
 * delegate bound to the dead pawn's comp would dangle; a per-tick read of GetOwningPlayerPawn() picks up the
 * new live pawn for free and clears the overlay on respawn.
 *
 * ROUND PHASE = the already-replicated UAFLRoundManagerComponent (GameState component; resolved via the proven
 * TryArm poll). RoundActive + dead -> "ELIMINATED" / "RESPAWN NEXT ROUND"; RoundEnd / HalfTime + dead ->
 * "RESPAWNING..." (the ~5s reset tail); alive / WarmUp / MatchEnd -> hidden (the MATCH COMPLETE banner owns
 * match end). NO fake respawn timer -- you return when the round ends; the round clock is already up top (Surface 1).
 *
 * Style: IRONICS_UI_STYLE_SSOT -- UIPanel.Glass, house-cyan #00F8FF active border, ALL-CAPS display, off the
 * five beam hues (the no-green-on-green readability law). Slotted via the experience GameFeatureAction_AddWidgets
 * (a new lower-center HUD slot). Layout/motion live in the WBP child (BindWidget + the OnSpectateShown hook).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_SpectateOverlay : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Primary line: "ELIMINATED" (round active) / "RESPAWNING..." (round-end tail). WBP child must provide. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** Secondary line: "RESPAWN NEXT ROUND" (round active) / empty (round-end tail). WBP child must provide. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailText;

	/** WBP animation hook -- C++ has set the lines + made the widget visible; the WBP plays the entrance fade/blur.
	 *  Fires ONLY on the hidden->shown transition (never on a phase-only re-text), so the entrance never replays. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Round")
	void OnSpectateShown();

private:
	void TryArm();
	void Refresh();

	TWeakObjectPtr<UAFLRoundManagerComponent> Round;
	TWeakObjectPtr<APawn> LastPawn;                 // cache -- re-resolve the health comp only on a pawn swap
	TWeakObjectPtr<ULyraHealthComponent> Health;
	FTimerHandle ArmRetryTimer;

	// change-guards -- only re-text / re-show on an actual transition (the tick read stays cheap).
	bool bShown = false;
	uint8 LastShownPhase = 255;
};
