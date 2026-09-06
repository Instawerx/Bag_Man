// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "AFLW_BRHeader.generated.h"

class UTextBlock;
class UAFLBattleRoyaleComponent;

/**
 * UAFLW_BRHeader  (Battle Royale HUD header -- the count-UP match clock made legible)
 *
 * Plain passive UUserWidget mirroring UAFLW_RoundHeader (the C++ half owns every binding; the WBP child owns
 * layout only, via BindWidget). Binds the ALREADY-REPLICATED UAFLBattleRoyaleComponent (a UGameStateComponent
 * on the GameState, client-visible): the match clock COUNTS UP from Playing entry via GetElapsedMatchSeconds()
 * and freezes at match end (recording the match length) -- NOT the match-play per-round COUNTDOWN the round
 * header shows, which is the bug this replaces. Also AlivePlayers / TotalParticipants and the local player's
 * placement. ZERO new replication -- the BR component already replicates these.
 *
 * Resolve mirrors UAFLW_RoundHeader::TryArm: the GameState component can replicate after widget construct, so
 * a bounded 0.5s poll arms it. Slotted into the BR HUD via LAS_AFL_BR_S1's GameFeatureAction_AddWidgets,
 * replacing WBP_AFL_RoundHeader (which counts DOWN). Match-play HUD is untouched.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_BRHeader : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** The count-UP match clock, "M:SS", frozen at match end (records the length). (WBP child must provide.) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TimeText;

	/** Living players over total, "N / M". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AliveText;

	/** The local player's placement, "#K", or a dash before any elimination. Optional in the WBP layout. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RankText;

	/** Count-up clock color (SSOT electric neon blue #1E5AFF). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|BR")
	FLinearColor TimeColor = FLinearColor(0.118f, 0.353f, 1.0f);

	/** WBP animation hook -- C++ signals an elimination-count change, the WBP child may play a flash. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|BR")
	void OnAliveChanged(int32 NewAlive);

private:
	void TryArm();
	void Refresh();
	static FText FormatClock(float Seconds);

	TWeakObjectPtr<UAFLBattleRoyaleComponent> BR;
	FTimerHandle ArmRetryTimer;

	// change-guards -- only re-text on an actual change, so the NativeTick read stays cheap.
	int32 LastClockSec = -1;
	int32 LastAlive = -1;
	int32 LastTotal = -1;
	int32 LastRank = -2;   // -2 (never), 0 (unranked -> dash), >0 (placement)
};
