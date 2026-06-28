// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "AFLW_RoundHeader.generated.h"

class UTextBlock;
class UAFLRoundManagerComponent;

/**
 * UAFLW_RoundHeader  (Tier-1 round HUD -- the round-based extraction loop made legible)
 *
 * Plain UUserWidget (a passive readout, never takes focus -- mirrors UAFLW_EnergyMeter, NOT a
 * CommonActivatable). The C++ half owns every binding; the WBP child owns layout only (BindWidget).
 *
 * Binds the ALREADY-REPLICATED UAFLRoundManagerComponent (a UGameStateComponent on the GameState,
 * client-visible): CurrentRound / RoundsToWin (best-of context), Team0Score / Team1Score (mapped to
 * my-vs-enemy via the local team), RoundTimeRemaining + Phase (the clock / phase label). ZERO new
 * replication -- the round manager was built to "drive the HUD via OnRep"; this is the missing consumer.
 *
 * Resolve mirrors UAFLW_EnergyMeter::TryArm: the GameState component can replicate after widget
 * construct, so a bounded 0.5s poll arms it; the local score slot resolves via ULyraTeamSubsystem
 * (falls back to raw slot 0 = "mine" if the team isn't ready -- the header still reads correctly).
 *
 * Style: IRONICS_UI_STYLE_SSOT -- house-cyan clock, amber when low; mono numerics; ALL-CAPS display.
 * Slotted into the Lyra HUD via the experience GameFeatureAction_AddWidgets (HUD.Slot.ModeStatus).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_RoundHeader : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** "ROUND 3 / 7" -- current round over the best-of target. (WBP child must provide.) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> RoundText;

	/** "01:23" while RoundActive, else the phase label (WARMUP / ROUND END / HALFTIME / MATCH END). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TimerText;

	/** The local player's team score (mono numeric). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MyScoreText;

	/** The enemy team score (mono numeric). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EnemyScoreText;

	/** Clock color normal / low (SSOT house-cyan -> amber under the low threshold). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round")
	FLinearColor TimerNormalColor = FLinearColor(0.0f, 0.94f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round")
	FLinearColor TimerLowColor = FLinearColor(1.0f, 0.70f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Round")
	float TimerLowThreshold = 10.0f;

	/** WBP animation hooks -- C++ signals a change, the WBP child plays the pulse/flash (layout owns motion). */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Round")
	void OnRoundChanged(int32 NewRound);

	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Round")
	void OnMyScoreChanged(int32 NewScore);

	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Round")
	void OnEnemyScoreChanged(int32 NewScore);

private:
	void TryArm();
	void Refresh();
	static FText FormatClock(float Seconds);

	TWeakObjectPtr<UAFLRoundManagerComponent> Round;
	int32 LocalSlot = INDEX_NONE;   // 0/1 = my score slot; INDEX_NONE = unresolved (raw slot 0 = "mine")
	FTimerHandle ArmRetryTimer;

	// change-guards -- only re-text on an actual change, so the NativeTick read stays cheap.
	int32 LastRound = -1;
	int32 LastMyScore = -1;
	int32 LastEnemyScore = -1;
	int32 LastClockSec = -1;
	uint8 LastPhase = 255;
};
