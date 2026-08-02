// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Round/AFLRoundManagerComponent.h"   // EAFLRoundWinReason (the resolve-handler signature) + OnRoundResolved

#include "AFLW_RoundResultToast.generated.h"

class UTextBlock;

/**
 * UAFLW_RoundResultToast  (Tier-1 round HUD -- the between-round result beat)
 *
 * Plain UUserWidget (passive transient banner; mirrors UAFLW_ExtractionAnnounce). C++ owns bindings,
 * the WBP child owns layout (BindWidget). Binds UAFLRoundManagerComponent::OnRoundResolved -- which
 * already fires on clients via OnRep_RoundResolved (ZERO new backend) -- and reads the win reason. On
 * each round resolve it announces, from the LOCAL player's POV: ROUND WON (cyan) / LOST (red) / DRAW
 * (amber) + the reason (ELIMINATION / EXTRACTION / TIMEOUT / REPLAY), holds HoldSeconds, then collapses.
 * The phase labels flash by between rounds, so this toast is what makes the round outcome legible.
 *
 * DISTINCT from Surface 1 (round header: always-on, ModeStatus slot, tick-read) and the ExtractionAnnounce
 * MATCH-COMPLETE banner (center, match-end, Event.Match.Ended): different slot (upper-center), different
 * source (OnRoundResolved delegate), different timing (round-resolve transient). No entanglement.
 * Resolve mirrors UAFLW_RoundHeader::TryArm (GameState-component poll + local team via LyraTeamSubsystem).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_RoundResultToast : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** "ROUND WON" / "ROUND LOST" / "ROUND DRAW" (color-coded by C++). WBP child must provide. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ResultText;

	/** The win reason: "ELIMINATION" / "EXTRACTION" / "TIMEOUT" / "REPLAY". WBP child must provide. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ReasonText;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") FLinearColor WonColor = FLinearColor(0.0f, 0.94f, 1.0f);    // house cyan
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") FLinearColor LostColor = FLinearColor(1.0f, 0.25f, 0.25f);  // hostile red
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") FLinearColor DrawColor = FLinearColor(1.0f, 0.70f, 0.0f);   // amber
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round") float HoldSeconds = 3.0f;

	/** WBP animation hook -- C++ has set the text/color + shown the widget; the WBP plays the entrance/exit. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Round")
	void OnToastShown();

private:
	void TryArm();
	void HandleRoundResolved(int32 WinningTeam, EAFLRoundWinReason Reason);
	void Hide();

	TWeakObjectPtr<UAFLRoundManagerComponent> Round;
	int32 LocalTeam = INDEX_NONE;
	FDelegateHandle ResolvedHandle;
	FTimerHandle ArmRetryTimer;
	FTimerHandle HoldTimer;
};
