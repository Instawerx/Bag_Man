// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "WeaponSpawns/AFLWeaponSpawner.h"
#include "Engine/TimerHandle.h"
#include "Retail/AFLRetailSubsystem.h" // EAFLGrabArmMode (the operator's dwell/press knob)

#include "AFLDisplayPedestal.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UWidgetComponent;

/**
 * AAFLDisplayPedestal -- the GRAB PAD (distributed retail S2, PX_DISTRIBUTED_RETAIL_PLAN).
 *
 * Evolved from the centralized-store shelf pedestal at the operator pivot (2026-08-31): the pad IS
 * the product now. Step onto the tight pad -> the retail subsystem arms the try-on (dwell or E, the
 * INTENTIONAL-pickup knob), the server map-exception grant equips it through the REAL selection seam
 * (wearing the mask / holding the weapon), and the small corner card opens. Step off unbought -> the
 * server restores your look. All UX lives in UAFLRetailSubsystem (AFLCombat); this actor is the
 * trigger + the display fixture, nothing more.
 *
 * Client-local by doctrine (helper s4): overlap events feed a client subsystem; the server sees only
 * the try-on/purchase RPCs. AttemptPickUpWeapon stays a no-op -- the pad never grants by touch.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLHUB_API AAFLDisplayPedestal : public AAFLWeaponSpawner
{
	GENERATED_BODY()

public:
	AAFLDisplayPedestal();

	/** The catalog row this pad sells (AFL.<Type>.<Name>). Display name/price resolve live. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	FName CosmeticId;

	/** INTENTIONAL-pickup knob (operator playtest call, plan s"INTENTIONAL pickups"): dwell arms after
	 *  DwellSeconds on the pad; Press waits for an explicit E-grab. Both ship; default dwell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	EAFLGrabArmMode ArmMode = EAFLGrabArmMode::Dwell;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float DwellSeconds = 0.35f;

	/** Head bust the facemask display paints (the dismembered-head recipe). Operator-swappable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TSoftObjectPtr<UStaticMesh> FacemaskBustMesh =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/BagMan/Characters/Dismember/SM_AFL_RobotHead_Gib.SM_AFL_RobotHead_Gib")));

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Retail pads are display-only: silence the stock spawner's null-definition Error. */
	virtual bool WantsStockSpawnerFallback() const override { return false; }

	/** THE ITEM IS THE DISPLAY (operator gap, lap 2026-08-31): resolve the catalog row to a real
	 *  visual -- accessory rows spawn their AccessoryPartClass actor (the actual chain/watch/pendant
	 *  mesh); facemask rows paint the mask MIC onto the head bust (AAFLDismemberedHead pattern,
	 *  every slot). Client-local, BeginPlay-time, no gameplay coupling. */
	void ResolveRetailDisplay();

	/** RETAIL OVERRIDE: the pad never grants by touch. The subsystem owns the whole verb set. */
	virtual void AttemptPickUpWeapon_Implementation(APawn* Pawn) override;

	UFUNCTION()
	void OnPadBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnPadEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void UpdatePlate();

	/** TIGHT arm trigger (~1m -- the item's own footprint, never zone-sized; operator law). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TObjectPtr<UBoxComponent> ShelfBox;

	/** Product plate: AT-ITEM ONLY (<=4m + line of sight -- the plate-through-walls bug, fixed by rule). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TObjectPtr<UWidgetComponent> PlateWidget;

	/** Optional display prop for non-weapon rows (mask bust / jewellery case) while the spawner half
	 *  only knows weapon meshes. Operator/artist assigns per placement; empty = spawner display only. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TObjectPtr<UStaticMeshComponent> DisplayProp;

	/** The visible display PLATFORM (operator lap-2: "no spawner or display platforms are visible") --
	 *  a dark brand cylinder the item floats above. Always present, editor-visible. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TObjectPtr<UStaticMeshComponent> PlinthMesh;

private:
	bool bPawnAtShelf = false;
	FTimerHandle PlateTimer;

	/** The spawned accessory display actor (client-local), destroyed with the pad. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> DisplayPartActor;

	/** Facemask display: floating shop-thumbnail plate (upright, brand-consistent -- replaces the
	 *  lap-2 "flat gib on the floor"). Created at runtime, spins with DisplayProp. */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> ThumbPlate;

	/** Pushed to the plate lazily (the widget component creates its UUserWidget late). */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ThumbTexture;
};
