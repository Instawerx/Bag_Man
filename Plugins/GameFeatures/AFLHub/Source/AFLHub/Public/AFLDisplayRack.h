// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "AFLDisplayRack.generated.h"

class AAFLDisplayPedestal;

/**
 * AAFLDisplayRack  (PX retail framework -- MAIN_MAP_LOBBY_SYSTEM_HELPER s4 / hub SSOT s5.1)
 *
 * A catalog-driven shelf row: give it a namespace prefix (the store-tab taxonomy) and a count,
 * and it stocks itself with display pedestals from the live PURCHASABLE set at BeginPlay --
 * "new catalog row = new stock, no map edit" (I-25 law). Client-local like everything retail
 * (s4: the server never pays for shopping); the row skips dedicated servers entirely.
 *
 * S1.5 split, stated honestly: rack-stocked pedestals show the PLATE (name + live price + E ->
 * product page). The held 3D weapon mesh on a pad comes from the spawner REGISTRY tag, which
 * maps a handful of roster weapons -- hand-placed pedestals carry both; rack rows go plate-first
 * until the catalog->display-mesh map exists (the mesh half of H3.6).
 */
UCLASS(Blueprintable, BlueprintType)
class AFLHUB_API AAFLDisplayRack : public AActor
{
	GENERATED_BODY()

public:
	AAFLDisplayRack();

	/** Catalog namespace this rack stocks (e.g. "AFL.Facemask." / "AFL.Weapon."). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	FString NamespacePrefix = TEXT("AFL.Facemask.");

	/** How many pedestals this rack holds (curation: the store surface is never the catalog). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail", meta = (ClampMin = "1", ClampMax = "12"))
	int32 SlotCount = 4;

	/** Spacing between pedestals along the rack's right vector (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	float SlotSpacing = 350.f;

	/** Pedestal class to stock with (data knob; defaults to the C++ pedestal). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TSubclassOf<AAFLDisplayPedestal> PedestalClass;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Restock();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAFLDisplayPedestal>> Stocked;
};
