// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Blueprint/UserWidget.h"

#include "AFLRetailStickerWall.generated.h"

class AAFLDisplayPedestal;
class UCanvasPanel;
class UImage;
class USizeBox;
class UStaticMeshComponent;
class UTexture2D;

/**
 * UAFLStickerCropWidget -- one atlas TILE as a plate. Stickers have no per-row textures or
 * thumbnails; they are 4x4 tiles of T_BagMan_StickerAtlas. The crop is pure widget math: a
 * clipping SizeBox windowing a 4x-scaled atlas image offset to the tile -- no material authoring.
 */
UCLASS()
class AFLHUB_API UAFLStickerCropWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTile(UTexture2D* Atlas, int32 TileIndex, float PlateSize);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient) TObjectPtr<USizeBox> Window;
	UPROPERTY(Transient) TObjectPtr<UImage> AtlasImage;
};

/**
 * AAFLRetailStickerWall -- the GRAFFITI WALL (operator directive 2026-08-31: "for our stickers
 * let's use a wall display graffiti wall inspired theme display").
 *
 * A freestanding dark wall slab plastered with every sticker in the catalog as atlas-crop plates
 * at scattered sizes/tilts (deterministic layout -- same wall every boot). Stickers themselves are
 * NOT directly sellable (bTransactable=false; they redeem through AFL.StickerCredit counters), so
 * the wall's PADS sell the CREDIT PACKS (x5/x10) through the proven grab loop, and the mural is
 * the catalog. Client-local; a dedicated server keeps the bare slab.
 */
UCLASS(Blueprintable, BlueprintType)
class AFLHUB_API AAFLRetailStickerWall : public AActor
{
	GENERATED_BODY()

public:
	AAFLRetailStickerWall();

	/** Mural rows (AFL.Sticker.*; inert tile=-1 rows are skipped automatically). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TArray<FName> StickerIds;

	/** The sellable pads in front of the wall (the credit packs). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AFL|Retail")
	TArray<FName> PadCosmeticIds;

	UPROPERTY(EditAnywhere, Category = "AFL|Retail")
	TSoftObjectPtr<UTexture2D> StickerAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/BagMan/Characters/Cosmetics/Stickers/T_BagMan_StickerAtlas.T_BagMan_StickerAtlas")));

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void BuildMural();

	UPROPERTY(VisibleAnywhere, Category = "AFL|Retail")
	TObjectPtr<UStaticMeshComponent> WallMesh;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AAFLDisplayPedestal>> Pads;
};
