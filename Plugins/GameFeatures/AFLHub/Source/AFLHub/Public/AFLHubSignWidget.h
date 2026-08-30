// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "AFLHubSignWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;

/** Distance tier the sign is rendering (ratified mock: far beacon / mid plate / at-door full plate). */
UENUM()
enum class EAFLHubSignTier : uint8
{
	Far,     // > 40m: diamond + name + light pillar
	Mid,     // 15-40m: name plate + live distance
	AtDoor,  // inside the door volume: full plate + status band
};

/**
 * UAFLHubSignWidget -- the ratified hub wayfinding sign (canvas 0542c547, operator-approved).
 *
 * Widget tree is BUILT IN C++ (no WBP): brand fonts loaded from /Game/UI/Foundation/Fonts
 * (Orbitron display, NotoSans body -- DroidSansMono is not imported, NotoSans substitutes on the
 * data line, flagged in the ticket). Lives on the door volume's screen-space WidgetComponent, which
 * supplies camera-facing + over-structures visibility; the volume drives SetSignData on a low-rate
 * timer. PROPOSED->ratified token status-offline #FF5A64 rides with the approval.
 */
UCLASS()
class AFLHUB_API UAFLHubSignWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** One call per tier decision -- rebuild-free; the blocks toggle visibility + text. */
	void SetSignData(const FText& InName, const FText& InSubtitle, bool bInEnabled,
		EAFLHubSignTier InTier, float InDistanceMeters);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient) TObjectPtr<UVerticalBox>   Root;
	UPROPERTY(Transient) TObjectPtr<UBorder>        DiamondOuter;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>     NameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>     SubtitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>     DistanceText;
	UPROPERTY(Transient) TObjectPtr<UBorder>        StatusBand;
	UPROPERTY(Transient) TObjectPtr<UBorder>        StatusDot;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>     StatusText;
	UPROPERTY(Transient) TObjectPtr<UBorder>        Pillar;
};
