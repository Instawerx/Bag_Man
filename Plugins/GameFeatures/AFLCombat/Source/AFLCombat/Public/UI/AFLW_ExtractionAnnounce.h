// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "AFLW_ExtractionAnnounce.generated.h"

class UTextBlock;

/**
 * UAFLW_ExtractionAnnounce  (match phases cycle 1 -- the window-open/close banner)
 *
 * The meter/channel-bar pattern: C++ owns the bindings (BP graphs are bridge-hostile), the WBP
 * child owns layout (one center-screen BindWidget TextBlock). Collapsed while idle.
 *
 * Listens for the DUAL-BROADCAST announce messages (Event.Extraction.WindowOpen / WindowClosed)
 * the driver fires on BOTH the client world (via the GameState multicast) AND the listen-server
 * host world (locally). So this shows on every screen with ZERO new replication -- the dual-
 * broadcast verdict is: if it shows on the client but not the host, the driver's server-local
 * broadcast is missing.
 *
 * WindowOpen -> "EXTRACTION WINDOW OPEN" (green), WindowClosed -> "WINDOW CLOSED" (red), each for
 * ~3s then collapse.
 *
 * Also listens for Event.Match.Ended (match spine cycle 1) -- the per-player dual-broadcast with the
 * this-match Watts in Magnitude. Filters Target == own PlayerState, shows "MATCH COMPLETE -- N WATTS
 * EARNED" HELD (terminal, never collapses). Full scoreboard (kills/energy) = named S-later debt.
 */
UCLASS(Abstract)
class AFLCOMBAT_API UAFLW_ExtractionAnnounce : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** The center-screen banner text the WBP child must provide. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AnnounceText;

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	FLinearColor OpenColor = FLinearColor(0.1f, 1.0f, 0.2f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	FLinearColor ClosedColor = FLinearColor(1.0f, 0.3f, 0.15f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Match")
	FLinearColor MatchEndColor = FLinearColor(1.0f, 0.85f, 0.1f);

	UPROPERTY(EditDefaultsOnly, Category = "AFL|Extraction")
	float HoldSeconds = 3.0f;

private:
	void HandleWindowOpen(FGameplayTag Channel, const struct FLyraVerbMessage& Msg);
	void HandleWindowClosed(FGameplayTag Channel, const struct FLyraVerbMessage& Msg);
	void HandleMatchEnded(FGameplayTag Channel, const struct FLyraVerbMessage& Msg);
	void Show(const FText& Message, const FLinearColor& Color);
	void ShowHeld(const FText& Message, const FLinearColor& Color); // terminal -- no collapse timer
	void Collapse();

	FGameplayMessageListenerHandle OpenListener;
	FGameplayMessageListenerHandle ClosedListener;
	FGameplayMessageListenerHandle MatchEndedListener;
	FTimerHandle CollapseTimer;
};
