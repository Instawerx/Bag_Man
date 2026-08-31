// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "AFLRetailWeaponTower.generated.h"

class AAFLDisplayPedestal;
class UStaticMeshComponent;
class USceneComponent;

/**
 * AAFLRetailWeaponTower -- the VERTICAL ROTATING weapon display (operator directive 2026-08-31:
 * "Add our weapons to a large open space. Create a vertical rotating display for space efficiency").
 *
 * One pole, up to six weapons in a rising spiral on a slow shared spinner -- six SKUs on a single
 * pad's footprint. Each tier's REAL weapon mesh is resolved WITHOUT spawning gameplay:
 * catalog row -> UAFLWeaponCosmeticAsset -> ULyraEquipmentDefinition CDO -> ActorsToSpawn[0]
 * actor class -> SCS component-template walk up the super chain (the CDO-iteration-lies trap:
 * the inherited SkeletalMesh component is INVISIBLE to CDO component iteration) -> skeletal mesh
 * asset. Beam weapons legitimately carry no mesh -> the row's shop thumbnail plate stands in.
 *
 * Purchase UX is 100% the proven pad loop: the tower spawns one display-suppressed
 * AAFLDisplayPedestal per tier around its base (grab -> REAL try-on in your hands -> card).
 * Client-local by doctrine; a dedicated server keeps only the bare actor.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLHUB_API AAFLRetailWeaponTower : public AActor
{
	GENERATED_BODY()

public:
	AAFLRetailWeaponTower();

	/** The weapon SKUs on this tower, bottom tier first (max 6 used). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TArray<FName> CosmeticIds;

	UPROPERTY(EditAnywhere, Category = "AFL|Retail") float TierBaseZ = 150.f;
	UPROPERTY(EditAnywhere, Category = "AFL|Retail") float TierStep = 88.f;
	UPROPERTY(EditAnywhere, Category = "AFL|Retail") float ArmRadius = 85.f;
	UPROPERTY(EditAnywhere, Category = "AFL|Retail") float PadRadius = 280.f;
	UPROPERTY(EditAnywhere, Category = "AFL|Retail") float SpinRateDegPerSec = 20.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void BuildDisplay();

	UPROPERTY(VisibleAnywhere, Category = "AFL|Retail")
	TObjectPtr<UStaticMeshComponent> PoleMesh;

	/** Everything item-visual hangs off this; Tick spins it. */
	UPROPERTY(VisibleAnywhere, Category = "AFL|Retail")
	TObjectPtr<USceneComponent> Spinner;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AAFLDisplayPedestal>> Pads;
};
