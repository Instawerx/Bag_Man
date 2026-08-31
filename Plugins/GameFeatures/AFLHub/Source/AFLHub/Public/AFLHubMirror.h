// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"

#include "AFLHubMirror.generated.h"

class UBoxComponent;
class UImage;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UWidgetComponent;

/** The mirror surface: one full-bleed image fed the capture RT (no material asset needed). */
UCLASS()
class AFLHUB_API UAFLHubMirrorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMirrorTexture(UTexture* InTexture, const FVector2D& InSize);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient) TObjectPtr<UImage> MirrorImage;
};

/**
 * AAFLHubMirror -- try-on mirror for the Visor / Jewellery houses
 * (MAIN_MAP_LOBBY_SYSTEM_HELPER s5, verbatim recipe; distributed retail S2).
 *
 * Proximity-gated SceneCapture: capture is OFF by default; the wake box turns it on for the LOCAL
 * player only, show-only-filtered to that pawn + everything attached to it (character parts, the
 * held try-on weapon) -- so 1,000 CCU never pays for a mirror nobody is standing at, and the mirror
 * shows YOU wearing what you're trying, not the crowd. Capture sleeps again on exit.
 *
 * Place with +X facing OUT of the wall; the surface widget and the capture both look down +X.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLHUB_API AAFLHubMirror : public AActor
{
	GENERATED_BODY()

public:
	AAFLHubMirror();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnWakeBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnWakeEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Rebuild the show-only list (equipment/parts change DURING try-on -- refreshed at 2 Hz awake). */
	void RefreshShowOnly();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Mirror")
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Mirror")
	TObjectPtr<UWidgetComponent> Surface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Mirror")
	TObjectPtr<UBoxComponent> WakeBox;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> MirrorRT;

	TWeakObjectPtr<APawn> WatchedPawn;
	FTimerHandle ShowOnlyTimer;
};
