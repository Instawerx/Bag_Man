// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"

#include "AFLRetailTill.generated.h"

class UBoxComponent;
class UWidgetComponent;

/**
 * AAFLRetailTill -- the CHECKOUT COUNTER (distributed retail S2; operator ruled BOTH checkout paths:
 * till pad AND from-chip anywhere).
 *
 * Walk up -> the cart chip opens expanded with checkout focused (X confirms, X runs). No input steal,
 * no screen takeover -- the till is a courtesy surface for the detailed shopper; the chip alone
 * already checks out from anywhere. Client-local; the server sees only the purchase RPCs.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLHUB_API AAFLRetailTill : public AActor
{
	GENERATED_BODY()

public:
	AAFLRetailTill();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTillBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTillEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** At-counter trigger (door PromptBox pattern, counter-sized). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TObjectPtr<UBoxComponent> CounterBox;

	/** "TILL — CHECKOUT" plate (screen-space sign, at-counter scale). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TObjectPtr<UWidgetComponent> SignWidget;
};
