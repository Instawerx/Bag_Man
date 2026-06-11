// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffectTypes.h"

#include "AFLW_ExtractChannelBar.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
class UAFLWalletComponent;

/**
 * UAFLW_ExtractChannelBar  (extraction cycle 1 -- the channel-progress HUD)
 *
 * The energy-meter pattern verbatim: C++ owns every binding (BP graphs are bridge-hostile), the
 * WBP child owns layout only. Collapsed while idle.
 *
 * Fill = CLIENT-LOCAL CLOCK, zero new replication: State.Extracting tag-add starts a 0->6s local
 * fill (the duration is a design constant mirroring UAFLAG_Extract::ChannelDuration); tag-remove
 * stops it. The replicated tag's arrival latency is cosmetic-acceptable (the meter precedent).
 *
 * Outcome flash, both windows truthfully:
 *  - GREEN on server-confirmed reward: OnWalletChanged with Watts UP (the store's proven bind --
 *    OnRep-driven, fires on the owning client) OR the Event.Extraction.Complete message (host
 *    window only -- the GA broadcasts on the authority world).
 *  - RED otherwise (interrupt/cancel: tag dropped with no wallet gain).
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_ExtractChannelBar : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** The fill bar the WBP child must provide. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ChannelBar;

	/** Optional label ("EXTRACTING..."). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChannelText;

	/** Mirrors UAFLAG_Extract::ChannelDuration (design constant this cycle -- a replicated
	 *  duration handshake is deliberately NOT built; re-sync both knobs together). */
	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction", meta = (ClampMin = "0.1"))
	float ChannelDuration = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	FLinearColor ChannelColor = FLinearColor(0.0f, 0.94f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	FLinearColor CompleteColor = FLinearColor(0.1f, 1.0f, 0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	FLinearColor FailColor = FLinearColor(1.0f, 0.15f, 0.1f);

private:
	void TryArm();
	void HandleExtractingChanged(const FGameplayTag Tag, int32 NewCount);

	UFUNCTION()
	void HandleWalletChanged(int32 Volts, int32 Watts);

	void Collapse();

	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<UAFLWalletComponent> Wallet;
	FDelegateHandle ExtractingTagHandle;
	FGameplayMessageListenerHandle CompleteListener;   // host-window refinement only
	FTimerHandle ArmRetryTimer;
	FTimerHandle CollapseTimer;

	bool bChanneling = false;
	bool bSawComplete = false;
	bool bSawWalletGain = false;
	int32 LastWatts = 0;
	float Elapsed = 0.0f;
};
