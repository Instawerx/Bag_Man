// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "WeaponSpawns/AFLWeaponSpawner.h"
#include "Engine/TimerHandle.h"

#include "AFLDisplayPedestal.generated.h"

class UBoxComponent;
class UWidgetComponent;
class UInputComponent;

/**
 * AAFLDisplayPedestal  (PX retail framework -- MAIN_MAP_LOBBY_SYSTEM_HELPER s4 / LOBBY_UPGRADE_DOC)
 *
 * "Use map weapon spawner systems recolored for our store -- gives us all the functionality we
 * need and AAA Display" (operator directive, verbatim). This IS the proven weapon-spawner pad,
 * re-purposed: the pad's registry-driven weapon display stays; the GRANT is replaced by RETAIL
 * ENGAGEMENT -- at the shelf, E opens the product page focused on this pedestal's catalog row.
 *
 * Client-local by doctrine (s4: shopping visuals cost the server nothing): the plate widget and
 * the engage keys exist only on the local client; AttemptPickUpWeapon is overridden to a no-op
 * so walking over the pad never grants. Purchase validation stays server/PlayFab-side, reached
 * through the product page's wallet calls -- this actor never touches currency.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLHUB_API AAFLDisplayPedestal : public AAFLWeaponSpawner
{
	GENERATED_BODY()

public:
	AAFLDisplayPedestal();

	/** The catalog row this pedestal sells (AFL.<Type>.<Name>). Display name/price resolve live. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	FName CosmeticId;

	/** The product-page widget class the engage push opens (soft path -- content stays data). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	FString ProductPageClassPath = TEXT("/Script/AFLCombat.AFLW_ProductPage");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** RETAIL OVERRIDE: the pad never grants. Engagement is the only verb. */
	virtual void AttemptPickUpWeapon_Implementation(APawn* Pawn) override;

	UFUNCTION()
	void OnShelfBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnShelfEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void OnEngagePressed();
	void UpdatePlate();

	/** At-shelf trigger (the door PromptBox pattern, shelf-sized). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TObjectPtr<UBoxComponent> ShelfBox;

	/** Floating product plate (name + price + ENTER), screen-space like the door signs. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TObjectPtr<UWidgetComponent> PlateWidget;

private:
	bool bPawnAtShelf = false;
	bool bEngageBound = false;
	FTimerHandle PlateTimer;
};
